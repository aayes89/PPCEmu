// Main.cpp
#include "PPCEmu.h"
#include "PPCEmuConfig.h"
#include <iostream>

int main(int argc, char* argv[]) {
	PPCEmuConfig cfg;
	// Optional: parse command-line args to override defaults
	// e.g. cfg.fbWidth = std::stoi(argv[1]);
	//      cfg.userBase = std::stoull(argv[2], nullptr, 16);
	// Choose to fill or load binary
	// emu.AutoLoad("./kernel/test.bin");
	// emu.AutoLoad("./kernel/1bl.bin");
	// emu.AutoLoad("./kernel/lk.elf");		

	try {
		PPCEmu emu(cfg);
		// Load binary (adjust path or pass as argv)
		//emu.AutoLoad("./kernel/test.bin"); // ok
		emu.AutoLoad("./kernel/lk.elf");
		// Run at 60 FPS
		emu.Run(60);
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}


