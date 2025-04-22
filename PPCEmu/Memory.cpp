/* Made by Slam */
/*
Virtual RAM
*/
#include "Memory.h"
#include "Log.h"
#include <cstring>

Memory::Memory(const std::string& name) : MemoryDevice(name) {
    data_.resize(512 * 1024 * 1024, 0); // 512 MB RAM
    LOG_INFO("Memory", "[%s] initialized: %zu bytes.", name.c_str(), data_.size());
}

void Memory::Read(uint64_t address, void* data, size_t size) {
    if (address + size > data_.size()) {
        throw std::out_of_range("Memory read out of bounds");
    }
    memcpy(data, data_.data() + address, size);
}

void Memory::Write(uint64_t address, const void* data, size_t size) {
    if (address + size > data_.size()) {
        throw std::out_of_range("Memory write out of bounds");
    }
    memcpy(data_.data() + address, data, size);
}

void Memory::MemSet(uint64_t address, uint8_t value, size_t size) {
    if (address + size > data_.size()) {
        throw std::out_of_range("Memory memset out of bounds");
    }
    memset(data_.data() + address, value, size);
}

uint8_t* Memory::GetPointerToAddress(uint64_t address) {
    if (address >= data_.size()) {
        throw std::out_of_range("Memory address out of bounds");
    }
    return data_.data() + address;
}

uint32_t Memory::Read32(uint64_t address) {
    if (address + 4 > data_.size()) {
        throw std::out_of_range("Memory read32 out of bounds");
    }
    uint32_t value;
    memcpy(&value, data_.data() + address, 4);
    return value;
}

void Memory::Write32(uint64_t address, uint32_t value) {
    if (address + 4 > data_.size()) {
        throw std::out_of_range("Memory write32 out of bounds");
    }
    memcpy(data_.data() + address, &value, 4);
}

uint64_t Memory::Read64(uint64_t address) {
    if (address + 8 > data_.size()) {
        throw std::out_of_range("Memory read64 out of bounds");
    }
    uint64_t value;
    memcpy(&value, data_.data() + address, 8);
    return value;
}

void Memory::Write64(uint64_t address, uint64_t value) {
    if (address + 8 > data_.size()) {
        throw std::out_of_range("Memory write64 out of bounds");
    }
    memcpy(data_.data() + address, &value, 8);
}
/*
u64 Memory::GetOffset(u64 address) const {
    if (address >= XBOX360_RAM_SIZE) {
        LOG_CRITICAL("System", "Invalid memory access at 0x%016llX!", address);
        throw std::out_of_range("Invalid memory address");
    }
    return address;
}

void Memory::CheckAlignment(u64 address, u64 alignment) const {
    if (address % alignment != 0) {
        LOG_CRITICAL("System", "Unaligned access at 0x%016llX (alignment: %llu)!", address, alignment);
        throw std::runtime_error("Unaligned memory access");
    }
}

u16 Memory::Swap16(u16 value) const {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

u32 Memory::Swap32(u32 value) const {
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
        ((value >> 8) & 0xFF00) | ((value >> 24) & 0xFF);
}

u64 Memory::Swap64(u64 value) const {
    return ((value & 0xFF) << 56) | ((value & 0xFF00) << 40) |
        ((value & 0xFF0000) << 24) | ((value & 0xFF000000) << 8) |
        ((value >> 8) & 0xFF000000) | ((value >> 24) & 0xFF0000) |
        ((value >> 40) & 0xFF00) | ((value >> 56) & 0xFF);
}*/
