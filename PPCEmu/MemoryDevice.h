#pragma once
#include "Tipos.h"


class MemoryDevice {
public:
    MemoryDevice(const std::string& name) : name_(name) {}
    virtual ~MemoryDevice() = default;

    virtual void Read(uint64_t address, void* data, size_t size) = 0;
    virtual void Write(uint64_t address, const void* data, size_t size) = 0;
    virtual void MemSet(uint64_t address, uint8_t value, size_t size) = 0;
    virtual uint8_t* GetPointerToAddress(uint64_t address) = 0;
    virtual uint32_t Read32(uint64_t address) = 0;
    virtual void Write8(uint64_t addr, uint8_t val) {
        Write(addr, &val, 1);
    }
    virtual void Write32(uint64_t address, uint32_t value) = 0;
    virtual uint64_t Read64(uint64_t address) = 0;
    virtual void Write64(uint64_t address, uint64_t value) = 0;

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

    virtual void Write16(uint64_t address, uint16_t value) {
        Write(address, &value, 2);
    }

    const std::string& GetName() const { return name_; }

protected:
    std::string name_;
};