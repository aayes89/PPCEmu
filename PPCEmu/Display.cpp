#include "Display.h"
#include "MMU.h"
#include "Log.h"
#include <cstring>
#include <iostream>
#include <shellscalingapi.h>

static const wchar_t* WC_NAME = L"EmuFrameWnd";

Display::Display(const std::string& name, uint64_t baseAddress, int width, int height)
    : MemoryDevice(name), base_(baseAddress), w_(width), h_(height) {
    pixels_.resize(width * height * 4, 0); // RGB, inicializado a 0
    ZeroMemory(&bmi_, sizeof(bmi_));
    bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi_.bmiHeader.biWidth = width;
    bmi_.bmiHeader.biHeight = -height; // top-down
    bmi_.bmiHeader.biPlanes = 1;
    bmi_.bmiHeader.biBitCount = 32; //RGB32
    bmi_.bmiHeader.biCompression = BI_RGB;

    createWindow();
    std::cout << "Display initialized: " << name << " at 0x" << std::hex << baseAddress
        << ", pixels_.size(): 0x" << pixels_.size() << std::endl;
}

Display::~Display() {
    if (hdcMem_) DeleteDC(hdcMem_);
    if (hBmp_) DeleteObject(hBmp_);
    if (hwnd_) DestroyWindow(hwnd_);
}

void Display::createWindow() {
    //WNDCLASSA wc = {};
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    //wc.lpszClassName = WC_NAME;
    wc.lpszClassName = WC_NAME;
    //RegisterClassA(&wc);
    RegisterClassW(&wc);    

    int title_len = MultiByteToWideChar(CP_UTF8, 0, name_.c_str(), -1, nullptr, 0);
    std::wstring wtitle(title_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, name_.c_str(), -1, &wtitle[0], title_len);

    hwnd_ = CreateWindowExW(//CreateWindowExA(
        0, WC_NAME, wtitle.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, w_ + 16, h_ + 39,
        nullptr, nullptr, GetModuleHandle(nullptr), this);
    if (!hwnd_) {
        std::cerr << "Error: No se pudo crear la ventana, GetLastError: " << GetLastError() << std::endl;
        return;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    HDC hdcWindow = GetDC(hwnd_);
    hdcMem_ = CreateCompatibleDC(hdcWindow);
    hBmp_ = CreateCompatibleBitmap(hdcWindow, w_, h_);
    SelectObject(hdcMem_, hBmp_);
    ReleaseDC(hwnd_, hdcWindow);

    if (!hdcMem_ || !hBmp_) {
        std::cerr << "Error: Failed to create DC or bitmap" << std::endl;
    }
}

bool Display::ProcessMessages() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

void Display::Present() {
    uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());
    LOG_INFO("Display", "Pixel[0]=0x%08X, Pixel[0x1F4]=0x%08X",
        ((uint32_t*)pixels_.data())[0],
        ((uint32_t*)pixels_.data())[0x1F4 / 4]);


    if (!SetDIBits(hdcMem_, hBmp_, 0, h_, pixels_.data(), &bmi_, DIB_RGB_COLORS)) {
        std::cerr << "Error: SetDIBits failed, GetLastError: " << GetLastError() << std::endl;
    }

    HDC hdcWindow = GetDC(hwnd_);
    if (!BitBlt(hdcWindow, 0, 0, w_, h_, hdcMem_, 0, 0, SRCCOPY)) {
        std::cerr << "Error: BitBlt failed, GetLastError: " << GetLastError() << std::endl;
    }
    ReleaseDC(hwnd_, hdcWindow);

    InvalidateRect(hwnd_, nullptr, TRUE);
    UpdateWindow(hwnd_);
}

uint8_t Display::Read8(uint64_t a) { uint8_t v; Read(a, &v, 1); return v; }
uint16_t Display::Read16(uint64_t a) { uint16_t v; Read(a, &v, 2); return v; }
uint32_t Display::Read32(uint64_t a) { uint32_t v; Read(a, &v, 4); return v; }
uint64_t Display::Read64(uint64_t a) { uint64_t v; Read(a, &v, 8); return v; }

void Display::Write8(uint64_t a, uint8_t v) { Write(a, &v, 1); }
void Display::Write16(uint64_t a, uint16_t v) { Write(a, &v, 2); }
void Display::Write32(uint64_t a, uint32_t v) {
    if (a < 0xC0000000ULL || a >= 0xC012C000ULL) {
        std::cout << "Display Write32: 0x" << std::hex << a << " = 0x" << v << std::endl;
    }
    Write(a, &v, 4);
}
void Display::Write64(uint64_t a, uint64_t v) {
    if (a < 0xC0000000ULL || a >= 0xC012C000ULL) {
        std::cout << "Display Write64: 0x" << std::hex << a << " = 0x" << v << std::endl;
    }
    Write(a, &v, 8);
}

void Display::Read(uint64_t address, void* buffer, size_t size) {
    auto offset = address - base_;
    if (offset + size > pixels_.size()) {
        std::cerr << "Read out of bounds: 0x" << std::hex << address << ", size: " << size << std::endl;
        return;
    }
    memcpy(buffer, pixels_.data() + offset, size);
}

