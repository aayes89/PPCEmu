// PPCEmu.cpp
#include "PPCEmu.h"
#include "PPCEmuConfig.h"
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <windows.h>
#include <shellscalingapi.h>

// ELF32 header structures
#pragma pack(push,1)
struct Elf32Ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
struct Elf32Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};
#pragma pack(pop)

// Helpers to convert big endian 32/16
static uint32_t be32(uint32_t v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}
static uint16_t be16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

PPCEmu::PPCEmu(const PPCEmuConfig& config)
    : cfg_(config),
    cpu_(&mmu_),
    mmu_(),
    ram_(std::make_shared<Memory>("RAM")),
    fb_(std::make_shared<Display>("ConsoleFB",
        cfg_.fbBase,
        cfg_.fbWidth,
        cfg_.fbHeight))
{
    // DPI awareness and text mode
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    fb_->textMode_ = cfg_.textMode;

    if (!fb_->ProcessMessages())
        throw std::runtime_error("Failed to initialize display");

    std::cout << ">>> FB base is 0x" << std::hex << cfg_.fbBase << std::dec << "\n";

    initMappings();
    initExceptionHandlers();

    // Optional: dump exception vector to verify
    uint8_t buf[8];
    ram_->Read(cfg_.excBase + 0x100, buf, sizeof(buf));
    std::cout << "Exception handler at 0x"
        << std::hex << (cfg_.excBase + 0x100) << ": ";
    for (auto b : buf) std::cout << std::setw(2) << int(b) << " ";
    std::cout << std::dec << "\n";
}
std::vector<uint8_t> PPCEmu::ReadFileToVector(const std::string& path) const {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    return { std::istreambuf_iterator<char>(f), {} };
}
BinaryType PPCEmu::DetectFormat(const std::vector<uint8_t>& data) const {
    if (data.size() >= 4 &&
        data[0] == 0x7F && data[1] == 'E' &&
        data[2] == 'L' && data[3] == 'F')
    {
        bool is64 = data[4] == 2, be = data[5] == 2;
        if (be && !is64) return BinaryType::ELF32_BE;
        if (be && is64)  return BinaryType::ELF64_BE;
    }
    return BinaryType::RAW;
}
void PPCEmu::initMappings() {
    mmu_.ClearRegions();

    // Exception vector region
    mmu_.MapMemory(ram_,
        cfg_.excBase,
        cfg_.excBase + cfg_.excSize,
        0,
        true, true, true);

    // User RAM region (for RAW/ELF load)
    mmu_.MapMemory(ram_,
        cfg_.userBase,
        cfg_.userBase + cfg_.userSize,
        0,
        true, true, true);

    // Framebuffer region
    mmu_.MapMemory(fb_,
        cfg_.fbBase,
        cfg_.fbBase + cfg_.fbSize,
        0,
        true, true, false);
}
void PPCEmu::initExceptionHandlers() {
    uint8_t nop_rfi[8] = { 0x60,0x00,0x00,0x00, 0x4C,0x00,0x00,0x64 };
    constexpr uint64_t off[] = { 0x100, 0x600, 0x700 };
    for (auto o : off)
        mmu_.Write(cfg_.excBase + o, nop_rfi, sizeof(nop_rfi));
}
void PPCEmu::LoadRAW(const std::string& filename, uint64_t loadAddr) {
    auto data = ReadFileToVector(filename);
    std::cout << "AutoLoad RAW: " << filename
        << ", " << data.size() << " bytes\n";

    mmu_.Write(loadAddr, data.data(), data.size());
    // patch r10 → framebuffer base
    uint8_t patch[] = { 0x3D,0x4C,0xC0,0x00, 0x61,0x4A,0x00,0x00 };
    mmu_.Write(loadAddr + 0xC, patch, sizeof(patch));
}
void PPCEmu::AutoLoad(const std::string& path) {
    auto data = ReadFileToVector(path);
    auto fmt = DetectFormat(data);
    uint64_t entry = 0;

    switch (fmt) {
    case BinaryType::RAW:
        LoadRAW(path, cfg_.userBase);
        entry = cfg_.userBase;
        break;
    case BinaryType::ELF32_BE:
        LoadELF32(data);
        entry = cpu_.GetPC();
        break;
    case BinaryType::ELF64_BE:
        LoadELF64(data);
        entry = cpu_.GetPC();
        break;
    default:
        throw std::runtime_error("Unknown binary format");
    }

    cpu_.Reset();
    cpu_.SetPC(entry);
    std::cout << "Entry PC: 0x" << std::hex << entry << std::dec << "\n";
}
void PPCEmu::Run(int fps) {
    const auto frameTime = std::chrono::milliseconds(1000 / fps);
    while (fb_->ProcessMessages()) {
        cpu_.Step();
        fb_->Present();     // refresca lo que se haya pintado por syscalls
        std::this_thread::sleep_for(frameTime);
    }
    std::cout << "Emulation ended." << std::endl;
}

