// MMU.cpp
#include "MMU.h"
#include "Log.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

constexpr size_t ALIGN_2 = 2;
constexpr size_t ALIGN_4 = 4;
constexpr size_t ALIGN_8 = 8;

void MMU::MapMemory(std::shared_ptr<MemoryDevice> device,
	uint64_t virtual_start,
	uint64_t virtual_end,
	uint64_t physical_start,
	bool readable,
	bool writable,
	bool executable) {
	MemoryRegion region;
	region.device = device;
	region.virtual_start = virtual_start;
	region.virtual_end = virtual_end;
	region.physical_start = physical_start;
	region.readable = readable;
	region.writable = writable;
	region.executable = executable;
	regions.push_back(region);
	LOG_INFO("MMU", "Mapped region 0x%016llX-0x%016llX to %s, readable=%d, writable=%d, executable=%d",
		virtual_start, virtual_end, device->GetName().c_str(), readable, writable, executable);
	std::cout << "[DEBUG] MMU::MapMemory: Added region 0x" << std::hex << virtual_start
		<< "-0x" << virtual_end << ", physical_start=0x" << physical_start
		<< ", device=" << device->GetName() << ", total regions=" << regions.size() << std::dec << "\n";
}

MemoryRegion* MMU::FindRegion(uint64_t addr, bool read, bool write, bool execute) {
	std::cout << "[DEBUG] MMU::FindRegion: Searching for addr=0x" << std::hex << addr
		<< ", read=" << read << ", write=" << write << ", execute=" << execute << std::dec << "\n";
	for (auto& region : regions) {
		std::cout << "[DEBUG] MMU::FindRegion: Checking region 0x" << std::hex << region.virtual_start
			<< "-0x" << region.virtual_end << ", device=" << region.device->GetName()
			<< ", readable=" << region.readable << ", writable=" << region.writable
			<< ", executable=" << region.executable << std::dec << "\n";
		if (addr >= region.virtual_start && addr < region.virtual_end) {
			if ((read && !region.readable) || (write && !region.writable) || (execute && !region.executable)) {
				std::cerr << "Access denied: addr=0x" << std::hex << addr << ", read=" << read
					<< ", write=" << write << ", execute=" << execute << std::dec << "\n";
				return nullptr;
			}
			std::cout << "[DEBUG] MMU::FindRegion: Found region for addr=0x" << std::hex << addr
				<< ", device=" << region.device->GetName() << std::dec << "\n";
			return &region;
		}
		else {
			std::cout << "[DEBUG] MMU::FindRegion: Address 0x" << std::hex << addr
				<< " not in region 0x" << region.virtual_start << "-0x" << region.virtual_end << std::dec << "\n";
		}
	}
	std::cerr << "[DEBUG] MMU::FindRegion: No region found for addr=0x" << std::hex << addr << std::dec << "\n";
	return nullptr;
}

void MMU::ClearRegions() {
	regions.clear();
}

bool MMU::Read(uint64_t address, uint8_t* data, uint64_t size)
{
	auto* region = FindRegion(address, true, false, false);
	uint64_t offset = address - region->virtual_start + region->physical_start;
	region->device->Read(offset, data, size);
	return true;
}

void MMU::Write(uint64_t addr, const uint8_t* src, uint64_t size) {
	auto region = FindRegion(addr, false, true, false);
	if (!region) {
		std::cerr << "MMU::Write: No region found for addr=0x" << std::hex << addr << std::dec << "\n";
		throw std::runtime_error("MMU: Write to unmapped region");
	}
	uint64_t offset = addr - region->virtual_start + region->physical_start;
	std::cout << "[DEBUG] MMU::Write: addr=0x" << std::hex << addr << ", offset=0x" << offset
		<< ", size=" << std::dec << size << ", device=" << region->device->GetName() << "\n";
	region->device->Write(offset, src, size);
}