void Display::Write(uint64_t address, const void* buffer, size_t size) {
    auto offset = address - base_;
    if (offset + size > pixels_.size() || offset > pixels_.size()) {
        std::cerr << "Write out of bounds: 0x" << std::hex << address << ", size: " << size
            << ", offset: 0x" << offset << ", pixels_.size(): 0x" << pixels_.size() << std::endl;
        return;
    }
    if (address >= 0xC0000000ULL && address < 0xC012C000ULL) {
        try {
            memcpy(pixels_.data() + offset, buffer, size);
        }
        catch (const std::exception& e) {
            std::cerr << "Error en memcpy: " << e.what() << std::endl;
            throw;
        }
    }
    else {
        memcpy(pixels_.data() + offset, buffer, size);
    }
}

void Display::MemSet(uint64_t address, uint8_t value, size_t size) {
    auto offset = address - base_;
    if (offset + size > pixels_.size()) {
        std::cerr << "MemSet out of bounds: 0x" << std::hex << address << ", size: " << size << std::endl;
        return;
    }
    memset(pixels_.data() + offset, value, size);
}

uint8_t* Display::GetPointerToAddress(uint64_t address) {
    // La dirección ya es un desplazamiento relativo al búfer (ajustado por MMU)
    if (address >= pixels_.size()) {
        std::cerr << "GetPointerToAddress out of bounds: 0x" << std::hex << address << std::endl;
        return nullptr;
    }
    return pixels_.data() + address;
}

LRESULT CALLBACK Display::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Display::BlitChar(int x, int y, char c, uint32_t color) {
    if (c < 0x20 || c > 0x7E) return;
    const uint8_t* glyph = font8x8_basic[c - 0x20];

    uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());

    for (int row = 0; row < 8; ++row) {
        if (y + row >= h_) continue;
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (x + col >= w_) continue;
            if (bits & (1 << col)) {
                pixels[(y + row) * w_ + (x + col)] = color;
            }
        }
    }
}
void Display::BlitText(int x, int y, const std::string& text, uint32_t color) {
    int cursor_x = x;
    for (char c : text) {
        if (cursor_x + 8 > w_) {
            cursor_x = x;
            y += 8;
            if (y + 8 > h_) break;
        }
        BlitChar(cursor_x, y, c, color);
        cursor_x += 8;
    }
}

void Display::Clear(uint32_t color) {
    uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());
    std::fill(pixels, pixels + (w_ * h_), color);
}

void Display::DrawRect(int x, int y, int w, int h, uint32_t color) {
    DrawLine(x, y, x + w, y, color);
    DrawLine(x, y, x, y + h, color);
    DrawLine(x + w, y, x + w, y + h, color);
    DrawLine(x, y + h, x + w, y + h, color);
}
void Display::FillRect(int x, int y, int w, int h, uint32_t color) {
    uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());
    for (int j = 0; j < h; ++j) {
        if (y + j >= h_) continue;
        for (int i = 0; i < w; ++i) {
            if (x + i >= w_) continue;
            pixels[(y + j) * w_ + (x + i)] = color;
        }
    }
}
void Display::DrawLine(int x0, int y0, int x1, int y1, uint32_t color) {
    uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < w_ && y0 >= 0 && y0 < h_)
            pixels[y0 * w_ + x0] = color;

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}
void Display::DrawCircle(int cx, int cy, int r, uint32_t color) {
    int x = r, y = 0;
    int decision = 1 - r;
    uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());

    while (y <= x) {
        auto plot = [&](int px, int py) {
            if (px >= 0 && px < w_ && py >= 0 && py < h_)
                pixels[py * w_ + px] = color;
            };

        plot(cx + x, cy + y);
        plot(cx + y, cy + x);
        plot(cx - y, cy + x);
        plot(cx - x, cy + y);
        plot(cx - x, cy - y);
        plot(cx - y, cy - x);
        plot(cx + y, cy - x);
        plot(cx + x, cy - y);

        y++;
        if (decision <= 0) {
            decision += 2 * y + 1;
        }
        else {
            x--;
            decision += 2 * (y - x) + 1;
        }
    }
}
void Display::FillCircle(int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < w_ && py >= 0 && py < h_)
                    reinterpret_cast<uint32_t*>(pixels_.data())[py * w_ + px] = color;
            }
        }
    }
}
void Display::DrawSquare(int x, int y, int size, uint32_t color) {
    DrawRect(x, y, size, size, color);
}
void Display::FillSquare(int x, int y, int size, uint32_t color) {
    FillRect(x, y, size, size, color);
}

void Display::FillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    auto swap = [](int& a, int& b) { int t = a; a = b; b = t; };

    // Ordenar por Y
    if (y2 < y1) { swap(y1, y2); swap(x1, x2); }
    if (y3 < y1) { swap(y1, y3); swap(x1, x3); }
    if (y3 < y2) { swap(y2, y3); swap(x2, x3); }

    auto drawHLine = [&](int x_start, int x_end, int y) {
        if (y < 0 || y >= h_) return;
        if (x_start > x_end) swap(x_start, x_end);
        for (int x = x_start; x <= x_end; ++x)
            if (x >= 0 && x < w_)
                reinterpret_cast<uint32_t*>(pixels_.data())[y * w_ + x] = color;
        };

    // Compute inverse slopes
    float dx1 = (y2 - y1) > 0 ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) > 0 ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) > 0 ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx = x1, ex = x1;

    // Parte superior
    for (int y = y1; y < y2; ++y) {
        drawHLine(sx, ex, y);
        sx += dx1;
        ex += dx2;
    }

    // Parte inferior
    sx = x2;
    for (int y = y2; y <= y3; ++y) {
        drawHLine(sx, ex, y);
        sx += dx3;
        ex += dx2;
    }
}
