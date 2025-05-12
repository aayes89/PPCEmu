// PPCEmuConfig.h
#pragma once
#include <cstdint>

struct PPCEmuConfig {
    // Memory regions
    uint64_t excBase = 0x00000000ULL;
    uint64_t excSize = 0x00001000ULL;
    uint64_t userBase = 0x80000000ULL;
    uint64_t userSize = 0x20000000ULL;    // 512MB user RAM

    // Framebuffer settings
    uint64_t fbBase = 0xC0000000ULL;
    uint64_t fbSize = 0x004B0000ULL;    // width*height*bytes-per-pixel
    int      fbWidth = 640;
    int      fbHeight = 480;
    bool     textMode = true;
};
