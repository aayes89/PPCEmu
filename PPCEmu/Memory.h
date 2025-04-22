/* Made by Slam */
#ifndef Memory_H
#define Memory_H
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include "MemoryDevice.h"

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s32 = int32_t;

constexpr u64 XBOX360_RAM_SIZE = 512 * 1024 * 1024; // not used

class Memory : public MemoryDevice {
public:
    Memory(const std::string& name);
    void Read(uint64_t address, void* data, size_t size) override;
    void Write(uint64_t address, const void* data, size_t size) override;
    void MemSet(uint64_t address, uint8_t value, size_t size) override;
    uint8_t* GetPointerToAddress(uint64_t address) override;
    uint32_t Read32(uint64_t address) override;
    void Write32(uint64_t address, uint32_t value) override;
    uint64_t Read64(uint64_t address) override;
    void Write64(uint64_t address, uint64_t value) override;

private:
    std::vector<uint8_t> data_;
};
#endif
