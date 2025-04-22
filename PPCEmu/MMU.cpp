#include "MMU.h"
#include "Log.h"
#include <stdexcept>

void MMU::MapMemory(std::shared_ptr<MemoryDevice> device, uint64_t virtual_start, uint64_t virtual_end, uint64_t physical_start, bool readable, bool writable, bool executable) {
    MemoryRegion region;
    region.device = device;
    region.virtual_start = virtual_start;
    region.virtual_end = virtual_end;
    region.physical_start = physical_start;
    region.readable = readable;
    region.writable = writable;
    region.executable = executable;
    regions.push_back(region);
    LOG_INFO("MMU", "Mapped region 0x%I64X-0x%I64X to %s", virtual_start, virtual_end, device->GetName().c_str());
}

MemoryRegion* MMU::FindRegion(uint64_t address, bool read, bool write, bool execute) {
    if (verbose_logging_) {
        std::cout << "FindRegion: address=0x" << std::hex << address << ", read=" << read << ", write=" << write << ", execute=" << execute << std::endl;
    }
    for (auto& region : regions) {
        if (verbose_logging_) {
            std::cout << "Checking region: 0x" << std::hex << region.virtual_start << "-0x" << region.virtual_end
                << ", readable=" << region.readable << ", writable=" << region.writable << ", executable=" << region.executable
                << ", device=" << region.device->GetName() << std::endl;
            std::cout << "Condition: address(0x" << std::hex << address << ") >= virtual_start(0x" << region.virtual_start << "): "
                << (address >= region.virtual_start) << ", address < virtual_end(0x" << region.virtual_end << "): "
                << (address < region.virtual_end) << std::endl;
        }
        if (address >= region.virtual_start && address < region.virtual_end) {
            if ((read && !region.readable) || (write && !region.writable) || (execute && !region.executable)) {
                LOG_CRITICAL("MMU", "Permission denied at 0x%016llX (read=%d, write=%d, execute=%d)", address, read, write, execute);
                throw std::runtime_error("Memory access permission denied");
            }
            if (verbose_logging_) {
                std::cout << "Region found for address 0x" << std::hex << address << std::endl;
            }
            return &region;
        }
    }
    LOG_CRITICAL("MMU", "Invalid memory access at 0x%016llX!", address);
    throw std::out_of_range("Accessing unmapped memory address");
}

void MMU::Read(uint64_t address, uint8_t* data, uint64_t size) {
    MemoryRegion* region = FindRegion(address, true, false, false);
    if (address + size > region->virtual_end) {
        throw std::out_of_range("Read out of bounds");
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    region->device->Read(physical_address, data, size);
}

void MMU::Write(uint64_t address, const uint8_t* data, uint64_t size) {
    MemoryRegion* region = FindRegion(address, false, true, false);
    if (address + size > region->virtual_end) {
        throw std::out_of_range("Write out of bounds");
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    region->device->Write(physical_address, data, size);
}

void MMU::MemSet(uint64_t address, int32_t value, uint64_t size) {
    MemoryRegion* region = FindRegion(address, false, true, false);
    if (address + size > region->virtual_end) {
        throw std::out_of_range("MemSet out of bounds");
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    region->device->MemSet(physical_address, value, size);
}

uint8_t* MMU::GetPointerToAddress(uint64_t address) {
    MemoryRegion* region = FindRegion(address, true, false, false);
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    return static_cast<uint8_t*>(region->device->GetPointerToAddress(physical_address));
}


/*uint16_t MMU::Read16(uint32_t addr) {
    uint8_t buf[2];
    Read(addr, buf, 2);                              // existing byte‐array reader
    return (uint16_t(buf[0]) << 8) | uint16_t(buf[1]);
}*/
void MMU::Write8(uint64_t addr, uint8_t val) {
    auto region = FindRegion(addr, /*read=*/false, /*write=*/true, /*execute=*/false);
    if (!region) {
        std::cerr << "[CRITICAL] Invalid memory write8 at 0x" << std::hex << addr << std::endl;
        throw std::runtime_error("Invalid memory access (Write8)");
    }
    region->device->Write8(addr - region->virtual_start, val);
}

void MMU::Write16(uint32_t addr, uint16_t value) {
    uint8_t buf[2] = { uint8_t(value >> 8), uint8_t(value) };
    Write(addr, buf, 2);                             // existing byte‐array writer
}

/*uint32_t MMU::Read32(uint64_t address) {
    MemoryRegion* region = FindRegion(address, true, false, false);
    if (address + 4 > region->virtual_end) {
        throw std::out_of_range("Read32 out of bounds");
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    return region->device->Read32(physical_address);
}*/

void MMU::Write32(uint64_t address, uint32_t value) {
    MemoryRegion* region = FindRegion(address, false, true, false);
    if (address + 4 > region->virtual_end) {
        throw std::out_of_range("Write32 out of bounds");
    }
    /* Hook para ver donde escribe en el FB*/
    if ((address >= 0xC0000000ULL && address < 0xC0000000ULL + (640 * 480 * 4)) ||
        (address >= 0x80000200EA000000ULL) ||
        (address >= 0xEC800000ULL && address < 0xEC800000ULL + (640 * 480 * 4))) {
        std::cout << "[FRAMEBUFFER WRITE] Addr=0x" << std::hex << address
            << " Value=0x" << value << std::endl;
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    if (region->device->GetName() == "XELL-FB") {
        printf("[FB Write32] addr=0x%08llX value=0x%08X\n", address, value);
    }
    region->device->Write32(physical_address, value);
}

uint64_t MMU::Read64(uint64_t address) {
    MemoryRegion* region = FindRegion(address, true, false, false);
    if (address + 8 > region->virtual_end) {
        throw std::out_of_range("Read64 out of bounds");
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    return region->device->Read64(physical_address);
}

void MMU::Write64(uint64_t address, uint64_t value) {
    MemoryRegion* region = FindRegion(address, false, true, false);
    if (address + 8 > region->virtual_end) {
        throw std::out_of_range("Write64 out of bounds");
    }
    uint64_t physical_address = address - region->virtual_start + region->physical_start;
    if (region->device->GetName() == "ConsoleFB") {
        printf("[FB Write64] addr=0x%08llX value=0x%016llX\n", address, value);
    }
    region->device->Write64(physical_address, value);
}

/*void MMU::SetVerboseLogging(bool verbose) {
    verbose_logging_ = verbose;
}*/