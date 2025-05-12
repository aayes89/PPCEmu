// PPCEmu.h
#pragma once
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include "CPU.h"
#include "MMU.h"
#include "Memory.h"
#include "Display.h"
#include "PPCEmuConfig.h"

// Supported binary formats
enum class BinaryType { ELF32_BE, ELF64_BE, RAW, UNKNOWN };

class PPCEmu {
public:
    explicit PPCEmu(const PPCEmuConfig& config);
    ~PPCEmu() = default;
    PPCEmu(const PPCEmu&) = delete;
    PPCEmu& operator=(const PPCEmu&) = delete;

    // Auto-detect and load binary
    void AutoLoad(const std::string& path);

    // Run the emulation loop
    void Run(int fps = 60);

    // Initialize exception vectors before loading
    void initExceptionHandlers();

private:
    // Mapping setup
    void initMappings();

    // ELF/RAW loaders
    BinaryType DetectFormat(const std::vector<uint8_t>& data) const;
    void LoadELF32(const std::vector<uint8_t>& data);
    void LoadELF64(const std::vector<uint8_t>& data);
    void LoadRAW(const std::string& filename, uint64_t loadAddr);
    std::vector<uint8_t> ReadFileToVector(const std::string& path) const;

    // Core components
    PPCEmuConfig               cfg_;
    CPU                         cpu_;
    MMU                         mmu_;
    std::shared_ptr<Memory>     ram_;
    std::shared_ptr<Display>    fb_;

    // Profiling
    uint64_t                    cycle_count_ = 0;
    const double                cpu_frequency_Hz = 729000000.0; // 729 MHz
    std::chrono::high_resolution_clock::time_point start_time_;
};