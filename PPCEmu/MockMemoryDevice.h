// MockMemoryDevice.h
#pragma once
#include "MemoryDevice.h"
#include <vector>
#include <cstring>
#include <stdexcept>

class MockMemoryDevice : public MemoryDevice {
public:
    explicit MockMemoryDevice(const std::string& name, size_t size)
        : MemoryDevice(name), memory(size, 0) {
    }

    void Read(uint64_t address, void* data, size_t size) override {
        CheckBounds(address, size);
        std::memcpy(data, &memory[address], size);
    }

    void Write(uint64_t address, const void* data, size_t size) override {
        CheckBounds(address, size);
        std::memcpy(&memory[address], data, size);
    }

    void MemSet(uint64_t address, uint8_t value, size_t size) override {
        CheckBounds(address, size);
        std::memset(&memory[address], value, size);
    }

    uint32_t Read32(uint64_t address) override {
        uint32_t value;
        Read(address, &value, sizeof(value));
        return value;
    }

    uint64_t Read64(uint64_t address) override {
        uint64_t value;
        Read(address, &value, sizeof(value));
        return value;
    }

    void Write32(uint64_t address, uint32_t value) override {
        Write(address, &value, sizeof(value));
    }

    void Write64(uint64_t address, uint64_t value) override {
        Write(address, &value, sizeof(value));
    }

    uint8_t* GetPointerToAddress(uint64_t address) override {
        CheckBounds(address, 1);
        return &memory[address];
    }

    uint64_t GetSize() const override {
        return memory.size();
    }

private:
    std::vector<uint8_t> memory;

    void CheckBounds(uint64_t address, size_t size) const {
        if (address + size > memory.size()) {
            throw std::out_of_range("Access out of bounds in MockMemoryDevice");
        }
    }
};
