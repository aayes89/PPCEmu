// MemoryDevice.h
#pragma once
#include <cstdint>
#include <string>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s32 = int32_t;

class MemoryDevice {
public:    
    MemoryDevice(const std::string& name) : name_(name) {}
    virtual ~MemoryDevice() = default;

    // Basic byte-level access
    virtual void Read(uint64_t address, void* data, size_t size) = 0;
    virtual void Write(uint64_t address, const void* data, size_t size) = 0;
    virtual void MemSet(uint64_t address, uint8_t value, size_t size) = 0;

    // Typed accessors
    virtual uint8_t Read8(uint64_t address) {
        uint8_t v;
        Read(address, &v, 1);
        return v;
    }
    virtual uint16_t Read16(uint64_t address) {
        uint16_t v;
        Read(address, &v, 2);
        return v;
    }
    virtual uint32_t Read32(uint64_t address) = 0;
    virtual uint64_t Read64(uint64_t address) = 0;

    virtual void Write8(uint64_t address, uint8_t value) {
        Write(address, &value, 1);
    }
    virtual void Write16(uint64_t address, uint16_t value) {
        Write(address, &value, 2);
    }
    virtual void Write32(uint64_t address, uint32_t value) = 0;
    virtual void Write64(uint64_t address, uint64_t value) = 0;

    // Direct pointer access (for optimization/MMIO mapping)
    virtual uint8_t* GetPointerToAddress(uint64_t address) = 0;

    // Total size of the device's memory    
    virtual uint64_t GetSize() const = 0;

    const std::string& GetName() const { return name_; }

protected:
    std::string name_;
};