void MMU::MemSet(uint64_t address, uint8_t value, uint64_t size)
{
	auto* region = FindRegion(address, false, true, false);
	uint64_t offset = address - region->virtual_start + region->physical_start;
	region->device->MemSet(offset, value, size);
}

uint8_t* MMU::GetPointerToAddress(uint64_t address)
{
	auto* region = FindRegion(address, true, false, false);
	uint64_t offset = address - region->virtual_start + region->physical_start;
	return region->device->GetPointerToAddress(offset);
}

// Accesos de lectura
uint8_t MMU::Read8(uint64_t addr)
{
	auto* region = FindRegion(addr, true, false, false);
	return region->device->Read8(addr - region->virtual_start + region->physical_start);
}

uint16_t MMU::Read16(uint64_t addr)
{
	CheckAlignment(addr, 2);
	auto* region = FindRegion(addr, true, false, false);
	return region->device->Read16(addr - region->virtual_start + region->physical_start);
}

/*
uint32_t MMU::Read32(uint64_t addr) {
	CheckAlignment(addr, 4);
	auto* region = FindRegion(addr, true, false, false);
	if (!region) {
		std::cerr << "MMU::Read32: Unmapped address 0x" << std::hex << addr << std::dec << "\n";
		throw std::runtime_error("MMU: unmapped address");
	}
	uint64_t offset = addr - region->virtual_start + region->physical_start;
	uint32_t value = region->device->Read32(offset);
	std::cout << "MMU::Read32: addr=0x" << std::hex << addr << ", offset=0x" << offset
		<< ", device=" << region->device->GetName() << ", value=0x" << value << std::dec << "\n";
	return value;
}*/
uint32_t MMU::Read32(uint64_t addr) {
	auto* region = FindRegion(addr, true, false, false);
	if (!region) throw std::runtime_error("MMU: unmapped address");

	uint64_t offset = addr - region->virtual_start + region->physical_start;

	// Si está alineado, podemos delegar directamente
	if ((addr & 0x3) == 0) {
		return region->device->Read32(offset);
	}
	// Si NO está alineado, leemos byte a byte (big-endian)
	uint8_t b0 = region->device->Read8(offset + 0);
	uint8_t b1 = region->device->Read8(offset + 1);
	uint8_t b2 = region->device->Read8(offset + 2);
	uint8_t b3 = region->device->Read8(offset + 3);
	return (uint32_t(b0) << 24)
		| (uint32_t(b1) << 16)
		| (uint32_t(b2) << 8)
		| (uint32_t(b3) << 0);
}

uint64_t MMU::Read64(uint64_t addr)
{
	CheckAlignment(addr, 8);
	auto* region = FindRegion(addr, true, false, false);
	return region->device->Read64(addr - region->virtual_start + region->physical_start);
}

uint64_t MMU::Read128(uint32_t addr)
{
	uint64_t low = Read64(addr);
	uint64_t high = Read64(addr + 8);
	return (high << 64) | low;
}

std::vector<uint8_t> MMU::ReadBytes(uint64_t address, size_t size)
{
	std::vector<uint8_t> buffer(size);
	Read(address, buffer.data(), size);
	return buffer;
}

// Escrituras
void MMU::Write8(uint64_t addr, uint8_t val) {
	auto region = FindRegion(addr, false, true, false);
	if (!region || !region->device) {
		std::cerr << "MMU::Write: No region found for addr=0x" << std::hex << addr << std::dec << "\n";
		throw std::runtime_error("MMU: Write to unmapped region");
	}
	uint64_t offset = addr - region->virtual_start;
	std::cout << "MMU::Write8: addr=0x" << std::hex << addr << ", offset=0x" << offset
		<< ", val=0x" << (int)val << " ('" << (char)val << "'), device=" << region->device->GetName() << "\n";
	std::cout << "MMU::Write8: Calling device->Write8 with addr=0x" << std::hex << addr << std::dec << "\n";
	region->device->Write8(addr, val);
}

