#pragma once
#include "MemoryDevice.h"
#include <vector>
#include <memory>

struct MemoryRegion {
    std::shared_ptr<MemoryDevice> device;
    uint64_t virtual_start;
    uint64_t virtual_end;
    uint64_t physical_start;
    bool readable;
    bool writable;
    bool executable;
};

class MMU {
public:
    void MapMemory(std::shared_ptr<MemoryDevice> device, uint64_t virtual_start, uint64_t virtual_end,
        uint64_t physical_start, bool readable, bool writable, bool executable);
    
    void Read(uint64_t address, uint8_t* data, uint64_t size);
    void Write(uint64_t address, const uint8_t* data, uint64_t size);
    void MemSet(uint64_t address, int32_t value, uint64_t size);
    uint8_t* GetPointerToAddress(uint64_t address);
    
    uint8_t  Read8(uint32_t addr) {
        uint8_t b;
        Read(addr, &b, 1);
        return b;
    }
    uint16_t Read16(uint32_t addr) {
        uint8_t buf[2];
        Read(addr, buf, 2);
        return uint16_t(buf[0]) << 8 | uint16_t(buf[1]);
    }
    
    uint32_t Read32(uint32_t addr) {
        uint8_t buf[4];
        Read(addr, buf, 4);
        return uint32_t(buf[0]) << 24
            | uint32_t(buf[1]) << 16
            | uint32_t(buf[2]) << 8
            | uint32_t(buf[3]) << 0;
    }
    uint64_t Read64(uint64_t address);

    void Write8(uint64_t addr, uint8_t val);
    void Write16(uint32_t addr, uint16_t value);
    void Write32(uint64_t address, uint32_t value);    
    void Write64(uint64_t address, uint64_t value);

    size_t GetRegionCount() const { return regions.size(); }    
    void SetVerboseLogging(bool verbose) { verbose_logging_ = verbose; } // Nuevo método

private:
    MemoryRegion* FindRegion(uint64_t address, bool read, bool write, bool execute);
    std::vector<MemoryRegion> regions;
    bool verbose_logging_ = true; // Por defecto, logs activados
};