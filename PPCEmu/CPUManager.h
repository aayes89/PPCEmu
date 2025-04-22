/* Made by Slam */
/*
CPU handling (threads){soon}
*/
#include "MMU.h"
#include "CPU.h"

class CPUManager {
public:
    CPUManager(MMU* mmu) {
        for (int i = 0; i < 3; ++i) cpu_cores[i] = std::make_unique<CPU>(mmu);
    }
private:
    std::array<std::unique_ptr<CPU>, 3> cpu_cores;
};