void MMU::Write16(uint64_t addr, uint16_t val) {
	auto* region = FindRegion(addr, false, true, false);
	if (!region) {
		std::cerr << "MMU::Write16: Unmapped address 0x" << std::hex << addr << std::dec << "\n";
		throw std::runtime_error("MMU: unmapped address");
	}
	uint64_t offset = addr - region->virtual_start + region->physical_start;
	std::cout << "MMU::Write16: addr=0x" << std::hex << addr << ", offset=0x" << offset
		<< ", val=0x" << val << ", device=" << region->device->GetName() << std::dec << "\n";
	region->device->Write16(offset, val);
}

/*void MMU::Write32(uint64_t addr, uint32_t value)
{
	CheckAlignment(addr, 4);
	auto* region = FindRegion(addr, false, true, false);
	region->device->Write32(addr - region->virtual_start + region->physical_start, value);
}*/
void MMU::Write32(uint64_t addr, uint32_t value) {
	auto* region = FindRegion(addr, false, true, false);
	if (!region) throw std::runtime_error("MMU: unmapped address");

	uint64_t offset = addr - region->virtual_start + region->physical_start;

	// Si está alineado, podemos delegar
	if ((addr & 0x3) == 0) {
		region->device->Write32(offset, value);
		return;
	}
	// Desalineado: escribimos byte a byte (big-endian)
	region->device->Write8(offset + 0, uint8_t(value >> 24));
	region->device->Write8(offset + 1, uint8_t(value >> 16));
	region->device->Write8(offset + 2, uint8_t(value >> 8));
	region->device->Write8(offset + 3, uint8_t(value >> 0));
}

void MMU::Write64(uint64_t addr, uint64_t value)
{
	CheckAlignment(addr, 8);
	auto* region = FindRegion(addr, false, true, false);
	region->device->Write64(addr - region->virtual_start + region->physical_start, value);
}

void MMU::Write128(uint32_t addr, uint64_t val)
{
	Write64(addr, val);        // LSB
	Write64(addr + 8, val);    // MSB (si deseas 128 bits de mismo valor)
}

// SIMD y pseudo SIMD
uint64_t MMU::ReadLeft(uint32_t addr) { return Read64(addr); }
uint64_t MMU::ReadRight(uint32_t addr) { return Read64(addr + 8); }
void MMU::WriteLeft(uint32_t addr, uint64_t val) { Write64(addr, val); }
void MMU::WriteRight(uint32_t addr, uint64_t val) { Write64(addr + 8, val); }

uint64_t MMU::LoadVectorShiftLeft(uint32_t addr)
{
	uint64_t val = Read64(addr);
	return val << 8;
}

uint64_t MMU::LoadVectorShiftRight(uint32_t addr)
{
	uint64_t val = Read64(addr);
	return val >> 8;
}

uint64_t MMU::LoadVectorRight(uint32_t addr) { return Read64(addr + 8); }
uint64_t MMU::LoadVectorLeft(uint32_t addr) { return Read64(addr); }

// Cachés (mock)
void MMU::DCACHE_Store(uint32_t addr) {
	if (verbose_logging_) std::cout << "[DCACHE_Store] addr=0x" << std::hex << addr << std::dec << "\n";
}

void MMU::DCACHE_Flush(uint32_t addr) {
	if (verbose_logging_) std::cout << "[DCACHE_Flush] addr=0x" << std::hex << addr << std::dec << "\n";
}

void MMU::DCACHE_CleanInvalidate(uint32_t addr) {
	if (verbose_logging_) std::cout << "[DCACHE_CleanInvalidate] addr=0x" << std::hex << addr << std::dec << "\n";
}

void MMU::ICACHE_Invalidate(uint32_t addr) {
	if (verbose_logging_) std::cout << "[ICACHE_Invalidate] addr=0x" << std::hex << addr << std::dec << "\n";
}

void MMU::CheckAlignment(uint64_t address, size_t alignment) const
{
	if (address % alignment != 0) {
		throw std::runtime_error("Unaligned access at address: " + std::to_string(address));
	}
}
