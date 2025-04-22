/* Made by Slam */
/*
Framebuufer Monitor {test only - not used [yet???]}
*/
#pragma once
// framebuffer_monitor.h
#pragma once
#include <cstdint>
#include <iostream>
#include "MMU.h"
#include "Display.h"

class FramebufferMonitor {
public:
    FramebufferMonitor(MMU& mmu)
        : mmu_(mmu) {
    }

    void CheckWrite(uint64_t addr, uint32_t val, uint64_t pc) {
        // Si está en rango framebuffer detectado dinámicamente
        if (addr >= 0x8000020000000000ULL && addr < 0x80000200FFFFFFFFULL) {
            std::cout << "[FB WRITE] PC=0x" << std::hex << pc << " Addr=0x"
                << addr << " Val=0x" << val << std::endl;

            // Si no está mapeado, mapear on-the-fly
            if (!mmu_.IsMapped(addr)) {
                auto fb = std::make_shared<Display>("DYN_FB", addr, 640, 480);
                mmu_.MapMemory(fb, addr, addr + (640 * 480 * 4), 0, true, true, false);
                std::cout << "[MMU] Framebuffer mapeado dinámicamente en 0x" << std::hex << addr << std::endl;
            }
        }
    }

private:
    MMU& mmu_;
};