void PPCEmu::LoadELF32(const std::vector<uint8_t>&data) {
    // 1) Cabecera ELF
    if (data.size() < sizeof(Elf32Ehdr))
        throw std::runtime_error("ELF data too small");

    const Elf32Ehdr* hdr = reinterpret_cast<const Elf32Ehdr*>(data.data());
    // Validar que es PowerPC BE
    if (be16(hdr->e_machine) != 20) // EM_PPC == 20
        throw std::runtime_error("Not a PowerPC ELF");

    // 2) Recorremos los headers de programa
    uint16_t phnum = be16(hdr->e_phnum);
    uint32_t phoff = be32(hdr->e_phoff);
    uint16_t phentsize = be16(hdr->e_phentsize);

    for (uint16_t i = 0; i < phnum; ++i) {
        size_t off_hdr = phoff + i * phentsize;
        if (off_hdr + sizeof(Elf32Phdr) > data.size())
            throw std::runtime_error("Program header out of bounds");

        const Elf32Phdr* ph = reinterpret_cast<const Elf32Phdr*>(data.data() + off_hdr);
        if (be32(ph->p_type) != 1) continue; // solo PT_LOAD

        // Campos
        uint32_t vaddr = be32(ph->p_vaddr);
        uint32_t filesz = be32(ph->p_filesz);
        uint32_t memsz = be32(ph->p_memsz);
        uint32_t offset = be32(ph->p_offset);

        // Chequeo vector de datos
        if (offset + filesz > data.size())
            throw std::runtime_error("ELF segment out of bounds");

        // 3) Escribimos bytes del fichero
        mmu_.Write(vaddr, data.data() + offset, filesz);

        // 4) Si hay .bss (memsz > filesz), lo rellenamos a cero
        if (memsz > filesz) {
            mmu_.MemSet(vaddr + filesz, 0, memsz - filesz);
        }
    }

    // 5) Ajustar PC al entry point
    cpu_.SetPC(be32(hdr->e_entry));
}
void PPCEmu::LoadELF64(const std::vector<uint8_t>& data) {
    // ELF64 big-endian loader
    struct Elf64Ehdr { unsigned char e_ident[16]; uint16_t e_type; uint16_t e_machine; uint32_t e_version; uint64_t e_entry; uint64_t e_phoff; uint64_t e_shoff; uint32_t e_flags; uint16_t e_ehsize; uint16_t e_phentsize; uint16_t e_phnum; uint16_t e_shentsize; uint16_t e_shnum; uint16_t e_shstrndx; };
    struct Elf64Phdr { uint32_t p_type; uint32_t p_flags; uint64_t p_offset; uint64_t p_vaddr; uint64_t p_paddr; uint64_t p_filesz; uint64_t p_memsz; uint64_t p_align; };
    auto* eh = reinterpret_cast<const Elf64Ehdr*>(data.data());
    auto be64 = [&](uint64_t v) {
        return ((v >> 56) & 0xFFULL) | ((v >> 40) & 0xFF00ULL) | ((v >> 24) & 0xFF0000ULL) |
            ((v >> 8) & 0xFF000000ULL) | ((v << 8) & 0xFF00000000ULL) |
            ((v << 24) & 0xFF0000000000ULL) | ((v << 40) & 0xFF000000000000ULL) |
            ((v << 56) & 0xFF00000000000000ULL);
        };
    uint64_t entry = be64(eh->e_entry);
    // Program headers
    for (int i = 0; i < be16(eh->e_phnum); ++i) {
        auto* ph = reinterpret_cast<const Elf64Phdr*>(
            data.data() + be64(eh->e_phoff) + i * be16(eh->e_phentsize));
        if (be32(ph->p_type) != 1) continue; // PT_LOAD
        uint64_t vaddr = be64(ph->p_vaddr);
        uint64_t filesz = be64(ph->p_filesz);
        uint64_t memsz = be64(ph->p_memsz);
        uint64_t off = be64(ph->p_offset);
        bool     rw = (be32(ph->p_flags) & 0x2);
        bool     rx = (be32(ph->p_flags) & 0x1);
        auto seg = std::make_shared<Memory>("SEG" + std::to_string(i));
        seg->Write(0, data.data() + off, filesz);
        if (memsz > filesz) seg->MemSet(filesz, 0, memsz - filesz);
        mmu_.MapMemory(seg, vaddr, vaddr + memsz, 0, true, rw, rx);
    }
    cpu_.SetPC(entry);
}
