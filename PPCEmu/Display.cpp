// Display.cpp: Supports both RAM-backed and internal framebuffer
#include "Display.h"
#include "MemoryDevice.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <windows.h>

static const wchar_t* WC_NAME = L"EmuFrameWnd";

Display::Display(const std::string& name, uint64_t baseAddress, int width, int height)
	: MemoryDevice(name), pixels_(static_cast<size_t>(width)* height * 4), base_(baseAddress), width_(width), height_(height) {
	initWindow();
	std::cout << "Display initialized: " << name << " at 0x" << std::hex << baseAddress
		<< ", size=0x" << pixels_.size() << std::dec << std::endl;
}

Display::Display(const std::string& name, std::shared_ptr<MemoryDevice> ram, uint64_t baseAddress, int width, int height)
	: MemoryDevice(name), ram_(ram), base_(baseAddress), width_(width), height_(height) {
	initWindow();
}

Display::~Display() {
	if (hdcMem_) DeleteDC(hdcMem_);
	if (hBmp_) DeleteObject(hBmp_);
	if (hwnd_) DestroyWindow(hwnd_);
}

void Display::initWindow() {
	ZeroMemory(&bmi_, sizeof(bmi_));
	bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi_.bmiHeader.biWidth = width_;
	bmi_.bmiHeader.biHeight = -height_;
	bmi_.bmiHeader.biPlanes = 1;
	bmi_.bmiHeader.biBitCount = 32;
	bmi_.bmiHeader.biCompression = BI_RGB;

	WNDCLASSW wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = WC_NAME;

	static bool classRegistered = false;
	if (!classRegistered) {
		RegisterClassW(&wc);
		classRegistered = true;
	}

	std::wstring wtitle(name_.begin(), name_.end());
	hwnd_ = CreateWindowExW(0, WC_NAME, wtitle.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width_ + 16, height_ + 39,
		nullptr, nullptr, GetModuleHandle(nullptr), this);
	if (!hwnd_) throw std::runtime_error("Failed to create display window");

	ShowWindow(hwnd_, SW_SHOW);
	HDC hdcWindow = GetDC(hwnd_);
	hdcMem_ = CreateCompatibleDC(hdcWindow);
	hBmp_ = CreateCompatibleBitmap(hdcWindow, width_, height_);
	SelectObject(hdcMem_, hBmp_);
	ReleaseDC(hwnd_, hdcWindow);
}

bool Display::ProcessMessages() {
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) return false;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;
}

void Display::Present() {
	HDC hdcWindow = GetDC(hwnd_);
	const uint8_t* data = ram_ ? ram_->GetPointerToAddress(base_) : pixels_.data();
	if (!data) return;
	SetDIBits(hdcMem_, hBmp_, 0, height_, data, &bmi_, DIB_RGB_COLORS);
	BitBlt(hdcWindow, 0, 0, width_, height_, hdcMem_, 0, 0, SRCCOPY);
	ReleaseDC(hwnd_, hdcWindow);
}

