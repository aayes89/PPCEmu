// Memory.cpp
#include "Memory.h"
#include "Log.h"
#include <cstring>

Memory::Memory(const std::string& name) : MemoryDevice(name) {
	data_.resize(XBOX360_RAM_SIZE); // 512 MB
	LOG_INFO("Memory", "[%s] initialized: %zu bytes.", name.c_str(), data_.size());
}

void Memory::Read(uint64_t address, void* data, size_t size) {
	if (address + size > data_.size()) {
		std::cout << "Memory read out of bounds: addr=0x" << std::hex << address
			<< ", size=" << std::dec << size << ", limit=" << data_.size() << std::endl;
		throw std::out_of_range("Read: Memory out of bounds");
	}
	memcpy(data, data_.data() + address, size);
	std::cout << "Memory::Read: addr=0x" << std::hex << address << ", size=" << std::dec << size << "\n";
}

void Memory::Write(uint64_t offset, const void* src, uint64_t size) {
	if (offset + size > XBOX360_RAM_SIZE) {
		std::cerr << "Memory write out of bounds: offset=0x" << std::hex << offset
			<< ", size=0x" << size << ", limit=0x" << XBOX360_RAM_SIZE << std::dec << "\n";
		throw std::runtime_error("Memory: Write out of bounds");
	}
	std::cout << "[DEBUG] Memory::Write: offset=0x" << std::hex << offset
		<< ", size=" << std::dec << size << ", endAddr=0x" << std::hex << (offset + size - 1)
		<< ", limit=0x" << XBOX360_RAM_SIZE << std::dec << "\n";
	memcpy(&data_[offset], src, size);
}

void Memory::MemSet(uint64_t address, uint8_t value, size_t size) {
	if (address + size > data_.size()) {
		throw std::out_of_range("MemSet: Memory out of bounds");
	}
	memset(data_.data() + address, value, size);
}

uint8_t* Memory::GetPointerToAddress(uint64_t address) {
	if (address >= data_.size()) {
		std::cerr << "Error: GetPointerToAddress: addr=0x" << std::hex << address << " out of bounds, size=0x" << data_.size() << std::dec << "\n";
		throw std::out_of_range("GetPointerToAddress: Memory address out of bounds");
	}
	return data_.data() + address;
}

uint16_t Memory::Read16(uint64_t address) {
	CheckAlignment(address, 2);
	if (address + 2 > data_.size()) {
		throw std::out_of_range("Read16: Memory out of bounds");
	}
	uint8_t* ptr = data_.data() + address;
	uint16_t value = (ptr[0] << 8) | ptr[1]; // Big-endian
	std::cout << "Memory::Read16: addr=0x" << std::hex << address << ", value=0x" << value << std::dec << "\n";
	return value;
}

uint32_t Memory::Read32(uint64_t address) {
	CheckAlignment(address, 4);
	if (address + 4 > data_.size()) {
		std::cerr << "Error: Read32: addr=0x" << std::hex << address << " out of bounds, size=0x" << data_.size() << std::dec << "\n";
		throw std::out_of_range("Read32: Memory out of bounds");
	}
	uint8_t* ptr = data_.data() + address;
	uint32_t value = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]; // Big-endian
	std::cout << "Memory::Read32: addr=0x" << std::hex << address << ", value=0x" << value << std::dec << "\n";
	return value;
}


uint64_t Memory::Read64(uint64_t address) {
	if (address + 8 > data_.size()) {
		throw std::out_of_range("Read64: Memory out of bounds");
	}
	uint8_t* ptr = data_.data() + address;
	uint64_t value = ((uint64_t)ptr[0] << 56) | ((uint64_t)ptr[1] << 48) | ((uint64_t)ptr[2] << 40) | ((uint64_t)ptr[3] << 32) |
		((uint64_t)ptr[4] << 24) | ((uint64_t)ptr[5] << 16) | ((uint64_t)ptr[6] << 8) | ptr[7]; // Big-endian
	std::cout << "Memory::Read64: addr=0x" << std::hex << address << ", value=0x" << value << std::dec << "\n";
	return value;
}

void Memory::Write32(uint64_t address, uint32_t value) {
	CheckAlignment(address, 4);
	if (address + 4 > data_.size()) {
		throw std::out_of_range("Write32: Memory out of bounds");
	}
	uint8_t* ptr = data_.data() + address;
	ptr[0] = (value >> 24) & 0xFF;
	ptr[1] = (value >> 16) & 0xFF;
	ptr[2] = (value >> 8) & 0xFF;
	ptr[3] = value & 0xFF; // Big-endian
	std::cout << "Memory::Write32: addr=0x" << std::hex << address << ", value=0x" << value << std::dec << "\n";
}

void Memory::Write64(uint64_t address, uint64_t value) {
	if (address + 8 > data_.size()) {
		throw std::out_of_range("Write64: Memory out of bounds");
	}
	uint8_t* ptr = data_.data() + address;
	ptr[0] = (value >> 56) & 0xFF;
	ptr[1] = (value >> 48) & 0xFF;
	ptr[2] = (value >> 40) & 0xFF;
	ptr[3] = (value >> 32) & 0xFF;
	ptr[4] = (value >> 24) & 0xFF;
	ptr[5] = (value >> 16) & 0xFF;
	ptr[6] = (value >> 8) & 0xFF;
	ptr[7] = value & 0xFF; // Big-endian
	std::cout << "Memory::Write64: addr=0x" << std::hex << address << ", value=0x" << value << std::dec << "\n";
}

void Memory::CheckAlignment(uint64_t address, size_t alignment) const {
	if (address % alignment != 0) {
		LOG_CRITICAL("Memory", "Unaligned access at 0x%016llX (alignment: %zu)!", address, alignment);
		throw std::runtime_error("CheckAlignment: Unaligned memory access");
	}
}

uint64_t Memory::GetOffset(uint64_t address) const {
	if (address >= XBOX360_RAM_SIZE) {
		LOG_CRITICAL("System", "Invalid memory access at 0x%016llX!", address);
		throw std::out_of_range("GetOffset: Invalid memory address");
	}
	return address;
}

uint16_t Memory::Swap16(uint16_t value) const {
	return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

uint32_t Memory::Swap32(uint32_t value) const {
	return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
		((value >> 8) & 0xFF00) | ((value >> 24) & 0xFF);
}

uint64_t Memory::Swap64(uint64_t value) const {
	return ((value & 0xFF) << 56) | ((value & 0xFF00) << 40) |
		((value & 0xFF0000) << 24) | ((value & 0xFF000000) << 8) |
		((value >> 8) & 0xFF000000) | ((value >> 24) & 0xFF0000) |
		((value >> 40) & 0xFF00) | ((value >> 56) & 0xFF);
}

uint64_t Memory::GetSize() const {
	return data_.size();
}
