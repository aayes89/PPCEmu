// Memory.h
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

static constexpr u64 XBOX360_RAM_SIZE = 512ULL * 1024 * 1024;

class Memory : public MemoryDevice {
public:
    Memory(const std::string& name);
    void Read(uint64_t address, void* data, size_t size) override;
    void Write(uint64_t address, const void* data, size_t size) override;    
    void MemSet(uint64_t address, uint8_t value, size_t size) override;
    uint8_t* GetPointerToAddress(uint64_t address) override;
    uint16_t Read16(uint64_t address);
    uint32_t Read32(uint64_t address) override;
    void Write32(uint64_t address, uint32_t value) override;
    uint64_t Read64(uint64_t address) override;
    void Write64(uint64_t address, uint64_t value) override;    
    void CheckAlignment(uint64_t address, size_t alignment) const;
    uint64_t GetOffset(uint64_t address) const;
    uint16_t Swap16(uint16_t value) const;
    uint32_t Swap32(uint32_t value) const;
    uint64_t Swap64(uint64_t value) const;
    uint64_t GetSize() const override;

private:
    std::vector<uint8_t> data_;
};
#endif