#define FB_ACCESS(method, type, sz, op) \
    type Display::method(uint64_t address) { \
        if (address < base_) throw std::out_of_range(#method " below base"); \
        size_t off = size_t(address - base_); \
        uint8_t* ptr = ram_ ? ram_->GetPointerToAddress(address) : (pixels_.data() + off);\
        if (!ptr || off + sz > width_ * height_ * 4) throw std::out_of_range(#method " out of bounds"); \
        type val; memcpy(&val, ptr + off, sz); return op; \
    }

FB_ACCESS(Read8, uint8_t, 1, val)
FB_ACCESS(Read16, uint16_t, 2, val)
FB_ACCESS(Read32, uint32_t, 4, val)
FB_ACCESS(Read64, uint64_t, 8, val)

#define FB_WRITE(method, type, sz) \
    void Display::method(uint64_t address, type value) { \
        if (address < base_) throw std::out_of_range(#method " below base"); \
        size_t off = size_t(address - base_); \
        uint8_t* ptr = ram_ ? ram_->GetPointerToAddress(address) : (pixels_.data() + off); \
        if (!ptr || off + sz > width_ * height_ * 4) throw std::out_of_range(#method " out of bounds"); \
        memcpy(ptr + off, &value, sz); \
    }

//FB_WRITE(Write8, uint8_t, 1)
//FB_WRITE(Write16, uint16_t, 2)
FB_WRITE(Write32, uint32_t, 4)
FB_WRITE(Write64, uint64_t, 8)

void Display::Write8(uint64_t addr, uint8_t value) {
	if (addr < base_ || addr >= base_ + width_ * height_) {
		std::cerr << "Display write out of bounds: addr=0x" << std::hex << addr
			<< ", base=0x" << base_ << ", limit=0x" << (width_ * height_) << std::dec << "\n";
		throw std::runtime_error("Display: Write out of bounds");
	}
	uint64_t offset = addr - base_;
	std::cout << "Display::Write8: addr=0x" << std::hex << addr << ", offset=0x" << offset
		<< ", value=0x" << (int)value << " ('" << (char)value << "')\n";
	if (textMode_) {
		uint32_t charIndex = offset;
		if (charIndex < width_ * height_) {
			PutChar(value);			
		}
	}
	else {
		pixels_[offset] = value;		
	}
}

void Display::Write16(uint64_t address, uint16_t val) {
	std::cout << "Display::Write16: addr=0x" << std::hex << address << ", val=0x" << val << std::dec << "\n";
	uint8_t bytes[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
	Write(address, bytes, 2);
}

void Display::Read(uint64_t address, void* buffer, size_t size) {
	if (address < base_) throw std::out_of_range("Read buffer below base");
	size_t off = size_t(address - base_);
	uint8_t* ptr = ram_ ? ram_->GetPointerToAddress(base_) : pixels_.data();
	if (!ptr || off + size > width_ * height_ * 4) throw std::out_of_range("Read buffer out of bounds");
	memcpy(buffer, ptr + off, size);
}

void Display::Write(uint64_t address, const void* buffer, size_t size) {
	std::cout << "Display::Write: addr=0x" << std::hex << address << ", size=" << std::dec << size << "\n";
	size_t off = size_t(address - base_);
	if (off + size > width_ * height_ * 4) {
		std::cerr << "Error: Write out of bounds: off=0x" << std::hex << off << ", size=" << std::dec << size
			<< ", max=" << width_ * height_ * 4 << "\n";
		throw std::out_of_range("Write buffer out of bounds");
	}
	uint8_t* ptr = pixels_.data() + off;
	memcpy(ptr, buffer, size);
	if (textMode_) {
		std::cout << "Display::Write: Calling UpdateText with size=" << std::dec << size << "\n";
		std::vector<uint8_t> textData((uint8_t*)buffer, (uint8_t*)buffer + size);
		UpdateText(textData, 0, 0, 0xFFFFFFFF);
		std::cout << "Display::Write: UpdateText completed\n";
	}
}

void Display::MemSet(uint64_t address, uint8_t value, size_t size) {
	if (address < base_) throw std::out_of_range("MemSet below base");
	size_t off = size_t(address - base_);
	uint8_t* ptr = ram_ ? ram_->GetPointerToAddress(address) : (pixels_.data() + off);
	if (!ptr || off + size > width_ * height_ * 4) throw std::out_of_range("MemSet out of bounds");
	memset(ptr + off, value, size);
}

uint8_t* Display::GetPointerToAddress(uint64_t address) {
	if (address < base_) return nullptr;
	size_t off = size_t(address - base_);
	if (off >= width_ * height_ * 4) return nullptr;
	return ram_ ? ram_->GetPointerToAddress(address) : (pixels_.data() + off);
}


uint8_t* Display::GetBuffer() {
	return ram_ ? ram_->GetPointerToAddress(base_) : pixels_.data();
}

LRESULT CALLBACK Display::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE: {
		auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Display::UpdateText(const std::vector<uint8_t>& textData, int x, int y, uint32_t color) {
	std::string text;
	for (uint8_t byte : textData) {
		if (byte == '\0') break; // Para cuando encuentra un terminador nulo
		text += static_cast<char>(byte); // Convierte el byte a carácter
	}
	BlitText(x, y, text, color); // Asume que BlitText ya existe para renderizar texto
}

// Graphic Toolkit
void Display::PutChar(char c) {
	const int CHAR_W = 8, CHAR_H = 8;
	std::cout << "PutChar: '" << c << "' at ("
		<< textCursorX_ << "," << textCursorY_ << ")\n";

	if (c == '\n') {
		textCursorX_ = 0;
		textCursorY_ += CHAR_H;
	}
	else {
		BlitChar(textCursorX_, textCursorY_, c, textColor_);
		textCursorX_ += CHAR_W;
		if (textCursorX_ + CHAR_W > width_) {
			textCursorX_ = 0;
			textCursorY_ += CHAR_H;
		}
	}

	// Si nos pasamos por debajo, scrollear todo hacia arriba 1 línea
	if (textCursorY_ + CHAR_H > height_) {
		ScrollUp(1);
	}
}
void Display::BlitChar(int x, int y, char c, uint32_t color) {
	if (c < 0x20 || c > 0x7E) return;
	const uint8_t* glyph = font8x8_basic[c - 0x20];

	uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());

	for (int row = 0; row < 8; ++row) {
		if (y + row >= height_) continue;
		uint8_t bits = glyph[row];
		for (int col = 0; col < 8; ++col) {
			if (x + col >= width_) continue; 
			if (bits & (1 << col)) {
				pixels[(y + row) * width_ + (x + col)] = color;
			}
		}		
	}
}
void Display::BlitText(int x, int y, const std::string& text, uint32_t color) {
	int cursor_x = x;
	for (char c : text) {
		if (cursor_x + 8 > width_) {
			cursor_x = x;
			y += 8;
			if (y + 8 > height_) break;
		}
		BlitChar(cursor_x, y, c, color);
		cursor_x += 8;
	}
}
void Display::BlitTextFromMemory(uint32_t addr, size_t max_len, int x, int y, uint32_t color) {
	const char* mem = reinterpret_cast<const char*>(addr);
	std::string buffer;

	for (size_t i = 0; i < max_len; ++i) {
		char c = mem[i];
		if (c == '\0') break;
		if (c >= 0x20 && c <= 0x7E) buffer += c;
		else if (c == '\n') buffer += '\n';
	}

	int cursor_x = x;
	int cursor_y = y;

	for (char c : buffer) {
		if (c == '\n') {
			cursor_x = x;
			cursor_y += 8;
			if (cursor_y + 8 > height_) break;
			continue;
		}

		BlitChar(cursor_x, cursor_y, c, color);
		cursor_x += 8;
		if (cursor_x + 8 > width_) {
			cursor_x = x;
			cursor_y += 8;
		}
	}
}
void Display::Clear(uint32_t color) {
	uint32_t* pixels = reinterpret_cast<uint32_t*>(pixels_.data());
	std::fill(pixels, pixels + (width_ * height_), color);
}
void Display::Draw1bppBitmap(uint8_t* src, uint32_t width, uint32_t height, uint32_t fb_base, uint32_t pitch, uint32_t fg_color, uint32_t bg_color) {
	uint32_t* fb = (uint32_t*)fb_base;

	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			uint32_t byte_index = (y * ((width + 7) / 8)) + (x / 8);
			uint8_t byte = src[byte_index];
			uint8_t bit = (byte >> (7 - (x % 8))) & 1;

			uint32_t color = bit ? fg_color : bg_color;
			uint32_t* pixel = (uint32_t*)(fb_base + y * pitch + x * 4);
			*pixel = color;
		}
	}
}
void Display::ScrollUp(int lines) {
	const int offset = lines * width_ * 8 * sizeof(uint32_t);
	std::memmove(pixels_.data(), pixels_.data() + offset, pixels_.size() - offset);
	std::memset(pixels_.data() + pixels_.size() - offset, 0, offset);
	textCursorY_ -= 8 * lines;
	if (textCursorY_ < 0 || textCursorY_ >= height_) textCursorY_ = 0;
}
void Display::ScrollDown(int lines) {
	const int lineHeight = 8;
	const size_t bytesPerLine = static_cast<size_t>(width_) * lineHeight * sizeof(uint32_t);
	size_t offset = lines * bytesPerLine;

	// 1) Mover contenido original hacia abajo
	//    desde inicio del buffer → posición 'offset'
	std::memmove(
		pixels_.data() + offset,
		pixels_.data(),
		pixels_.size() - offset
	);

	// 2) Limpiar la zona superior (espacio liberado)
	std::memset(
		pixels_.data(),
		0,
		offset
	);

	// 3) Ajustar cursor vertical
	textCursorY_ += lineHeight * lines;
	if (textCursorY_ < 0)
		textCursorY_ = 0;
	else if (textCursorY_ + lineHeight > height_)
		textCursorY_ = height_ - lineHeight;
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
		if (y + j >= height_) continue;
		for (int i = 0; i < w; ++i) {
			if (x + i >= width_) continue;
			pixels[(y + j) * width_ + (x + i)] = color;
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
		if (x0 >= 0 && x0 < width_ && y0 >= 0 && y0 < height_)
			pixels[y0 * width_ + x0] = color;

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
			if (px >= 0 && px < width_ && py >= 0 && py < height_)
				pixels[py * width_ + px] = color;
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
				if (px >= 0 && px < width_ && py >= 0 && py < height_)
					reinterpret_cast<uint32_t*>(pixels_.data())[py * width_ + px] = color;
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
		if (y < 0 || y >= height_) return;
		if (x_start > x_end) swap(x_start, x_end);
		for (int x = x_start; x <= x_end; ++x)
			if (x >= 0 && x < width_)
				reinterpret_cast<uint32_t*>(pixels_.data())[y * width_ + x] = color;
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
