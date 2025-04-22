#include <iostream>
#include <fstream>
#include <thread>
#include <windows.h>
#include "CPU.h"
#include "Memory.h"
#include "Display.h"
#include "MMU.h"
#include "Log.h"
#include "Tipos.h"

// Configuración
constexpr uint64_t RAM_BASE = 0x00000000ULL;
constexpr uint64_t RAM_SIZE = 0x20000000ULL;
constexpr uint64_t RAM_EXTRA_BASE = 0x20000000ULL;
constexpr uint64_t RAM_EXTRA_SIZE = 0x10000000ULL;
constexpr uint64_t OCRAM_BASE = 0x8000020000000000ULL;
constexpr uint64_t OCRAM_SIZE = 0x000000FFFFFFFFFFULL;
constexpr uint64_t FB_BASE = 0xC0000000ULL;
constexpr uint64_t FB_SIZE = 640 * 480 * 4;
constexpr uint64_t XELL_LOAD_ADDR = 0x00000000ULL;

void LoadBinary(const std::string& filename, MMU& mmu) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("No se pudo abrir el binario");
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), {});
    mmu.Write(XELL_LOAD_ADDR, buffer.data(), buffer.size());
    std::cout << "Cargado " << buffer.size() << " bytes en 0x" << std::hex << XELL_LOAD_ADDR << std::endl;
}


int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    auto ram = std::make_shared<Memory>("RAM");
    auto framebuffer = std::make_shared<Display>("XELL-FB", FB_BASE, 640, 480);
    MMU mmu;    

    mmu.MapMemory(ram, RAM_BASE, RAM_BASE + RAM_SIZE, 0, true, true, true);
    mmu.MapMemory(ram, RAM_EXTRA_BASE, RAM_EXTRA_BASE + RAM_EXTRA_SIZE, 0, true, true, true);
    mmu.MapMemory(ram, OCRAM_BASE, OCRAM_BASE + OCRAM_SIZE, 0, true, true, true);
    mmu.MapMemory(framebuffer, FB_BASE, FB_BASE + FB_SIZE, 0, true, true, false);
    mmu.MapMemory(framebuffer, 0x6A000000ULL, 0x6A000000ULL + (640 * 480 * 4), 0, true, true, false);

    std::cout << "Memoria RAM y Framebuffer mapeados" << std::endl;        
    
    framebuffer->Clear();
    framebuffer->BlitText(50, 10, "Prueba de Primitivas en FB con GDI", 0xFFFFFF);
    framebuffer->BlitText(50, 40, "--------------------------------------", 0x00FF00);
    framebuffer->BlitText(50, 50, "| XeLL RELOADED - Xenon Linux Loader |", 0x00FF00);
    framebuffer->BlitText(50, 60, "--------------------------------------", 0x00FF00);
    framebuffer->BlitText(50, 100, "* Looking for bootloader file ...", 0xFFFF00);
    framebuffer->DrawRect(40, 30, 320, 50, 0xFF0000);
    framebuffer->FillCircle(320, 240, 50, 0x0000FF);
    framebuffer->FillSquare(320, 320, 80, 0x00FF00);
    // triangulo eq
    int centerX = 420;
    int centerY = 240;
    int size = 50;
    int x1 = centerX;
    int y1 = centerY - size;
    int x2 = centerX - size;
    int y2 = centerY + size;
    int x3 = centerX + size;
    int y3 = centerY + size;
    framebuffer->FillTriangle(x1, y1, x2, y2, x3, y3, 0xFF0000);   
    
    

    
    

    //BlitChar(mmu, FB_BASE, 640, 480, 60, 50, 'B', 0x00FF00);
    //BlitChar(mmu, FB_BASE, 640, 480, 70, 50, 'C', 0x0000FF);
    //BlitChar(mmu, FB_BASE, 640, 480, 80, 50, 'D', 0xFFFFFF);

    try {
        LoadBinary("./kernel/lk.bin", mmu);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    CPU cpu(&mmu);
    cpu.SetGPR(1, 0x10000);
    cpu.SetMSR(0x00000000);
    cpu.Reset();

    std::cout << "CPU inicializada" << std::endl;

    while (framebuffer->ProcessMessages()) {
        cpu.Step();
        framebuffer->Present();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "Emulación terminada" << std::endl;
    return 0;
}
