#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <filesystem>
#include "MMU.h"


#pragma pack(push, 1)
struct XEXHeader {
	char magic[4]; // "XEX2"
	uint32_t module_flags;
	uint32_t code_offset;
	uint32_t reserved;
	uint32_t cert_offset;
	uint32_t section_count;
	// Seguido por section_count entradas de sección
};

struct XEXSection {
	uint32_t virtual_address;
	uint32_t virtual_size;
	uint32_t file_offset;
	uint32_t file_size;
};
#pragma pack(pop)
void LoadXEX(const std::string& filename, MMU& mmu) {
	std::ifstream file(filename, std::ios::binary);
	if (!file) {
		std::cerr << "Error: No se pudo abrir el archivo " << filename << std::endl;
		throw std::runtime_error("No se pudo abrir el XEX");
	}

	// Leer encabezado
	XEXHeader header;
	file.read(reinterpret_cast<char*>(&header), sizeof(XEXHeader));
	if (std::string(header.magic, 4) != "XEX2") {
		std::cerr << "Error: No es un archivo XEX válido" << std::endl;
		throw std::runtime_error("Formato XEX inválido");
	}

	// Leer secciones
	std::vector<XEXSection> sections(header.section_count);
	file.seekg(header.code_offset);
	file.read(reinterpret_cast<char*>(sections.data()), header.section_count * sizeof(XEXSection));

	// Cargar secciones
	for (const auto& section : sections) {
		std::vector<u8> buffer(section.file_size);
		file.seekg(section.file_offset);
		file.read(reinterpret_cast<char*>(buffer.data()), section.file_size);
		mmu.Write(section.virtual_address, buffer.data(), buffer.size());
		std::cout << "Cargada sección en 0x" << std::hex << section.virtual_address
			<< ", tamaño: 0x" << section.virtual_size << std::endl;
	}

	file.close();

	// Log de la primera instrucción en el punto de entrada
	try {
		u32 first_instr = mmu.Read32(sections[0].virtual_address);
		std::cout << "Primera instrucción en 0x" << std::hex << sections[0].virtual_address
			<< ": 0x" << first_instr << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Error al leer la primera instrucción: " << e.what() << std::endl;
	}
}