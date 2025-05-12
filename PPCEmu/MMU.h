// MMU.h
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

struct TLBEntry {
    uint64_t vaddr_base;
    uint64_t vaddr_end;
    MemoryRegion* region;
};

class MMU {
public:
    void MapMemory(std::shared_ptr<MemoryDevice> device, uint64_t virtual_start, uint64_t virtual_end,
        uint64_t physical_start, bool readable, bool writable, bool executable);

    bool Read(uint64_t address, uint8_t* data, uint64_t size);
    void Write(uint64_t address, const uint8_t* data, uint64_t size);
    void MemSet(uint64_t address, uint8_t value, uint64_t size);
    uint8_t* GetPointerToAddress(uint64_t address);

    uint8_t Read8(uint64_t addr);
    uint16_t Read16(uint64_t addr);
    uint32_t Read32(uint64_t addr);
    uint64_t Read64(uint64_t addr);
    uint64_t Read128(uint32_t addr);

    std::vector<uint8_t> ReadBytes(uint64_t address, size_t size);

    void Write8(uint64_t addr, uint8_t val);
    void Write16(uint64_t addr, uint16_t value);
    void Write32(uint64_t addr, uint32_t value);
    void Write64(uint64_t address, uint64_t value);    
    void Write128(uint32_t addr, uint64_t val);

    uint64_t ReadLeft(uint32_t addr);
    uint64_t ReadRight(uint32_t addr);
    void WriteLeft(uint32_t addr, uint64_t val);
    void WriteRight(uint32_t addr, uint64_t val);
    void ClearRegions();

    uint64_t LoadVectorShiftLeft(uint32_t addr);
    uint64_t LoadVectorShiftRight(uint32_t addr);
    // vectores: carga 128-bits desde memoria
    uint64_t LoadVectorRight(uint32_t addr);
    uint64_t LoadVectorLeft(uint32_t addr);   

    size_t GetRegionCount() const { return regions.size(); }
    void SetVerboseLogging(bool verbose) { verbose_logging_ = verbose; } // Nuevo método
    // D-cache & I-cache operations
    void DCACHE_Store(uint32_t addr);
    void DCACHE_Flush(uint32_t addr);
    void DCACHE_CleanInvalidate(uint32_t addr);
    void ICACHE_Invalidate(uint32_t addr);   
    
    void CheckAlignment(uint64_t address, size_t alignment) const;


private:
    MemoryRegion* FindRegion(uint64_t address, bool read, bool write, bool execute);
    //MemoryRegion* FindRegion(u64 address, bool write_access, bool exec_access);
    std::vector<MemoryRegion> regions;
    bool verbose_logging_ = true; // Por defecto, logs activados   

    static constexpr int TLB_SIZE = 16;
    //std::array<TLBEntry, TLB_SIZE> tlb;
    int tlb_next = 0; // índice circular para reemplazo simple
};