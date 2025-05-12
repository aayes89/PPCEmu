// CPU.cpp
#include "CPU.h"
#include "Log.h"
#include <iostream>
#include <atomic>
#include <cmath>
#include <stdio.h>
#include <stdint.h>
#include <ostream>
using namespace std;

CPU::CPU(MMU* mmu) :
	mmu(mmu), PC(0), LR(0), CTR(0), XER(0), MSR(0), FPSCR(0), HID4(0), GQR{ 0 },
	SPRG0(0), SPRG1(0), SPRG2(0), SPRG3(0), GPR{ 0 }, FPR{ 0 }, VPR{ 0,0,0,0 }, SPR{ 0 },
	running(false) {
}

CPU::CPU(MMU* mmu, Display* display)
	: mmu(mmu), display(display), PC(0), LR(0), CTR(0), XER(0),
	MSR(0), FPSCR(0), HID4(0),
	GQR{ 0 }, SPRG0(0), SPRG1(0), SPRG2(0), SPRG3(0),
	GPR{ 0 }, FPR{ 0 }, VPR{ 0,0,0,0 }, SPR{ 0 }, running(false) {
}

void CPU::Reset(uint32_t start_pc, std::array<uint32_t, 32> GPR) {
	PC = start_pc;
	LR = 0;
	CR.value = 0;
	CTR = 0;
	XER = 0;
	FPSCR = 0;
	MSR = 0;
	HID4 = 0;
	SPRG0 = 0;
	SPRG1 = 0;
	SPRG2 = 0;
	SPRG3 = 0;
	this->GPR = GPR;
	running = true;
	//VPR.fill({ 0, 0, 0, 0 });
	VPR.fill({ 0 });
	SPR.fill(0);
	//GQR.fill(0);
	LOG_INFO("CPU", "CPU reset, PC set to 0x%08X", PC);
}

void CPU::Reset() {
	PC = 0;
	for (int i = 0; i < 32; ++i) GPR[i] = 0;
	MSR = 0; // Asegura que MSR[DR]=0, MSR[IR]=0 (sin traducción)
	std::cout << "CPU reset, PC=0x" << std::hex << PC << ", MSR=0x" << MSR << std::dec << "\n";
	FPR.fill(0.0);
	SPRG0 = 0;
	SPRG1 = 0;
	SPRG2 = 0;
	SPRG3 = 0;

	//GQR.fill(0);
	VPR.fill({ 0 });

	SPR.fill(0);
	LR = 0;
	CR.value = 0;
	CTR = 0;
	XER = 0;
	FPSCR = 0;
	HID4 = 0;
	running = true;
	LOG_INFO("CPU", "CPU reset, PC unchanged at 0x%08X", PC);
}
constexpr uint32_t ExtractBits(uint32_t v, uint32_t a, uint32_t b) {
	if (b >= 31) b = 31;
	if (a > b) return 0;
	return (v >> (31 - b)) & ((1U << (b - a + 1)) - 1);
}

// Maneja instrucciones secuencialmente
void CPU::Step() {
	std::lock_guard<std::mutex> lock(cpu_mutex);
	static uint64_t step_count = 0;
	step_count++;
	/*if (step_count > 50) {
		std::cerr << "CPU: Execution stopped after 50 steps to prevent infinite loop\n";
		throw std::runtime_error("Possible infinite loop detected");
	}*/

	uint64_t old_pc = PC;
	uint32_t instr = mmu->Read32(PC);
	std::cout << "CPU: Step=" << step_count << ", PC=0x" << std::hex << PC << ", r1=0x" << GPR[1] << ", r3=0x" << GPR[3] << ", r10=0x" << GPR[10]	<< ", MSR=0x" << MSR << ", instruction=0x" << instr << std::dec << "\n";

	if (!running) { cout << "App not running!"; return; };
	uint32_t instruction = FetchInstruction();
	std::cout << "CPU: PC=0x" << std::hex << PC << ", r1=0x" << GPR[1] << ", r10=0x" << GPR[10]	<< ", MSR=0x" << MSR << ", instruction=0x" << instruction << std::dec << "\n";

	if (DEC > 0) {
		DEC--;
		if (DEC == 0 && (MSR & 0x8000)) { // EE bit enabled
			TriggerException(0x900);
		}
	}
	// Decode and execute instruction
	try {
		//execute(instruction); // Disassembly code
		DecodeExecute(instruction); // Normal execution
		if (!(instruction >> 26 == 18)) { // Skip PC increment for branch
			PC += 4;
		}		
	}
	catch (const std::exception& e) {
		std::cerr << "[ERROR] Halt at PC=0x" << std::hex << PC << ": " << e.what() << std::endl;
		DumpRegisters();
		running = false; // STOP CPU en caso de fallo crítico
		throw;
	}
}

// Captura la instrucción del momento
uint32_t CPU::FetchInstruction() {
	if (PC % 4 != 0) {
		throw std::runtime_error("FetchInstruction: PC misaligned");
	}
	return mmu->Read32(PC);
}

void CPU::HandleSyscall() {
	uint32_t syscall_id = GPR[0];

	/*switch (syscall_id) {
	case 0x01:
	case 0x80:{ // Print string at (x,y)*/
	uint32_t addr = GPR[3]; // dirección del string
	uint32_t x = GPR[4];
	uint32_t y = GPR[5];
	uint32_t color = GPR[6]; // ARGB o RGBA según tu formato

	std::string text;
	while (true) {
		uint8_t ch = mmu->Read8(addr++);
		if (ch == 0) break;
		text += static_cast<char>(ch);
	}

	if (display) {
		display->BlitText(x, y, text, color);
		display->Present();
	}
	/*break;
}
default:
	LOG_WARNING("CPU", "Unhandled syscall id=0x%08X", syscall_id);
	break;
}*/

	PC = SRR0 + 4; // restaurar
	MSR = SRR1;
}

// Manejo de excepciones
void CPU::TriggerException(uint32_t vector) {
	SRR0 = PC; // Guardar PC actual
	SRR1 = MSR; // Guardar estado de MSR
	MSR &= ~0x8000; // Deshabilitar interrupciones externas (EE=0)
	PC = vector; // Saltar al vector de excepción

	if (vector == 0x200 || vector == 200) {
		HandleSyscall();
	}
	LOG_INFO("CPU", "Exception triggered, vector=0x%08X", vector);
}

// --- TriggerTrap: usar excepción de programa/prog trap ---
void CPU::TriggerTrap() {
	trapFlag = true;
	TriggerException(PPU_EX_PROG); // use program exception vector
}

void CPU::HandleCRInstructions(uint32_t instr, uint32_t sub) {
	uint32_t crfd = ExtractBits(instr, 6, 10);
	uint32_t crfs = ExtractBits(instr, 11, 15);
	uint32_t crft = ExtractBits(instr, 16, 20);

	if (sub == 0) { // mcrf
		uint32_t src = (CR.value >> ((7 - crfs) * 4)) & 0xF;
		CR.value &= ~(0xF << ((7 - crfd) * 4));
		CR.value |= src << ((7 - crfd) * 4);
		return;
	}

	uint32_t vfs = (CR.value >> ((7 - crfs) * 4)) & 0xF;
	uint32_t vft = (CR.value >> ((7 - crft) * 4)) & 0xF;
	uint32_t vr = 0;

	switch (sub) {
	case 257: vr = vfs & vft; break;     // crand
	case 225: vr = ~(vfs & vft) & 0xF; break; // crnand
	case 33:  vr = ~(vfs | vft) & 0xF; break; // crnor
	case 449: vr = vfs | vft; break;     // cror
	case 193: vr = vfs ^ vft; break;     // crxor
	case 289: vr = vfs ^ (~vft & 0xF); break; // creqv
	case 129: vr = vfs & (~vft & 0xF); break; // crandc
	case 417: vr = (~vfs & 0xF) | vft; break; // crorc

	case 0x4C000064:// rfi - Return From Interrupt
	case 76: {
		PC = SRR0;
		MSR = SRR1;
		std::cout << "[CPU] RFI executed: PC=0x" << std::hex << PC << ", MSR=0x" << MSR << std::endl;
		return;
	}
	default:
		std::cout << "[WARN] Unhandled CR logical subopcode: " << sub << std::endl;
		return;
	}

	CR.value &= ~(0xF << ((7 - crfd) * 4));
	CR.value |= (vr & 0xF) << ((7 - crfd) * 4);
}
void CPU::HandleBranchConditional(uint32_t instr, bool to_ctr) {
	uint32_t bo = ExtractBits(instr, 6, 10);
	uint32_t bi = ExtractBits(instr, 11, 15);
	bool cond = false;

	if (bo & 0x10) cond = true;
	else {
		bool ctr_ok = ((bo & 0x04) == 0) || (CTR != 0);
		bool cr_ok = ((bo & 0x20) == 0) || (((CR.value >> ((7 - bi) * 4)) & 0x8) != 0);
		cond = ctr_ok && cr_ok;
	}

	if (bo & 0x02) CTR--;

	if (cond) {
		PC = to_ctr ? CTR : LR;
	}
}
void CPU::HandleISync() {
#if defined(__GNUC__)
	__sync_synchronize();
#elif defined(_MSC_VER)
	_ReadWriteBarrier();
#endif
}

// --- cargar 16 bytes contiguos en un registro vectorial ---
std::array<uint32_t, 32> CPU::LoadVectorShiftLeft(uint32_t addr) {
	std::array<uint32_t, 32> result = { 0 };
	for (int i = 0; i < 32; ++i) {
		result[i] = mmu->Read32(addr + i * 4);
	}
	return result;
}

// --- idéntico, pero para ShiftRight semantics ---
std::array<uint32_t, 32> CPU::LoadVectorShiftRight(uint32_t addr) {
	std::array<uint32_t, 32> result = { 0 };
	uint32_t offset = addr & 0xF;  // los bits 0-3 determinan el desplazamiento	
	for (int i = 0; i < 32; ++i) {
		uint32_t value = mmu->Read32(addr + i * 4);
		result[i] = value >> offset;
	}
	return result;
}

// Máscara 
uint32_t CPU::MaskFromMBME(uint32_t MB, uint32_t ME) {
	if (MB <= ME)
		return ((0xFFFFFFFF >> MB) & (0xFFFFFFFF << (31 - ME)));
	else
		return ((0xFFFFFFFF >> MB) | (0xFFFFFFFF << (31 - ME)));
}

// Manejo de estados
void CPU::SerializeState(std::ostream& out) {
	out.write(reinterpret_cast<const char*>(&PC), sizeof(PC));
	out.write(reinterpret_cast<const char*>(&LR), sizeof(LR));
	out.write(reinterpret_cast<const char*>(GPR.data()), GPR.size() * sizeof(uint32_t));

	out.write(reinterpret_cast<const char*>(&GQR), sizeof(GQR));
	out.write(reinterpret_cast<const char*>(&CTR), sizeof(CTR));
	out.write(reinterpret_cast<const char*>(&XER), sizeof(XER));
	out.write(reinterpret_cast<const char*>(&MSR), sizeof(MSR));
	out.write(reinterpret_cast<const char*>(&FPSCR), sizeof(FPSCR));
	out.write(reinterpret_cast<const char*>(&SRR0), sizeof(SRR0));
	out.write(reinterpret_cast<const char*>(&SRR1), sizeof(SRR1));
	out.write(reinterpret_cast<const char*>(&SPRG0), sizeof(SPRG0));
	out.write(reinterpret_cast<const char*>(&SPRG1), sizeof(SPRG1));
	out.write(reinterpret_cast<const char*>(&SPRG2), sizeof(SPRG2));
	out.write(reinterpret_cast<const char*>(&SPRG3), sizeof(SPRG3));
	out.write(reinterpret_cast<const char*>(&HID0), sizeof(HID0));
	out.write(reinterpret_cast<const char*>(&HID1), sizeof(HID1));
	out.write(reinterpret_cast<const char*>(&HID4), sizeof(HID4));
	out.write(reinterpret_cast<const char*>(&DEC), sizeof(DEC));
	out.write(reinterpret_cast<const char*>(&TBL), sizeof(TBL));
	out.write(reinterpret_cast<const char*>(&TBU), sizeof(TBU));
	out.write(reinterpret_cast<const char*>(&CR), sizeof(CR));
	out.write(reinterpret_cast<const char*>(FPR.data()), FPR.size() * sizeof(double));
	out.write(reinterpret_cast<const char*>(VPR.data()), VPR.size() * sizeof(uint64_t));
	out.write(reinterpret_cast<const char*>(SPR.data()), SPR.size() * sizeof(uint32_t));
	out.write(reinterpret_cast<const char*>(GQR.data()), GQR.size() * sizeof(uint32_t));

}

void CPU::DeserializeState(std::istream& in) {
	in.read(reinterpret_cast<char*>(&PC), sizeof(PC));
	in.read(reinterpret_cast<char*>(&LR), sizeof(LR));
	in.read(reinterpret_cast<char*>(GPR.data()), GPR.size() * sizeof(uint32_t));

	in.read(reinterpret_cast<char*>(&GQR), sizeof(GQR));
	in.read(reinterpret_cast<char*>(&CTR), sizeof(CTR));
	in.read(reinterpret_cast<char*>(&XER), sizeof(XER));
	in.read(reinterpret_cast<char*>(&MSR), sizeof(MSR));
	in.read(reinterpret_cast<char*>(&FPSCR), sizeof(FPSCR));
	in.read(reinterpret_cast<char*>(&SRR0), sizeof(SRR0));
	in.read(reinterpret_cast<char*>(&SRR1), sizeof(SRR1));
	in.read(reinterpret_cast<char*>(&SPRG0), sizeof(SPRG0));
	in.read(reinterpret_cast<char*>(&SPRG1), sizeof(SPRG1));
	in.read(reinterpret_cast<char*>(&SPRG2), sizeof(SPRG2));
	in.read(reinterpret_cast<char*>(&SPRG3), sizeof(SPRG3));
	in.read(reinterpret_cast<char*>(&HID0), sizeof(HID0));
	in.read(reinterpret_cast<char*>(&HID1), sizeof(HID1));
	in.read(reinterpret_cast<char*>(&HID4), sizeof(HID4));
	in.read(reinterpret_cast<char*>(&DEC), sizeof(DEC));
	in.read(reinterpret_cast<char*>(&TBL), sizeof(TBL));
	in.read(reinterpret_cast<char*>(&TBU), sizeof(TBU));
	in.read(reinterpret_cast<char*>(&CR), sizeof(CR));
	in.read(reinterpret_cast<char*>(FPR.data()), FPR.size() * sizeof(double));
	in.read(reinterpret_cast<char*>(VPR.data()), VPR.size() * sizeof(uint64_t));
	in.read(reinterpret_cast<char*>(SPR.data()), SPR.size() * sizeof(uint32_t));
	in.read(reinterpret_cast<char*>(GQR.data()), GQR.size() * sizeof(uint32_t));

}

// Ver registros
void CPU::DumpRegisters() const {
	std::cout << " === REGISTERS DUMP === " << "\n";
	std::cout << "PC: 0x" << PC << "\n";
	std::cout << "LR: 0x" << LR << "\n";
	std::cout << "CR: 0x" << CR.value << "\n";
	std::cout << "CTR: 0x" << CTR << "\n";
	std::cout << "XER: 0x" << XER << "\n";
	std::cout << "MSR: 0x" << MSR << "\n";
	std::cout << "FPSCR: 0x" << FPSCR << "\n";
	std::cout << "SRR0: 0x" << SRR0 << "\n";
	std::cout << "SRR1: 0x" << SRR1 << "\n";
	std::cout << "SPRG0: 0x" << SPRG0 << "\n";
	std::cout << "SPRG1: 0x" << SPRG1 << "\n";
	std::cout << "SPRG2: 0x" << SPRG2 << "\n";
	std::cout << "SPRG3: 0x" << SPRG3 << "\n";
	std::cout << "HID0: 0x" << HID0 << "\n";
	std::cout << "HID1: 0x" << HID1 << "\n";
	std::cout << "HID4: 0x" << HID4 << "\n";
	std::cout << "DEC: 0x" << DEC << "\n";
	std::cout << "TBL: 0x" << TBL << "\n";
	std::cout << "TBU: 0x" << TBU << "\n";

	std::cout << " === GPR === " << "\n";
	for (int i = 0; i < 32; ++i) // GPR
		std::cout << "r" << i << ": 0x" << std::hex << GPR[i] << "\n";
	std::cout << " === END GPR REGISTER === " << "\n";
	/*std::cout << " === FPR === " << "\n";
	for (int i = 0; i < 32; ++i) // FPR
		std::cout << "r" << i << ": 0x" << std::hex << FPR[i] << "\n";
	std::cout << " === END FPR REGISTER === " << "\n";
	std::cout << " === SPR === " << "\n";
	for (int i = 0; i < 1024; ++i) // SPR
		std::cout << "r" << i << ": 0x" << std::hex << SPR[i] << "\n";
	std::cout << " === END SPR REGISTER === " << "\n";
	std::cout << " === GQR === " << "\n";
	for (int i = 0; i < 8; ++i) // GQR
		std::cout << "r" << i << ": 0x" << std::hex << GQR[i] << "\n";
	std::cout << " === END GQR REGISTER === " << "\n";
		*/
	std::cout << " === END ALL REGISTERS DUMP === " << "\n";
}

unsigned int CPU::invertirBytes(unsigned int valor) {
	unsigned int resultado = 0;
	for (int i = 0; i < 4; ++i) {
		resultado |= ((valor >> (i * 8)) & 0xFF) << ((3 - i) * 8);
	}
	return resultado;
}


void CPU::DecodeExecute(uint32_t instr) {
	uint32_t opcode = (instr >> 26) & 0x3F;
	uint32_t rt = (instr >> 21) & 0x1F; // Bits 21–25
	uint32_t ra = (instr >> 16) & 0x1F; // Bits 16–20
	uint32_t rs = (instr >> 21) & 0x1F; // Bits 21–25
	int16_t imm = instr & 0xFFFF;
	int16_t offset = imm;
	uint32_t addr = (ra == 0 ? 0 : GPR[ra]) + imm;

	uint32_t case1 = ExtractBits(instr, 0, 5);
	uint32_t case2 = (ExtractBits(instr, 21, 27) << 3) | ExtractBits(instr, 28, 30);
	// (ExtractBits(instr, 21, 27) << 4) | (ExtractBits(instr, 30, 31) << 0);
	uint32_t case3 = ExtractBits(instr, 21, 31);
	uint32_t case4 = ExtractBits(instr, 22, 31);
	uint32_t case5 = ExtractBits(instr, 26, 31);

	//uint32_t rt = ExtractBits(instr, 6, 10);
	//ra = ExtractBits(instr, 11, 15);
	uint32_t rb = ExtractBits(instr, 16, 20);
	uint32_t src1 = VPR[ra];//.at(ra);// [ra] ;
	uint32_t src2 = VPR[rb];
	uint32_t res;
	uint8_t* a = reinterpret_cast<uint8_t*>(&src1);
	uint8_t* b = reinterpret_cast<uint8_t*>(&src2);
	uint8_t* r = reinterpret_cast<uint8_t*>(&res);

	uint8_t* aa = reinterpret_cast<uint8_t*>(&VPR[ra]);
	uint8_t* bb = reinterpret_cast<uint8_t*>(&VPR[rb]);
	uint8_t* rr = reinterpret_cast<uint8_t*>(&VPR[rt]);
	uint8_t tmp[16];
	uint64_t acc;

	LOG_INFO("[CPU]", "OPCODE %d Instruccion 0x%008X", opcode, instr);
	switch (opcode) {
	case 0: { // MagicKey        	
		uint32_t op = instr & 0xFC0007FE; // mask: bits 0–1(always 0), 6–10(op), 21–30(XO)
		if (instr == 0) {
			break; // NOP
		}
		if (op == 0) {
			// instrucción “null” real: PC += 4 (ya lo hace el Step)			
			break;
		}
		else if (op == 0x7C0004A6) { // formato de nop en PPC
			//PC += 4;
			break;
		}
		else if (ExtractBits(instr, 0, 15) == 0x7c00) {
			//PC += 4;
			break;
		}

		/*if (instr == 0x3410583 || instr == 0x83054103) {
			std::cout<<"[1BL]   MagicKey Stage 1 detected!"<<std::endl;
			break;
		}
		if (instr == 0x7c00) { // 32KB - 31744B
			LOG_INFO("", "[2BL]  1BL total size: %dB", instr);
			PC = (0xF8);
		}
		else
			if (instr == 0x100) { // 256B for Header and Copyrights
				LOG_INFO("", "[1BL]   Valid 1BL Stage Size: 0x%08X, EntryPoint at: 0x%08X", instr, (0xF8));
			}
			else {
				LOG_INFO("", "[1BL]   Invalid 1BL Stage Size: 0x%08X", instr);
			}*/
		LOG_ERROR("[CPU]", "Invalid MagicKey instruction 0x%08X", instr);
		TriggerException(PPU_EX_PROG);
		break;
	}
	case 2: { // tdi - Trap immediate
		uint32_t tocr = (instr >> 21) & 0x1F;
		uint32_t tbr = (instr >> 16) & 0x1F;
		int16_t simm = instr & 0xFFFF;
		int32_t ra_val = (int32_t)GPR[tbr];
		bool trap = false;
		if (tocr & 0x10) trap |= (ra_val < simm);
		if (tocr & 0x08) trap |= (ra_val > simm);
		if (tocr & 0x04) trap |= (ra_val == simm);
		if (tocr & 0x02) trap |= (ra_val < 0);
		if (tocr & 0x01) trap |= (ra_val > 0);

		//uint32_t tocr = ExtractBits(instr, 21, 25);
		//uint32_t tbr = ExtractBits(instr, 16, 20);				

		if (trap) {
			TriggerException(PPU_EX_PROG); // o el vector adecuado
			return; // salir sin avanzar PC extra
		}
		break;
	}
	case 3: {
		// twi - Trap word immediate
		uint32_t tocr = ExtractBits(instr, 21, 25);
		uint32_t ra = ExtractBits(instr, 16, 20);
		int16_t simm = (int16_t)ExtractBits(instr, 0, 15);
		int32_t ra_val = (int32_t)GPR[ra];

		bool trap = false;
		if (tocr & 0x10) trap |= (ra_val < simm);
		if (tocr & 0x08) trap |= (ra_val > simm);
		if (tocr & 0x04) trap |= (ra_val == simm);
		if (tocr & 0x02) trap |= (ra_val < 0);
		if (tocr & 0x01) trap |= (ra_val > 0);

		if (trap) TriggerTrap();  // Asumiendo función TriggerTrap() existe
		break;
	}
	case 4: {
		switch (case2) {

		case 3: {
			// lvsl128 - Load Vector Shift Left
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = (GPR[ra] + GPR[rb]) & ~0xF; // align 16 bytes
			VPR = LoadVectorShiftLeft(addr);
			break;
		}
		case 67: { // lvsr - Load Vector Shift Right		
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 11, 15);
			uint32_t rb = ExtractBits(instr, 16, 20);
			uint32_t addr = (GPR[ra] + GPR[rb]) & ~0xF;
			VPR = LoadVectorShiftRight(addr);
			break;
		}
		case 131: {
			// lvewx128 - Load Vector Element Word Indexed
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t word = mmu->Read32(addr);
			VPR[rt] = word;  // Solo primera palabra en el vector			
			break;
		}
		case 195: {
			// lvx128 - Load Vector Indexed
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->Read64(addr);
			/*uint32_t addr = GPR[ra] + GPR[rb];
			for (int i = 0; i < 16; i++) {
				VPR[rt][i] = mmu->Read8(addr + i);
			}*/
			break;
		}
		case 387: {	// stvewx128 - Store Vector Element Word Indexed
			/*uint32_t rs = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write32(addr, VPR[rs]);
			break;*/
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t vs = ExtractBits(instr, 6, 10); // vector source register
			uint32_t addr = GPR[ra] + GPR[rb];
			// El elemento se elige en base a bits [4:5] del address (alignment).
			uint32_t element = (addr >> 2) & 0x3; // 0..3 (selecciona palabra 0,1,2,3)
			uint32_t value = 0;
			if constexpr (sizeof(VPR[vs]) == 16) { // 128-bit real
				value = (reinterpret_cast<const uint32_t*>(&VPR[vs]))[element];
			}
			else {
				// Si simulaste como std::array<uint32_t, 4>:
				value = VPR[element];
			}
			mmu->Write32(addr, value);
			break;
		}
		case 451: {
			// stvx128 - Store Vector Indexed
			uint32_t rs = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rs]);
			break;
		}
		case 707: {
			// lvxl128 - Load Vector Indexed with Update (load with possible update)
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->Read64(addr);
			break;
		}
		case 963: {
			// stvxl128 - Store Vector Indexed with Update
			uint32_t rs = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rs]);
			break;
		}
		case 1027: {
			// lvlx128 - Load Vector Left Indexed
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->ReadLeft(addr);
			break;
		}
		case 1091: {
			// lvrx128 - Load Vector Right Indexed
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->ReadRight(addr);
			break;
		}
		case 1283: {
			// stvlx128 - Store Vector Left Indexed
			uint32_t rs = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->WriteLeft(addr, VPR[rs]);
			break;
		}
		case 1347: {
			// stvrx128 - Store Vector Right Indexed
			uint32_t rs = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->WriteRight(addr, VPR[rs]);
			break;
		}
		case 1539: {
			// lvlxl128 - Load Vector Left Indexed with Update
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->ReadLeft(addr);
			break;
		}
		case 1603: {
			// lvrxl128 - Load Vector Right Indexed with Update
			uint32_t rt = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->ReadRight(addr);
			break;
		}
		case 1795: {
			// stvlxl128 - Store Vector Left Indexed with Update
			uint32_t rs = ExtractBits(instr, 6, 10);
			uint32_t ra = ExtractBits(instr, 16, 20);
			uint32_t rb = ExtractBits(instr, 11, 15);
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->WriteLeft(addr, VPR[rs]);
			break;
		}
				 switch (case3) {
				 case 0: {// vaddubm
					 for (int i = 0;i < 16;i++) r[i] = a[i] + b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 2: {// vmaxub
					 for (int i = 0;i < 16;i++) r[i] = a[i] > b[i] ? a[i] : b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 4: { // vrlb
					 for (int i = 0;i < 16;i++) r[i] = (a[i] >> (b[i] & 0x7)) | (a[i] << ((8 - (b[i] & 0x7)) & 7));
					 VPR[rt] = res;
					 break;
				 }
				 case 8: { // vmuloub
					 for (int i = 0;i < 16;i++) r[i] = (a[i] * b[i]) & 0xFF;
					 VPR[rt] = res;
					 break;
				 }
				 case 10: {// vaddfp  (float32 lanes)

					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 float* fr = reinterpret_cast<float*>(&res);
					 for (int i = 0;i < 4;i++) fr[i] = fa[i] + fb[i];
					 VPR[rt] = res;

					 break;
				 }
				 case 12: {// vmrghb
					 for (int i = 0;i < 8;i++) { r[2 * i] = b[2 * i + 1]; r[2 * i + 1] = a[2 * i + 1]; }
					 VPR[rt] = res;
					 break;
				 }
				 case 14: {// vpkuhum
					 for (int i = 0;i < 8;i++) {
						 uint16_t  v = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t(v >> 8);
						 r[2 * i + 1] = uint8_t(v & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 64: {// vadduhm
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 uint16_t vb = (uint16_t)b[2 * i] << 8 | b[2 * i + 1];
						 uint16_t vr = va + vb;
						 r[2 * i] = vr >> 8;
						 r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 66: {// vmaxuh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 uint16_t vb = (uint16_t)b[2 * i] << 8 | b[2 * i + 1];
						 uint16_t vr = va > vb ? va : vb;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 68: {// vrlh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 uint16_t sh = ((uint16_t)b[2 * i] << 8 | b[2 * i + 1]) & 0xF;
						 uint16_t vr = (va >> sh) | (va << (16 - sh));
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 72: {// vmulouh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 uint16_t vb = (uint16_t)b[2 * i] << 8 | b[2 * i + 1];
						 uint32_t mul = va * vb;
						 uint16_t vr = (mul >> 16) & 0xFFFF;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 74: { // vsubfp

					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 float* fr = reinterpret_cast<float*>(&res);
					 for (int i = 0;i < 4;i++) fr[i] = fa[i] - fb[i];
					 VPR[rt] = res;

					 break;
				 }
				 case 76: { // vmrghh
					 for (int i = 0;i < 4;i++) {
						 uint16_t hi_a = (a[4 * i] << 8) | a[4 * i + 1];
						 uint16_t hi_b = (b[4 * i] << 8) | b[4 * i + 1];
						 r[4 * i] = b[4 * i + 1];
						 r[4 * i + 1] = a[4 * i + 1];
						 r[4 * i + 2] = b[4 * i];
						 r[4 * i + 3] = a[4 * i];
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 78: {// vpkuwum
					 for (int i = 0;i < 8;i++) {
						 uint16_t v = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 r[2 * i] = v >> 8;
						 r[2 * i + 1] = v & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 128: {// vadduwm
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = reinterpret_cast<uint32_t*>(&a[4 * i])[0];
						 uint32_t vb = reinterpret_cast<uint32_t*>(&b[4 * i])[0];
						 uint32_t vr = va + vb;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 130: { // vmaxuw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = reinterpret_cast<uint32_t*>(&a[4 * i])[0];
						 uint32_t vb = reinterpret_cast<uint32_t*>(&b[4 * i])[0];
						 uint32_t vr = va > vb ? va : vb;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 132: {// vrlw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = reinterpret_cast<uint32_t*>(&a[4 * i])[0];
						 uint32_t sh = reinterpret_cast<uint32_t*>(&b[4 * i])[0] & 0x1F;
						 uint32_t vr = (va >> sh) | (va << (32 - sh));
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 140: {// vmrghw
					 for (int i = 0;i < 2;i++) {
						 uint32_t wa = reinterpret_cast<uint32_t*>(&a[8 * i])[0];
						 uint32_t wb = reinterpret_cast<uint32_t*>(&b[8 * i])[0];
						 uint32_t w0 = (wb & 0xFFFF0000) | (wa & 0x0000FFFF);
						 uint32_t w1 = (wb & 0x0000FFFF) << 16 | (wa >> 16);
						 std::memcpy(&r[8 * i], &w0, 4);
						 std::memcpy(&r[8 * i + 4], &w1, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 142: {// vpkuhus
					 for (int i = 0;i < 8;i++) {
						 uint16_t v = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = v >> 8;
						 r[2 * i + 1] = v & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 206: { // vpkuwus
					 for (int i = 0;i < 8;i++) {
						 uint16_t v = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = v >> 8;
						 r[2 * i + 1] = v & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 258: {// vmaxsb
					 for (int i = 0;i < 16;i++) {
						 int8_t sa = int8_t(a[i]), sb = int8_t(b[i]);
						 r[i] = uint8_t(sa > sb ? sa : sb);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 260: {// vslb
					 for (int i = 0;i < 16;i++) {
						 uint8_t sh = b[i] & 0x7;
						 r[i] = (a[i] << sh) | (a[i] >> (8 - sh));
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 264: {// vmulosb
					 for (int i = 0;i < 16;i++) {
						 int16_t prod = int8_t(a[i]) * int8_t(b[i]);
						 r[i] = uint8_t((prod >> 8) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 266: {// vrefp
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fb = reinterpret_cast<float*>(b);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = fa[i] < 0 ? -fb[i] : fb[i];
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 268: {// vmrglb
					 for (int i = 0;i < 8;i++) {
						 r[2 * i] = b[2 * i];
						 r[2 * i + 1] = a[2 * i];
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 270: {// vpkshus
					 for (int i = 0;i < 8;i++) {
						 uint16_t v = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = v >> 8;
						 r[2 * i + 1] = v & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 322: {// vmaxsh
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int16_t vr = va > vb ? va : vb;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 324: {// vslh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t sh = ((uint16_t(b[2 * i]) << 8) | b[2 * i + 1]) & 0xF;
						 uint16_t vr = (va << sh) | (va >> (16 - sh));
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 328: {// vmulosh
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int32_t prod = va * vb;
						 int16_t vr = (prod >> 16) & 0xFFFF;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 330: {// vrsqrtefp
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = 1.0f / sqrtf(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 332: {// vmrglh
					 for (int i = 0;i < 4;i++) {
						 uint16_t low_b = (b[4 * i] << 8) | b[4 * i + 1];
						 uint16_t high_a = (a[4 * i + 2] << 8) | a[4 * i + 3];
						 r[4 * i] = low_b >> 8; r[4 * i + 1] = low_b & 0xFF;
						 r[4 * i + 2] = high_a >> 8; r[4 * i + 3] = high_a & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 334: { // vpkswus
					 for (int i = 0;i < 8;i++) {
						 uint16_t v = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = v >> 8; r[2 * i + 1] = v & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 384: {// vaddcuw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = reinterpret_cast<uint32_t*>(&a[4 * i])[0];
						 uint32_t vb = reinterpret_cast<uint32_t*>(&b[4 * i])[0];
						 uint64_t sum = uint64_t(va) + vb;
						 uint32_t vr = sum & 0xFFFFFFFF;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 386: {// vmaxsw
					 for (int i = 0;i < 4;i++) {
						 int32_t va = reinterpret_cast<int32_t*>(&a[4 * i])[0];
						 int32_t vb = reinterpret_cast<int32_t*>(&b[4 * i])[0];
						 int32_t vr = va > vb ? va : vb;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 388: {// vslw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = reinterpret_cast<uint32_t*>(&a[4 * i])[0];
						 uint32_t sh = reinterpret_cast<uint32_t*>(&b[4 * i])[0] & 0x1F;
						 uint32_t vr = (va << sh) | (va >> (32 - sh));
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 394: { // vexptefp
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = expf(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 396: {// vmrglw
					 for (int i = 0;i < 2;i++) {
						 uint32_t wa = reinterpret_cast<uint32_t*>(&a[8 * i])[0];
						 uint32_t wb = reinterpret_cast<uint32_t*>(&b[8 * i])[0];
						 uint32_t vr = (wb & 0xFFFF0000) | (wa & 0x0000FFFF);
						 std::memcpy(&r[8 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 398: {// vpkshss
					 for (int i = 0;i < 8;i++) {
						 int16_t v = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t((v < 0 ? -v : v) >> 8);
						 r[2 * i + 1] = uint8_t((v < 0 ? -v : v) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 452: {// vsl
					 for (int i = 0;i < 16;i++) {
						 uint8_t sh = b[i] & 0x7;
						 r[i] = a[i] << sh;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 458: {// vlogefp
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = logf(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 462: { // vpkswss
					 for (int i = 0;i < 8;i++) {
						 int16_t v = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t((v < 0 ? -v : v) >> 8);
						 r[2 * i + 1] = uint8_t((v < 0 ? -v : v) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 512: {// vaddubs
					 for (int i = 0;i < 16;i++) {
						 uint16_t sum = uint16_t(a[i]) + b[i];
						 r[i] = sum > 0xFF ? 0xFF : sum;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 514: {// vminub
					 for (int i = 0;i < 16;i++) r[i] = a[i] < b[i] ? a[i] : b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 516: {// vsrb
					 for (int i = 0;i < 16;i++) r[i] = a[i] >> (b[i] & 0x7);
					 VPR[rt] = res;
					 break;
				 }
				 case 520: {// vmuleub
					 for (int i = 0;i < 16;i++) {
						 uint16_t prod = uint16_t(a[i]) * b[i];
						 r[i] = prod & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 522: { // vrfin
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = std::floor(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 524: {// vspltb

					 uint8_t val = a[0];
					 for (int i = 0;i < 16;i++) r[i] = val;

					 VPR[rt] = res;
					 break;
				 }
				 case 526: { // vupkhsb
					 for (int i = 0;i < 8;i++) {
						 int8_t v = int8_t(a[2 * i]);
						 r[2 * i] = uint8_t(v < 0 ? -v : v);
						 r[2 * i + 1] = uint8_t(a[2 * i + 1]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 576: { // vadduhs
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int32_t sum = va + vb;
						 int16_t vr = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 578: { // vminuh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t vb = (uint16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 uint16_t vr = va < vb ? va : vb;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 580: {// vsrh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t sh = ((uint16_t(b[2 * i]) << 8) | b[2 * i + 1]) & 0xF;
						 uint16_t vr = va >> sh;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 584: {// vmuleuh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t vb = (uint16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 uint32_t prod = va * vb;
						 uint16_t vr = prod & 0xFFFF;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 586: {// vrfiz
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = std::round(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 588: {// vsplth

					 uint16_t val = (uint16_t(a[0]) << 8) | a[1];
					 for (int i = 0;i < 8;i++) { r[2 * i] = val >> 8; r[2 * i + 1] = val & 0xFF; }

					 VPR[rt] = res;
					 break;
				 }
				 case 590: {// vupkhsh
					 for (int i = 0;i < 8;i++) {
						 int16_t v = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t((v < 0 ? -v : v) >> 8);
						 r[2 * i + 1] = uint8_t((v < 0 ? -v : v) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 640: {// vadduws
					 for (int i = 0;i < 2;i++) {
						 uint64_t va = *reinterpret_cast<uint32_t*>(&a[8 * i]);
						 uint64_t vb = *reinterpret_cast<uint32_t*>(&b[8 * i]);
						 uint64_t sum = va + vb;
						 std::memcpy(&r[8 * i], &sum, 8);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 642: {// vminuw
					 for (int i = 0;i < 2;i++) {
						 uint32_t va = *reinterpret_cast<uint32_t*>(&a[8 * i]);
						 uint32_t vb = *reinterpret_cast<uint32_t*>(&b[8 * i]);
						 uint32_t vr = va < vb ? va : vb;
						 std::memcpy(&r[8 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 644: { // vsrw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *reinterpret_cast<uint32_t*>(&a[4 * i]);
						 uint32_t sh = *reinterpret_cast<uint32_t*>(&b[4 * i]) & 0x1F;
						 uint32_t vr = va >> sh;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 650: {// vrfip
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = std::floor(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 652: {// vspltw

					 uint32_t val = *reinterpret_cast<uint32_t*>(&a[0]);
					 for (int i = 0;i < 4;i++) std::memcpy(&r[4 * i], &val, 4);

					 VPR[rt] = res;
					 break;
				 }
				 case 654: {// vupklsb
					 for (int i = 0;i < 16;i++) {
						 int8_t v = int8_t(a[i]);
						 r[i] = uint8_t(v < 0 ? -v : v);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 708: {// vsr
					 for (int i = 0;i < 16;i++) {
						 uint8_t sh = b[i] & 0x7;
						 r[i] = a[i] >> sh;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 714: {// vrfim
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 float* fr = reinterpret_cast<float*>(&res);
						 fr[i] = std::floor(fa[i]);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 718: {// vupklsh
					 for (int i = 0;i < 8;i++) {
						 int16_t v = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t((v < 0 ? -v : v) >> 8);
						 r[2 * i + 1] = uint8_t((v < 0 ? -v : v) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 768: {// vaddsbs
					 for (int i = 0;i < 16;i++) {
						 int16_t sum = int8_t(a[i]) + int8_t(b[i]);
						 r[i] = uint8_t(sum<INT8_MIN ? INT8_MIN : sum>INT8_MAX ? INT8_MAX : sum);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 770: {// vminsb
					 for (int i = 0;i < 16;i++) {
						 int8_t sa = int8_t(a[i]), sb = int8_t(b[i]);
						 r[i] = uint8_t((sa < sb ? sa : sb));
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 772: {// vsrab
					 for (int i = 0;i < 16;i++) {
						 int8_t v = int8_t(a[i]);
						 uint8_t sh = b[i] & 0x7;
						 r[i] = uint8_t(v >> sh);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 776: {// vmulesb
					 for (int i = 0;i < 16;i++) {
						 int16_t prod = int8_t(a[i]) * int8_t(b[i]);
						 r[i] = uint8_t((prod >> 8) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 778: {// vcfux
					 for (int i = 0;i < 16;i++) {
						 r[i] = (a[i] & 0x80) ? 0xFF : 0;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 780: {// vspltisb

					 int8_t val = int8_t(a[0]);
					 for (int i = 0;i < 16;i++) r[i] = uint8_t(val);

					 VPR[rt] = res;
					 break;
				 }
				 case 782: { // vpkpx
					 for (int i = 0;i < 16;i++) {
						 uint8_t hi = a[i] & 0xF0;
						 uint8_t lo = b[i] & 0x0F;
						 r[i] = hi | lo;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 832: {// vaddshs
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int32_t sum = va + vb;
						 int16_t vr = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 834: {// vminsh
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int16_t vr = va < vb ? va : vb;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 836: {// vsrah
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t sh = ((uint16_t(b[2 * i]) << 8) | b[2 * i + 1]) & 0xF;
						 int16_t vr = va >> sh;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 840: {// vmulesh
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int32_t prod = va * vb;
						 int16_t vr = (prod >> 16) & 0xFFFF;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 842: {// vcfsx
					 for (int i = 0;i < 4;i++) {
						 int32_t vi = reinterpret_cast<int32_t*>(a)[i];
						 float vf = float(vi);
						 std::memcpy(&r[4 * i], &vf, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 844: {// vspltish			
					 int16_t val = (int16_t(a[0]) << 8) | a[1];
					 for (int i = 0;i < 8;i++) { r[2 * i] = val >> 8; r[2 * i + 1] = val & 0xFF; }
					 VPR[rt] = res;
					 break;
				 }
				 case 846: { // vupkhpx
					 for (int i = 0;i < 8;i++) {
						 int16_t v = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t((v < 0 ? -v : v) >> 8);
						 r[2 * i + 1] = uint8_t((v < 0 ? -v : v) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 896: { // vaddsws
					 for (int i = 0;i < 2;i++) {
						 int64_t va = *reinterpret_cast<int32_t*>(&a[8 * i]);
						 int64_t vb = *reinterpret_cast<int32_t*>(&b[8 * i]);
						 int64_t sum = va + vb;
						 std::memcpy(&r[8 * i], &sum, 8);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 898: {// vminsw
					 for (int i = 0;i < 4;i++) {
						 int32_t va = reinterpret_cast<int32_t*>(&a[4 * i])[0];
						 int32_t vb = reinterpret_cast<int32_t*>(&b[4 * i])[0];
						 int32_t vr = va < vb ? va : vb;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 900: { // vsraw
					 for (int i = 0;i < 4;i++) {
						 int32_t va = reinterpret_cast<int32_t*>(&a[4 * i])[0];
						 uint32_t sh = reinterpret_cast<uint32_t*>(&b[4 * i])[0] & 0x1F;
						 int32_t vr = va >> sh;
						 std::memcpy(&r[4 * i], &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 906: { // vctuxs
					 for (int i = 0;i < 4;i++) {
						 float* fa = reinterpret_cast<float*>(a);
						 int32_t vi = int32_t(fa[i]);
						 std::memcpy(&r[4 * i], &vi, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 908: {// vspltisw

					 int32_t val = reinterpret_cast<int32_t*>(&a[0])[0];
					 for (int i = 0;i < 4;i++) std::memcpy(&r[4 * i], &val, 4);

					 VPR[rt] = res;
					 break;
				 }
				 case 970: {// vctsxs
					 for (int i = 0;i < 4;i++) {
						 int32_t* pa = reinterpret_cast<int32_t*>(a);
						 float vf = reinterpret_cast<float*>(b)[i];
						 int16_t vi = int16_t(vf);
						 uint16_t uv = uint16_t(vi);
						 std::memcpy(&r[4 * i], &uv, 2);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 974: {// vupklpx
					 for (int i = 0;i < 8;i++) {
						 int16_t v = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 r[2 * i] = uint8_t((v < 0 ? -v : v) >> 8);
						 r[2 * i + 1] = uint8_t((v < 0 ? -v : v) & 0xFF);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1024: {// vsububm
					 for (int i = 0;i < 16;i++) r[i] = a[i] - b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1026: {// vavgub
					 for (int i = 0;i < 16;i++) r[i] = (uint8_t)((a[i] + b[i] + 1) >> 1);
					 VPR[rt] = res;
					 break;
				 }
				 case 1028: { // vand
					 for (int i = 0;i < 16;i++) r[i] = a[i] & b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1034: { // vmaxfp
					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 float* fr = reinterpret_cast<float*>(&res);
					 for (int i = 0;i < 4;i++) fr[i] = fa[i] > fb[i] ? fa[i] : fb[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1036: {// vslo
					 for (int i = 0;i < 16;i++) { uint8_t sh = b[i] & 0x7; r[i] = a[i] << sh; }
					 VPR[rt] = res;
					 break;
				 }
				 case 1088: {// vsubuhm
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 uint16_t vb = (uint16_t)b[2 * i] << 8 | b[2 * i + 1];
						 uint16_t vr = va - vb;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1090: {// vavguh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t)a[2 * i] << 8 | a[2 * i + 1];
						 uint16_t vb = (uint16_t)b[2 * i] << 8 | b[2 * i + 1];
						 uint16_t vr = (va + vb + 1) >> 1;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1092: {// vandc
					 for (int i = 0;i < 16;i++) r[i] = a[i] & ~b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1098: {// vminfp		
					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 float* fr = reinterpret_cast<float*>(&res);
					 for (int i = 0;i < 4;i++) fr[i] = fa[i] < fb[i] ? fa[i] : fb[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1100: {// vsro
					 for (int i = 0;i < 16;i++) { uint8_t sh = b[i] & 0x7; r[i] = a[i] >> sh; }
					 VPR[rt] = res;
					 break;
				 }
				 case 1152: { // vsubuwm
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *((uint32_t*)(a + 4 * i));
						 uint32_t vb = *((uint32_t*)(b + 4 * i));
						 uint32_t vr = va - vb;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1154: {// vavguw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *((uint32_t*)(a + 4 * i));
						 uint32_t vb = *((uint32_t*)(b + 4 * i));
						 uint32_t vr = (va + vb + 1) >> 1;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1156: {// vor
					 for (int i = 0;i < 16;i++) r[i] = a[i] | b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1220: { // vxor
					 for (int i = 0;i < 16;i++) r[i] = a[i] ^ b[i];
					 VPR[rt] = res;
					 break;
				 }
				 case 1282: { // vavgsb
					 for (int i = 0;i < 16;i++) {
						 int8_t sa = int8_t(a[i]), sb = int8_t(b[i]);
						 int16_t avg = (sa + sb) >> 1;
						 r[i] = uint8_t(avg);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1284: { // vnor
					 for (int i = 0;i < 16;i++) r[i] = ~(a[i] | b[i]);
					 VPR[rt] = res;
					 break;
				 }
				 case 1346: {// vavgsh
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int16_t vr = (va + vb) >> 1;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1408: { // vsubcuw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *((uint32_t*)(a + 4 * i));
						 uint32_t vb = *((uint32_t*)(b + 4 * i));
						 uint32_t vr = va - vb;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1410: {// vavgsw
					 for (int i = 0;i < 4;i++) {
						 int32_t va = *((int32_t*)(a + 4 * i));
						 int32_t vb = *((int32_t*)(b + 4 * i));
						 int32_t vr = (va + vb) >> 1;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1536: {// vsububs
					 for (int i = 0;i < 16;i++) {
						 uint8_t aa = a[i], bb = b[i];
						 r[i] = aa > bb ? aa - bb : 0;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1540: { // mfvscr
					 GPR[rt] = FPSCR;
					 break;
				 }
				 case 1544: {// vsum4ubs
					 for (int i = 0;i < 4;i++) {
						 uint32_t sum = 0;
						 for (int j = 0;j < 4;j++) sum += a[4 * i + j];
						 memcpy(r + 4 * i, &sum, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1600: { // vsubuhs
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int16_t vr = va > vb ? va - vb : 0;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1604: { // mtvscr
					 FPSCR = GPR[rt];
					 break;
				 }
				 case 1608: {// vsum4shs
					 for (int i = 0;i < 4;i++) {
						 int16_t sum = 0;
						 for (int j = 0;j < 2;j++) {
							 sum += (int16_t(a[4 * i + 2 * j]) << 8) | a[4 * i + 2 * j + 1];
						 }
						 r[4 * i] = sum >> 8; r[4 * i + 1] = sum & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1664: { // vsubuws
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *((uint32_t*)(a + 4 * i)), vb = *((uint32_t*)(b + 4 * i));
						 uint32_t vr = va > vb ? va - vb : 0;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1672: { // vsum2sws
					 for (int i = 0;i < 2;i++) {
						 int32_t sum = 0;
						 for (int j = 0;j < 2;j++) sum += *((int32_t*)(a + 4 * (2 * i + j)));
						 memcpy(r + 4 * i, &sum, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1792: { // vsubsbs
					 for (int i = 0;i < 16;i++) {
						 int16_t d = int8_t(a[i]) - int8_t(b[i]);
						 if (d > 127) d = 127; if (d < -128) d = -128;
						 r[i] = uint8_t(d);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1800: { // vsum4sbs
					 for (int i = 0;i < 4;i++) {
						 int32_t sum = 0;
						 for (int j = 0;j < 4;j++) sum += int8_t(a[4 * i + j]);
						 memcpy(r + 4 * i, &sum, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1856: { // vsubshs
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 int32_t d = va - vb;
						 if (d > 32767) d = 32767; if (d < -32768) d = -32768;
						 int16_t vr = d;
						 r[2 * i] = vr >> 8; r[2 * i + 1] = vr & 0xFF;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1920: {// vsubsws
					 for (int i = 0;i < 4;i++) {
						 int32_t va = *((int32_t*)(a + 4 * i)), vb = *((int32_t*)(b + 4 * i));
						 int64_t d = int64_t(va) - vb;
						 if (d > INT32_MAX) d = INT32_MAX; if (d < INT32_MIN) d = INT32_MIN;
						 int32_t vr = d;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 1928: { // vsumsws
					 for (int i = 0;i < 4;i++) {
						 int64_t sum = 0;
						 for (int j = 0;j < 4;j++) sum += *((int32_t*)(a + 4 * (i * 4 + j)));
						 if (sum > INT32_MAX) sum = INT32_MAX; if (sum < INT32_MIN) sum = INT32_MIN;
						 int32_t vr = sum;
						 memcpy(r + 4 * i, &vr, 4);
					 }
					 VPR[rt] = res;
					 break;
				 }
				 default: {
					 //PPC_DECODER_MISS;
					 std::cout << "Default method for case 4 ext undef" << std::endl;
				 }
				 }
				 switch (case4) {
				 case 6: { // vcmpequb
					 for (int i = 0;i < 16;i++) r[i] = (a[i] == b[i]) ? 0xFF : 0x00;
					 VPR[rt] = res;
					 break;
				 }
				 case 70: { // vcmpequh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t vb = (uint16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 bool eq = va == vb;
						 r[2 * i] = eq ? 0xFF : 0x00;
						 r[2 * i + 1] = eq ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 134: {// vcmpequw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *reinterpret_cast<uint32_t*>(a + 4 * i);
						 uint32_t vb = *reinterpret_cast<uint32_t*>(b + 4 * i);
						 bool eq = va == vb;
						 for (int j = 0;j < 4;j++) r[4 * i + j] = eq ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 198: // vcmpeqfp
				 {
					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 uint8_t* rr = r;
					 for (int i = 0;i < 4;i++) {
						 bool eq = fa[i] == fb[i];
						 uint8_t v = eq ? 0xFF : 0x00;
						 for (int j = 0;j < 4;j++) rr[4 * i + j] = v;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 454: // vcmpgefp
				 {
					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 for (int i = 0;i < 4;i++) {
						 bool ge = fa[i] >= fb[i];
						 uint8_t v = ge ? 0xFF : 0x00;
						 for (int j = 0;j < 4;j++) r[4 * i + j] = v;
					 }
					 VPR[rt] = res;

					 break;
				 }
				 case 518: { // vcmpgtub
					 for (int i = 0;i < 16;i++) r[i] = (a[i] > b[i]) ? 0xFF : 0x00;
					 VPR[rt] = res;
					 break;
				 }
				 case 582: {// vcmpgtuh
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 uint16_t vb = (uint16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 bool gt = va > vb;
						 r[2 * i] = gt ? 0xFF : 0x00;
						 r[2 * i + 1] = gt ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 646: { // vcmpgtuw
					 for (int i = 0;i < 4;i++) {
						 uint32_t va = *reinterpret_cast<uint32_t*>(a + 4 * i);
						 uint32_t vb = *reinterpret_cast<uint32_t*>(b + 4 * i);
						 bool gt = va > vb;
						 for (int j = 0;j < 4;j++) r[4 * i + j] = gt ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 710: // vcmpgtfp
				 {
					 float* fa = reinterpret_cast<float*>(a);
					 float* fb = reinterpret_cast<float*>(b);
					 for (int i = 0;i < 4;i++) {
						 bool gt = fa[i] > fb[i];
						 uint8_t v = gt ? 0xFF : 0x00;
						 for (int j = 0;j < 4;j++) r[4 * i + j] = v;
					 }
					 VPR[rt] = res;

					 break;
				 }
				 case 774: { // vcmpgtsb
					 for (int i = 0;i < 16;i++) {
						 int8_t sa = int8_t(a[i]), sb = int8_t(b[i]);
						 r[i] = (sa > sb) ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 838: { // vcmpgtsh
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(a[2 * i]) << 8) | a[2 * i + 1];
						 int16_t vb = (int16_t(b[2 * i]) << 8) | b[2 * i + 1];
						 bool gt = va > vb;
						 r[2 * i] = gt ? 0xFF : 0x00;
						 r[2 * i + 1] = gt ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 902: { // vcmpgtsw
					 for (int i = 0;i < 4;i++) {
						 int32_t va = *reinterpret_cast<int32_t*>(a + 4 * i);
						 int32_t vb = *reinterpret_cast<int32_t*>(b + 4 * i);
						 bool gt = va > vb;
						 for (int j = 0;j < 4;j++) r[4 * i + j] = gt ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 case 966: { // vcmpbfp
					 for (int i = 0;i < 4;i++) {
						 uint32_t wa = *reinterpret_cast<uint32_t*>(a + 4 * i);
						 uint32_t wb = *reinterpret_cast<uint32_t*>(b + 4 * i);
						 bool eq = (wa == wb);
						 for (int j = 0;j < 4;j++) r[4 * i + j] = eq ? 0xFF : 0x00;
					 }
					 VPR[rt] = res;
					 break;
				 }
				 default: {
					 //PPC_DECODER_MISS;
					 std::cout << "Default method for case 4 ..." << std::endl;
				 }
				 }
				 switch (case5) {
				 case 32: {// vmhaddshs: Half round add signed halfwords
					 for (int i = 0;i < 8;i++) {
						 int16_t wa = (int16_t(aa[2 * i]) << 8) | aa[2 * i + 1];
						 int16_t wb = (int16_t(bb[2 * i]) << 8) | bb[2 * i + 1];
						 int32_t sum = wa + wb;
						 sum = (sum >= 0 ? (sum + 1) / 2 : (sum - 1) / 2);
						 int16_t hr = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 tmp[2 * i] = hr >> 8;
						 tmp[2 * i + 1] = hr & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 break;
				 }
				 case 33: {// vmhraddshs: Half round add with accumulate
					 acc = VACC;
					 for (int i = 0;i < 8;i++) {
						 int16_t wa = (int16_t(aa[2 * i]) << 8) | aa[2 * i + 1];
						 int16_t wb = (int16_t(bb[2 * i]) << 8) | bb[2 * i + 1];
						 int16_t ha = (int16_t(reinterpret_cast<uint8_t*>(&acc)[2 * i]) << 8) | reinterpret_cast<uint8_t*>(&acc)[2 * i + 1];
						 int32_t sum = wa + wb + ha;
						 sum = (sum >= 0 ? (sum + 1) / 2 : (sum - 1) / 2);
						 int16_t hr = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 tmp[2 * i] = hr >> 8;
						 tmp[2 * i + 1] = hr & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 VACC = VPR[rt];
					 break;
				 }
				 case 34: { // vmladduhm: multiply-add unsigned halfwords into accumulator
					 acc = VACC;
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(aa[2 * i]) << 8) | aa[2 * i + 1];
						 uint16_t vb = (uint16_t(bb[2 * i]) << 8) | bb[2 * i + 1];
						 uint32_t prod = va * vb;
						 uint16_t high = (prod >> 16) & 0xFFFF;
						 int16_t ha = (int16_t(reinterpret_cast<uint8_t*>(&acc)[2 * i]) << 8) | reinterpret_cast<uint8_t*>(&acc)[2 * i + 1];
						 int32_t sum = ha + high;
						 int16_t resh = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 tmp[2 * i] = resh >> 8;
						 tmp[2 * i + 1] = resh & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 VACC = VPR[rt];
					 break;
				 }
				 case 36: {// vmsumubm: sum bytes unsigned into halfword lanes
					 memset(tmp, 0, 16);
					 for (int i = 0;i < 4;i++) {
						 uint32_t sum = 0;
						 for (int j = 0;j < 4;j++) sum += aa[i * 4 + j] * bb[i * 4 + j];
						 tmp[2 * i] = (sum >> 8) & 0xFF;
						 tmp[2 * i + 1] = sum & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 break;
				 }
				 case 37: { // vmsummbm: sum bytes signed*unsigned into halfword
					 memset(tmp, 0, 16);
					 for (int i = 0;i < 4;i++) {
						 int32_t sum = 0;
						 for (int j = 0;j < 4;j++) sum += int8_t(aa[i * 4 + j]) * bb[i * 4 + j];
						 int16_t res = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 tmp[2 * i] = res >> 8;
						 tmp[2 * i + 1] = res & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 break;
				 }
				 case 38: {// vmsumuhm: sum unsigned halfwords into word lanes
					 for (int i = 0;i < 4;i++) {
						 uint32_t sum = 0;
						 for (int j = 0;j < 2;j++) {
							 uint16_t v = (uint16_t(aa[4 * i + 2 * j]) << 8) | aa[4 * i + 2 * j + 1];
							 sum += v * bb[4 * i + 2 * j + 1]; // assume b holds multipliers
						 }
						 memcpy(&r[4 * i], &sum, 4);
					 }
					 break;
				 }
				 case 39: { // vmsumuhs: sum unsigned halfwords into signed halfwords
					 for (int i = 0;i < 8;i++) {
						 uint16_t va = (uint16_t(aa[2 * i]) << 8) | aa[2 * i + 1];
						 uint16_t vb = (uint16_t(bb[2 * i]) << 8) | bb[2 * i + 1];
						 uint32_t sum = va * vb;
						 int16_t res = (int16_t)((sum >> 16) & 0xFFFF);
						 tmp[2 * i] = res >> 8; tmp[2 * i + 1] = res & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 break;
				 }
				 case 40: {// vmsumshm: sum signed halfwords into word
					 for (int i = 0;i < 4;i++) {
						 int32_t sum = 0;
						 for (int j = 0;j < 2;j++) {
							 int16_t v = (int16_t(aa[4 * i + 2 * j]) << 8) | aa[4 * i + 2 * j + 1];
							 sum += v;
						 }
						 memcpy(&r[4 * i], &sum, 4);
					 }
					 break;
				 }
				 case 41: {// vmsumshs: sum signed halfwords into halfword
					 for (int i = 0;i < 8;i++) {
						 int16_t va = (int16_t(aa[2 * i]) << 8) | aa[2 * i + 1];
						 int16_t vb = (int16_t(bb[2 * i]) << 8) | bb[2 * i + 1];
						 int32_t sum = va + vb;
						 int16_t res = sum<INT16_MIN ? INT16_MIN : sum>INT16_MAX ? INT16_MAX : sum;
						 tmp[2 * i] = res >> 8; tmp[2 * i + 1] = res & 0xFF;
					 }
					 memcpy(r, tmp, 16);
					 break;
				 }
				 case 42: { // vsel
					 for (int i = 0;i < 16;i++) r[i] = (aa[i] & 0x80) ? bb[i] : aa[i];
					 break;
				 }
				 case 43: {// vperm
					 for (int i = 0;i < 16;i++) {
						 uint8_t idx = bb[i] & 0x0F;
						 tmp[i] = aa[idx];
					 }
					 memcpy(r, tmp, 16);
					 break;
				 }
				 case 44: {// vsldoi
					 {
						 int sh = ExtractBits(instr, 16, 20) & 0xF;
						 for (int i = 0;i < 16;i++) {
							 int src = i + sh;
							 r[i] = src < 16 ? aa[src] : bb[src - 16];
						 }
					 }
					 break;
				 }
				 case 46: // vmaddfp
				 {
					 float* fa = reinterpret_cast<float*>(aa);
					 float* fb = reinterpret_cast<float*>(bb);
					 float* fr = reinterpret_cast<float*>(&VACC);
					 for (int i = 0;i < 4;i++) fr[i] = fr[i] + fa[i] * fb[i];
					 VPR[rt] = VACC;

					 break;
				 }
				 case 47: // vnmsubfp
				 {
					 float* fa = reinterpret_cast<float*>(aa);
					 float* fb = reinterpret_cast<float*>(bb);
					 float* fr = reinterpret_cast<float*>(&VACC);
					 for (int i = 0;i < 4;i++) fr[i] = -(fa[i] * fb[i]) - fr[i];
					 VPR[rt] = VACC;

					 break;
				 }
				 default: {
					 //PPC_DECODER_MISS;
					 std::cout << " Default method for case 4" << std::endl;
				 }
				 }
		}
		break;
	}
	case 5: {
		// 128-bit permute
		uint32_t sub = (ExtractBits(instr, 22, 22) << 5) | (ExtractBits(instr, 27, 27) << 0);
		uint32_t rt = ExtractBits(instr, 6, 10),
			ra = ExtractBits(instr, 11, 15),
			rb = ExtractBits(instr, 16, 20);
		uint8_t* A = reinterpret_cast<uint8_t*>(&VPR[ra]);
		uint8_t* B = reinterpret_cast<uint8_t*>(&VPR[rb]);
		uint8_t* R = reinterpret_cast<uint8_t*>(&VPR[rt]);
		float* FA = reinterpret_cast<float*>(A),
			* FB = reinterpret_cast<float*>(B),
			* FR = reinterpret_cast<float*>(&VPR[rt]);
		double* DA = reinterpret_cast<double*>(A),
			* DB = reinterpret_cast<double*>(B),
			* DR = reinterpret_cast<double*>(&VPR[rt]);
		uint64_t acc = VACC;
		switch (sub) {
		case 0: {// vperm128 – igual que vperm pero en 16 bytes
			for (int i = 0;i < 16;i++) {
				uint8_t idx = B[i] & 0x0F;
				R[i] = A[idx];
			}
			break;
		}
		default: {
			//PPC_DECODER_MISS;
		}
		}
		sub = (ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0);
		switch (sub) {
		case  1: {// vaddfp128 : 2 doubles
			for (int i = 0;i < 2;i++) DR[i] = DA[i] + DB[i];
			break;
		}
		case 5: {// vsubfp128
			for (int i = 0;i < 2;i++) DR[i] = DA[i] - DB[i];
			break;
		}
		case 9: { // vmulfp128
			for (int i = 0;i < 2;i++) DR[i] = DA[i] * DB[i];
			break;
		}
		case 13: {// vmaddfp128
			for (int i = 0;i < 2;i++) {
				double a = DA[i], b = DB[i];
				double c = reinterpret_cast<double*>(&acc)[i];
				DR[i] = c + a * b;
			}
			VACC = VPR[rt];
			break;
		}
		case 17: {// vmaddcfp128 (c = a*b + c)
			for (int i = 0;i < 2;i++) {
				double a = DA[i], b = DB[i];
				double c = reinterpret_cast<double*>(&acc)[i];
				DR[i] = a * b + c;
			}
			VACC = VPR[rt];
			break;
		}
		case 21: {// vnmsubfp128
			for (int i = 0;i < 2;i++) {
				double a = DA[i], b = DB[i];
				double c = reinterpret_cast<double*>(&acc)[i];
				DR[i] = -(a * b) - c;
			}
			VACC = VPR[rt];
			break;
		}
		case 25: { // vmsum3fp128: suma de 3 productos en SP lanes
			float sum = FA[0] * FB[0] + FA[1] * FB[1] + FA[2] * FB[2];
			for (int i = 0;i < 4;i++) FR[i] = sum;
			break;
		}
		case 29: { // vmsum4fp128
			float sum = 0;
			for (int i = 0;i < 4;i++) sum += FA[i] * FB[i];
			for (int i = 0;i < 4;i++) FR[i] = sum;
			break;
		}
		case 32: { // vpkshss128
			for (int i = 0;i < 8;i++) {
				int16_t v = (int16_t(A[2 * i]) << 8) | A[2 * i + 1];
				int16_t sat = v<INT16_MIN ? INT16_MIN : v>INT16_MAX ? INT16_MAX : v;
				R[2 * i] = sat >> 8; R[2 * i + 1] = sat & 0xFF;
			}
			break;
		}
		case 33: {// vand128
			for (int i = 0;i < 16;i++) R[i] = A[i] & B[i];
			break;
		}
		case 36: { // vpkshus128
			for (int i = 0;i < 8;i++) {
				uint16_t v = (uint16_t(A[2 * i]) << 8) | A[2 * i + 1];
				uint16_t sat = v > UINT16_MAX ? UINT16_MAX : v;
				R[2 * i] = sat >> 8; R[2 * i + 1] = sat & 0xFF;
			}
			break;
		}
		case 37: {// vandc128
			for (int i = 0;i < 16;i++) R[i] = A[i] & ~B[i];
			break;
		}
		case 40: {// vpkswss128
			for (int i = 0;i < 8;i++) {
				int16_t v = (int16_t(A[2 * i]) << 8) | A[2 * i + 1];
				int16_t sat = v<INT16_MIN ? INT16_MIN : v>INT16_MAX ? INT16_MAX : v;
				R[2 * i] = sat >> 8; R[2 * i + 1] = sat & 0xFF;
			}
			break;
		}
		case 41: {// vnor128
			for (int i = 0;i < 16;i++) R[i] = ~(A[i] | B[i]);
			break;
		case 44: // vpkswus128
			for (int i = 0;i < 8;i++) {
				int16_t v = (int16_t(A[2 * i]) << 8) | A[2 * i + 1];
				uint16_t sat = v<0 ? 0 : v>UINT16_MAX ? UINT16_MAX : v;
				R[2 * i] = sat >> 8; R[2 * i + 1] = sat & 0xFF;
			}
			break;
		}
		case 45: {// vor128
			for (int i = 0;i < 16;i++) R[i] = A[i] | B[i];
			break;
		}
		case 48: {// vpkuhum128
			for (int i = 0;i < 8;i++) {
				uint16_t v = (uint16_t(A[2 * i]) << 8) | A[2 * i + 1];
				R[2 * i] = v >> 8; R[2 * i + 1] = v & 0xFF;
			}
			break;
		}
		case 49: {// vxor128
			for (int i = 0;i < 16;i++) R[i] = A[i] ^ B[i];
			break;
		}
		case 52: {// vpkuhus128
			for (int i = 0;i < 8;i++) {
				uint16_t v = (uint16_t(A[2 * i]) << 8) | A[2 * i + 1];
				R[2 * i] = v >> 8; R[2 * i + 1] = v & 0xFF;
			}
			break;
		}
		case 53: {// vsel128
			for (int i = 0;i < 16;i++) R[i] = (A[i] & 0x80) ? B[i] : A[i];
			break;
		}
		case 56: {// vpkuwum128
			for (int i = 0;i < 4;i++) {
				uint32_t v = *reinterpret_cast<uint32_t*>(A + 4 * i);
				memcpy(R + 4 * i, &v, 4);
			}
			break;
		}
		case 57: {// vslo128
			for (int i = 0;i < 16;i++) { uint8_t sh = B[i] & 0x7; R[i] = A[i] << sh; }
			break;
		}
		case 60: {// vpkuwus128
			for (int i = 0;i < 4;i++) {
				uint32_t v = *reinterpret_cast<uint32_t*>(A + 4 * i);
				uint32_t sat = v > UINT32_MAX ? UINT32_MAX : v;
				memcpy(R + 4 * i, &sat, 4);
			}
			break;
		}
		case 61: {// vsro128
			for (int i = 0;i < 16;i++) { uint8_t sh = B[i] & 0x7; R[i] = A[i] >> sh; }
			break;
		}
			   break;
		}
	}
	case 6: {
		rt = ExtractBits(instr, 6, 10);
		ra = ExtractBits(instr, 11, 15);
		rb = ExtractBits(instr, 16, 20);
		uint8_t* A = (uint8_t*)&VPR[ra];
		uint8_t* B = (uint8_t*)&VPR[rb];
		uint8_t* R = (uint8_t*)&VPR[rt];
		float* FA = (float*)A;
		float* FB = (float*)B;
		float* FR = (float*)&VPR[rt];
		double* DA = (double*)A;
		double* DB = (double*)B;
		double* DR = (double*)&VPR[rt];
		uint32_t tmpw;
		switch ((ExtractBits(instr, 21, 22) << 5) | (ExtractBits(instr, 26, 27) << 0)) {
		case 33: { // vpermwi128
			uint32_t sh = ExtractBits(instr, 23, 28) & 3;
			for (int w = 0;w < 4;w++) {
				uint8_t* src = A + 4 * ((w + sh) & 3);
				memcpy(R + 4 * w, src, 4);
			}
			break;
		}
		default: { std::cout << " Default method for case 6 ext 33 - vpermwi128" << std::endl; }
		}
		switch ((ExtractBits(instr, 21, 23) << 4) | (ExtractBits(instr, 26, 27) << 0)) {
		case 97: { // vpkd3d128: pack high double lanes
			memcpy(R, A + 8, 8);
			memcpy(R + 8, B + 8, 8);
			break;
		}
		case 113: { // vrlimi128: rotate bytes by imm bits16-20
			uint32_t sh = ExtractBits(instr, 16, 20) & 0xF;
			for (int i = 0;i < 16;i++) {
				R[i] = A[(i + sh) & 0xF];
			}
			break;
		}
		default: {
			std::cout << " Default method for case 6 ext 97 y 113 - vpkd3d128 or vrlimi128" << std::endl;
			//PPC_DECODER_MISS;	
		}
			   break;
		}
		switch ((ExtractBits(instr, 21, 27) << 0)) {
		case 35: {// vcfpsxws128
			for (int i = 0;i < 4;i++) {
				int32_t v = lrintf(FA[i]);
				memcpy(R + 4 * i, &v, 4);
			}
			break;
		}
		case 39: { // vcfpuxws128
			for (int i = 0;i < 4;i++) {
				uint32_t v = (uint32_t)floorf(FA[i]);
				memcpy(R + 4 * i, &v, 4);
			}
			break;
		}
		case 43: {// vcsxwfp128
			for (int i = 0;i < 4;i++) {
				int32_t v = *reinterpret_cast<int32_t*>(A + 4 * i);
				FR[i] = float(v);
			}
			break;
		}
		case 47: {// vcuxwfp128
			for (int i = 0;i < 4;i++) {
				uint32_t v = *reinterpret_cast<uint32_t*>(A + 4 * i);
				FR[i] = float(v);
			}
			break;
		}
		case 51: {// vrfim128
			for (int i = 0;i < 4;i++) FR[i] = floorf(FA[i]);
			break;
		}
		case 55: {// vrfin128
			for (int i = 0;i < 4;i++) FR[i] = floorf(FA[i]);
			break;
		}
		case 59: {// vrfip128
			for (int i = 0;i < 4;i++) FR[i] = floorf(FA[i]);
			break;
		}
		case 63: {// vrfiz128
			for (int i = 0;i < 4;i++) FR[i] = roundf(FA[i]);
			break;
		}
		case 99: {// vrefp128
			for (int i = 0;i < 4;i++) FR[i] = FA[i] < 0 ? -FB[i] : FB[i];
			break;
		}
		case 103: { // vrsqrtefp128
			for (int i = 0;i < 4;i++) FR[i] = 1.0f / sqrtf(FA[i]);
			break;
		}
		case 107: {// vexptefp128
			for (int i = 0;i < 4;i++) FR[i] = expf(FA[i]);
			break;
		}
		case 111: {// vlogefp128
			for (int i = 0;i < 4;i++) FR[i] = logf(FA[i]);
			break;
		}
		case 115: { // vspltw128
			uint32_t val = *reinterpret_cast<uint32_t*>(A);
			for (int i = 0;i < 4;i++) memcpy(R + 4 * i, &val, 4);
			break;
		}
		case 119: { // vspltisw128
			uint32_t sel = ExtractBits(instr, 11, 15) & 3;
			uint32_t val = *reinterpret_cast<uint32_t*>(A + 4 * sel);
			for (int i = 0;i < 4;i++) memcpy(R + 4 * i, &val, 4);
			break;
		}
		case 127: {// vupkd3d128
			memcpy(R, A, 8);
			memcpy(R + 8, B, 8);
			break;
		}
		default: {//PPC_DECODER_MISS;
			std::cout << " Default method for case 6 ext 35 to 127" << std::endl;
		}
		}
		switch ((ExtractBits(instr, 22, 24) << 3) | (ExtractBits(instr, 27, 27) << 0)) {

		case  0: { // vcmpeqfp128
			for (int i = 0;i < 2;i++) {
				bool eq = DA[i] == DB[i];
				uint8_t v = eq ? 0xFF : 0x00;
				for (int j = 0;j < 8;j++) R[8 * i + j] = v;
			}
			break;
		}
		case  8: {// vcmpgefp128
			for (int i = 0;i < 2;i++) {
				bool ge = DA[i] >= DB[i];
				uint8_t v = ge ? 0xFF : 0x00;
				for (int j = 0;j < 8;j++) R[8 * i + j] = v;
			}
			break;
		}
		case 16: { // vcmpgtfp128
			for (int i = 0;i < 2;i++) {
				bool gt = DA[i] > DB[i];
				uint8_t v = gt ? 0xFF : 0x00;
				for (int j = 0;j < 8;j++) R[8 * i + j] = v;
			}
			break;
		}
		case 24: { // vcmpbfp128
			for (int i = 0;i < 2;i++) {
				uint64_t wa = *reinterpret_cast<uint64_t*>(A + 8 * i);
				uint64_t wb = *reinterpret_cast<uint64_t*>(B + 8 * i);
				bool eq = wa == wb;
				uint8_t v = eq ? 0xFF : 0x00;
				for (int j = 0;j < 8;j++) R[8 * i + j] = v;
			}
			break;
		}
		case 32: {// vcmpequw128
			for (int i = 0;i < 4;i++) {
				uint32_t va = *reinterpret_cast<uint32_t*>(A + 4 * i);
				uint32_t vb = *reinterpret_cast<uint32_t*>(B + 4 * i);
				bool eq = va == vb;
				uint8_t v = eq ? 0xFF : 0x00;
				for (int j = 0;j < 4;j++) R[4 * i + j] = v;
			}
			break;
		}
		default: {//PPC_DECODER_MISS;
			std::cout << " Default method for case 6 ext 0, 8, 16, 24, 32" << std::endl;
		}
		}
		switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {

		case  5: { // vrlw128
			for (int i = 0;i < 4;i++) {
				uint32_t va = *reinterpret_cast<uint32_t*>(A + 4 * i);
				uint32_t sh = *reinterpret_cast<uint32_t*>(B + 4 * i) & 0x1F;
				tmpw = (va << sh) | (va >> (32 - sh));
				memcpy(R + 4 * i, &tmpw, 4);
			}
			break;
		}
		case 13: { // vslw128
			for (int i = 0;i < 4;i++) {
				uint32_t va = *reinterpret_cast<uint32_t*>(A + 4 * i);
				uint32_t sh = *reinterpret_cast<uint32_t*>(B + 4 * i) & 0x1F;
				tmpw = va << sh;
				memcpy(R + 4 * i, &tmpw, 4);
			}
			break;
		}
		case 21: {// vsraw128
			for (int i = 0;i < 4;i++) {
				int32_t va = *reinterpret_cast<int32_t*>(A + 4 * i);
				uint32_t sh = *reinterpret_cast<uint32_t*>(B + 4 * i) & 0x1F;
				int32_t vr = va >> sh;
				memcpy(R + 4 * i, &vr, 4);
			}
			break;
		}
		case 29: {// vsrw128
			for (int i = 0;i < 4;i++) {
				uint32_t va = *reinterpret_cast<uint32_t*>(A + 4 * i);
				uint32_t sh = *reinterpret_cast<uint32_t*>(B + 4 * i) & 0x1F;
				tmpw = va >> sh;
				memcpy(R + 4 * i, &tmpw, 4);
			}
			break;
		}
		case 40: { // vmaxfp128
			for (int i = 0;i < 2;i++) DR[i] = DA[i] > DB[i] ? DA[i] : DB[i];
			break;
		}
		case 44: {// vminfp128
			for (int i = 0;i < 2;i++) DR[i] = DA[i] < DB[i] ? DA[i] : DB[i];
			break;
		}
		case 48: { // vmrghw128
			for (int i = 0;i < 2;i++) {
				uint32_t wa = *reinterpret_cast<uint32_t*>(A + 8 * i);
				uint32_t wb = *reinterpret_cast<uint32_t*>(B + 8 * i);
				uint32_t w0 = (wb & 0xFFFF0000) | (wa & 0x0000FFFF);
				uint32_t w1 = (wb & 0x0000FFFF) << 16 | (wa >> 16);
				memcpy(R + 8 * i, &w0, 4);
				memcpy(R + 8 * i + 4, &w1, 4);
			}
			break;
		}
		case 52: {// vmrglw128
			for (int i = 0;i < 2;i++) {
				uint32_t wa = *reinterpret_cast<uint32_t*>(A + 8 * i);
				uint32_t wb = *reinterpret_cast<uint32_t*>(B + 8 * i);
				uint32_t w0 = (wa & 0xFFFF0000) | (wb & 0x0000FFFF);
				uint32_t w1 = (wa & 0x0000FFFF) << 16 | (wb >> 16);
				memcpy(R + 8 * i, &w0, 4);
				memcpy(R + 8 * i + 4, &w1, 4);
			}
			break;
		}
		case 56: {// vupkhsb128
			for (int i = 0;i < 16;i++) {
				int8_t v = int8_t(A[i]);
				R[i] = uint8_t(v < 0 ? -v : v);
			}
			break;
		}
		case 60: { // vupklsb128
			for (int i = 0;i < 16;i++) {
				int8_t v = int8_t(A[i]);
				R[i] = uint8_t(v);
			}
			break;
		}
		default: {// PPC_DECODER_MISS;
			std::cout << " Default method for case 6 ext 5, 13, 21, 29, 40, 44, 48, 52, 60" << std::endl;
		}
		}
	}
	case 7: { // mulli rA, rS, SI
		uint32_t rt = ExtractBits(instr, 6, 10);
		uint32_t ra = ExtractBits(instr, 11, 15);
		int16_t si = instr & 0xFFFF;
		int32_t prod = int32_t(GPR[ra]) * si;
		GPR[rt] = uint32_t(prod);
		break;
	}
	case 8: { // subficx rA, rS, UI
		uint32_t rt = ExtractBits(instr, 6, 10);
		uint32_t ra = ExtractBits(instr, 11, 15);
		uint16_t ui = instr & 0xFFFF;
		uint32_t val = ui - GPR[ra];
		GPR[rt] = val;
		break;
	}
	case 9: { // stw rS, d(rA)
		u32 rS = (instr >> 21) & 0x1F;
		u32 rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		u32 base = (rA == 0 ? 0 : GPR[rA]);
		u32 addr = base + d;
		mmu->Write32(addr, GPR[rS]);
		LOG_INFO("CPU", "stw r%d to [0x%08X] = 0x%08X", rS, addr, GPR[rS]);

		break;
	}
	case 10: { // cmpli crfD, rA, UI
		uint32_t crfD = ExtractBits(instr, 6, 8);
		uint32_t ra = ExtractBits(instr, 11, 15);
		uint16_t ui = instr & 0xFFFF;
		bool lt = GPR[ra] < ui;
		bool gt = GPR[ra] > ui;
		bool eq = GPR[ra] == ui;
		CR.fields.cr0 = (lt ? 8 : 0) | (gt ? 4 : 0) | (eq ? 2 : 0) | 0;
		break;
	}
	case 11: { // cmpi crfD, rA, SI
		uint32_t crfD = ExtractBits(instr, 6, 8);
		uint32_t ra = ExtractBits(instr, 11, 15);
		int16_t si = instr & 0xFFFF;
		int32_t v = int32_t(GPR[ra]) - si;
		bool lt = v < 0;
		bool gt = v > 0;
		bool eq = v == 0;
		CR.fields.cr0 = (lt ? 8 : 0) | (gt ? 4 : 0) | (eq ? 2 : 0) | 0;
		break;
	}
	case 12: { // addic rA, rS, SI
		uint32_t rt = ExtractBits(instr, 6, 10);
		uint32_t ra = ExtractBits(instr, 11, 15);
		int16_t si = instr & 0xFFFF;
		uint32_t sum = GPR[ra] + uint32_t(si);
		GPR[rt] = sum;
		XER = (sum < GPR[ra]) ? (XER | 0x02) : (XER & ~0x02);
	}
	case 13: { // addicx rA, rS, SI with carry
		uint32_t rt = ExtractBits(instr, 6, 10);
		uint32_t ra = ExtractBits(instr, 11, 15);
		int16_t si = instr & 0xFFFF;
		uint32_t carry = (XER >> 2) & 1;
		uint64_t sum = uint64_t(GPR[ra]) + si + carry;
		GPR[rt] = uint32_t(sum);
		XER = (sum >> 32) ? (XER | 0x02) : (XER & ~0x02);
	}
	case 14: { // addi
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t imm = (int16_t)(instr & 0xFFFF);
		GPR[rD] = (rA ? GPR[rA] : 0) + imm;
		std::cout << "Executing addi: r" << rD << " = r" << rA << " + " << imm
			<< " = 0x" << std::hex << GPR[rD] << std::dec << "\n";
		break;
	}
	case 15: { // lis
		uint32_t rD = (instr >> 21) & 0x1F;
		int16_t imm = (int16_t)(instr & 0xFFFF);
		GPR[rD] = ((uint64_t)imm) << 16;
		std::cout << "Executing lis: r" << rD << " = 0x" << std::hex << GPR[rD] << std::dec << "\n";
		break;
	}
	case 16: { // bc BO,BI,BD
		uint32_t bo = ExtractBits(instr, 6, 10);
		uint32_t bi = ExtractBits(instr, 11, 15);
		int32_t  raw = instr & 0xFFFC;          // BD field includes low two bits = 0
		// sign-extend 16→32
		int32_t  simm = (raw & 0x8000) ? (raw | 0xFFFF0000) : raw;
		// shift-left 2
		simm <<= 2;

		bool ctr_ok = ((bo & 0x04) == 0) || (CTR != 0);
		bool cond_ok = ((bo & 0x20) == 0) || (((CR.value >> ((7 - bi) * 4)) & 0x8) != 0);
		bool do_branch = (bo & 0x10) || (ctr_ok && cond_ok);

		// CTR update if requested
		if (bo & 0x02) --CTR;

		if (do_branch) {
			uint32_t target = (PC + simm) & ~0x3u;
			PC = target;
		}
		else {
			PC += 4;
		}
		break;
	}
	case 17: { // sc
		TriggerException(0x200);
		break;
	}
	case 18: { // b[?] LI,AA,LK
		bool absolute = (instr & 0x00000002) != 0;
		bool link = (instr & 0x00000001) != 0;

		// Extraer offset de 26 bits y sign-extend:
		int32_t raw26 = instr & 0x03FFFFFC;
		if (raw26 & 0x02000000) raw26 |= 0xFC000000;
		// el <<2 ya está en raw26 (low bits son 00).
		int32_t simm = raw26;

		uint32_t nextPC = absolute
			? uint32_t(simm)     // dirección absoluta si AA=1
			: (PC + simm);       // relativa

		// Si link, guardar en LR la dirección de retorno (PC+4)
		if (link) {
			LR = PC;             // PC ya adelantado en Step()
		}

		PC = nextPC & ~0x3u;     // alinear
		break;
	}
	case 19: {
		uint32_t xo = (instr >> 1) & 0x3FF; // Extended opcode
		uint32_t sub = ExtractBits(instr, 21, 30);
		if (xo == 50) { // rfi
			std::cout << "Executing rfi, restoring PC=0x" << std::hex << SRR0 << ", MSR=0x" << SRR1 << std::dec << "\n";
			PC = SRR0; // Restaurar PC
			MSR = SRR1; // Restaurar MSR
		}
		else {
			std::cerr << "[WARN] Unknown case19 sub-opcode: " << xo << "\n";
			std::cerr << "Unknown miscellaneous instruction: 0x" << std::hex << instr << std::dec << "\n";
			//PC += 4;
		}

		if (sub == 0) { // mcrf
			HandleCRInstructions(instr, sub);
		}
		else if (sub == 16) { // bclr
			HandleBranchConditional(instr, false);
		}
		else if (sub == 528) { // bcctr
			HandleBranchConditional(instr, true);
		}
		else if (sub == 150) { // isync
			HandleISync();
		}
		else if (sub == 33 || sub == 257 || sub == 289 || sub == 193 || sub == 225 || sub == 129 || sub == 417 || sub == 449 || sub == 76 || sub == 0x4C000064) {
			HandleCRInstructions(instr, sub);
		}
		else {
			std::cout << "[WARN] Unknown case19 sub-opcode: " << sub << std::endl;
			std::cerr << "Unknown miscellaneous instruction: 0x" << std::hex << instr << std::dec << "\n";
		}
		break;
	}
	case 20: { // rlwimix
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint32_t sh = ExtractBits(instr, 16, 20);
		uint32_t mb = ExtractBits(instr, 21, 25);
		uint32_t me = ExtractBits(instr, 26, 30);
		uint32_t val = GPR[ra];
		uint32_t res = (val << sh) | (val >> (32 - sh));
		uint32_t mask = MaskFromMBME(mb, me);
		GPR[rt] = (res & mask) | (GPR[rt] & ~mask);
		break;
	}
	case 21: { // rlwinmx
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint32_t sh = ExtractBits(instr, 16, 20);
		uint32_t mb = ExtractBits(instr, 21, 25);
		uint32_t me = ExtractBits(instr, 26, 30);
		uint32_t val = GPR[ra];
		uint32_t res = (val << sh) | (val >> (32 - sh));
		uint32_t mask = MaskFromMBME(mb, me);
		GPR[rt] = (res & mask);
		break;
	}
	case 22: { // LHZ
		u32 rD = (instr >> 21) & 0x1F;
		u32 rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		u32 ea = (rA == 0 ? 0 : GPR[rA]) + d;
		GPR[rD] = mmu->Read16(ea);
		break;
	}
	case 23: { // rlwnmx
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15), rb = ExtractBits(instr, 16, 20);
		uint32_t sh = ExtractBits(instr, 21, 25);
		uint32_t mb = ExtractBits(instr, 26, 30);
		uint32_t val = GPR[ra] ^ GPR[rb];
		uint32_t res = (val << sh) | (val >> (32 - sh));
		uint32_t mask = MaskFromMBME(mb, mb + sh); // rlwnmx mask semantics
		GPR[rt] = (res & mask);
		break;
	}
	case 24: { // ori
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t imm = instr & 0xFFFF;
		GPR[rA] = GPR[rS] | imm;
		std::cout << "Executing ori: r" << rA << " = r" << rS << " | 0x" << imm
			<< " = 0x" << std::hex << GPR[rA] << std::dec << "\n";
		break;
	}
	case 25: { // oris
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint16_t imm = instr & 0xFFFF;
		GPR[ra] = GPR[rs] | (uint32_t(imm) << 16);
		break;
	}
	case 26: { // xori
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint16_t imm = instr & 0xFFFF;
		GPR[ra] = GPR[rs] ^ imm;
		break;
	}
	case 27: { // xoris
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint16_t imm = instr & 0xFFFF;
		GPR[ra] = GPR[rs] ^ (uint32_t(imm) << 16);
		break;
	}
	case 28: { // andix
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint16_t imm = instr & 0xFFFF;
		GPR[ra] &= ~uint32_t(imm);
		break;
	}
	case 29: { // andisx
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		uint16_t imm = instr & 0xFFFF;
		GPR[ra] &= ~(uint32_t(imm) << 16);
		break;
	}
	case 30: {
		switch (ExtractBits(instr, 27, 29)) {
		case 0: { // rldiclx
			uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
			uint32_t sh = ExtractBits(instr, 21, 25);
			uint32_t val = GPR[ra];
			GPR[rt] = (val << sh);
			break;
		}
		case 1: { // rldicrx
			uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
			uint32_t sh = ExtractBits(instr, 21, 25);
			uint32_t val = GPR[ra];
			GPR[rt] = (val >> sh);
			break;
		}
		case 2: { // rldicx
			uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
			uint32_t sh = ExtractBits(instr, 21, 25);
			uint32_t val = GPR[ra];
			GPR[rt] = (val << sh) | (val >> (32 - sh));
			break;
		}
		case 3: { // rldimix
			uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
			uint32_t sh = ExtractBits(instr, 21, 25);
			uint32_t mb = ExtractBits(instr, 16, 20);
			uint32_t val = GPR[ra];
			uint32_t rot = (val << sh) | (val >> (32 - sh));
			uint32_t mask = MaskFromMBME(mb, mb + sh);
			GPR[rt] = (rot & mask) | (GPR[rt] & ~mask);
			break;
		}
			  switch (ExtractBits(instr, 27, 30)) {
			  case 8: { // rldclx
				  uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
				  uint32_t mb = ExtractBits(instr, 21, 25);
				  uint32_t val = GPR[ra];
				  uint32_t mask = MaskFromMBME(mb, 31);
				  GPR[rt] = val & mask;
				  break;
			  }
			  case 9: { // rldcrx
				  uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
				  uint32_t me = ExtractBits(instr, 21, 25);
				  uint32_t val = GPR[ra];
				  uint32_t mask = MaskFromMBME(0, me);
				  GPR[rt] = val & mask;
				  break;
			  }
			  }
		}
		// PPC_DECODER_MISS;
	}
	case 31: {
		uint32_t sub21_29 = ExtractBits(instr, 21, 29);
		uint32_t sub21_30 = ExtractBits(instr, 21, 30);
		uint32_t sub6_10_21_30 = (ExtractBits(instr, 6, 10) << 20) | sub21_30;
		rt = ExtractBits(instr, 6, 10);
		ra = ExtractBits(instr, 11, 15);
		rb = ExtractBits(instr, 16, 20);
		uint32_t XO = ExtractBits(instr, 21, 30);

		// primer grupo: sub21_29
		switch (sub21_29) {
		case 413: { // sradix
			int32_t sh = ExtractBits(instr, 16, 20);
			int32_t va = int32_t(GPR[ra]);
			GPR[rt] = uint32_t(va >> sh);
		} break;
		}

		// segundo grupo: sub21_30
		switch (sub21_30) {
		case   0: { // cmp
			uint32_t rb2 = ExtractBits(instr, 16, 20);
			int32_t diff = int32_t(GPR[ra]) - int32_t(GPR[rb2]);
			CR.fields.cr0 = (diff < 0 ? 8 : 0) | (diff > 0 ? 4 : 0) | (diff == 0 ? 2 : 0);
		} break;
		case   4: { // tw
			uint32_t tocr = ExtractBits(instr, 21, 25);
			uint32_t tbr = ExtractBits(instr, 16, 20);
			int16_t simm = instr & 0xFFFF;
			int32_t v = int32_t(GPR[tbr]) - simm;
			bool trap = (tocr & 4 && v == 0) || (tocr & 8 && v > 0) || (tocr & 2 && v < 0);
			if (trap) TriggerTrap();
		} break;
		case   6: { // lvsl
			//uint32_t rt2 = rt;
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR = LoadVectorShiftLeft(addr);
		} break;
		case   7: { // lvebx
			uint32_t rt2 = rt;
			uint32_t addr = GPR[ra] + GPR[rb];
			uint8_t b = mmu->Read8(addr);
			// splat byte across vector
			uint8_t* R = (uint8_t*)&VPR[rt2];
			for (int i = 0;i < 16;i++) R[i] = b;
		} break;
		case  19: { // mfcr
			GPR[rt] = CR.value;
		} break;
		case  20: { // lwarx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
			reservation_addr = addr; reservation_valid = true;
		} break;
		case  21: { // ldx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
		} break;
		case  23: { // lwzx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
		} break;
		case  24: { // slwx
			uint32_t sh = GPR[rb] & 0x1F;
			GPR[rt] = GPR[ra] << sh;
		} break;
		case  26: { // cntlzwx						
			GPR[rt] = __lzcnt(GPR[ra]);//__builtin_clz(GPR[ra]); 
		} break;
		case  27: { // sldx
			uint32_t sh = GPR[rb] & 0x1F;
			GPR[rt] = (GPR[ra] << sh) | (GPR[ra] >> (32 - sh));
		} break;
		case  28: { // andx
			GPR[rt] = GPR[ra] & GPR[rb];
		} break;
		case  32: { // cmpl
			uint32_t rb2 = rb;
			uint32_t ua = GPR[ra], ub = GPR[rb2];
			bool lt = ua<ub, gt = ua>ub, eq = ua == ub;
			CR.fields.cr0 = (lt ? 8 : 0) | (gt ? 4 : 0) | (eq ? 2 : 0);
		} break;
		case  38: { // lvsr
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR = LoadVectorShiftRight(addr);
		} break;
		case  39: { // lvehx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint16_t h = mmu->Read16(addr);
			uint8_t* R = (uint8_t*)&VPR[rt];
			R[0] = h >> 8; R[1] = h; // low half
		} break;
		case  53: { // ldux
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
		} break;
		case  54: { // dcbst
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->DCACHE_Store(addr);
		} break;
		case  55: { // lwzux
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
			GPR[ra] = addr;
		} break;
		case  58: { // cntlzdx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = __lzcnt(mmu->Read32(addr));//__builtin_clz(mmu->Read32(addr));
		} break;
		case  60: { // andcx
			GPR[rt] = GPR[ra] & ~GPR[rb];
		} break;
		case  68: { // td
			TriggerException(PPU_EX_DATASTOR);
		} break;
		case  71: { // lvewx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t w = mmu->Read32(addr);
			uint8_t* R = (uint8_t*)&VPR[rt];
			memcpy(R, &w, 4);
		} break;
		case  83: { // mfmsr
			GPR[rt] = MSR;
		} break;
		case  84: { // ldarx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
			reservation_addr = addr; reservation_valid = true;
		} break;
		case  86: { // dcbf
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->DCACHE_Flush(addr);
		} break;
		case  87: { // lbzx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read8(addr);
		} break;
		case 103: { // lvx
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->Read64(addr);
		} break;
		case 119: { // lbzux
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read8(addr);
			GPR[ra] = addr;
		} break;
		case 124: { // norx
			GPR[rt] = ~(GPR[ra] | GPR[rb]);
		} break;
		case 135: { // stvebx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint8_t* R = (uint8_t*)&VPR[rt];
			mmu->Write8(addr, R[0]);
		} break;
		case 144: { // mtcrf
			uint32_t mask = ExtractBits(instr, 6, 10) << ((7 - ExtractBits(instr, 11, 15)) * 4);
			CR.value = (CR.value & ~mask) | (GPR[rt] & mask);
		} break;
		case 146: { // mtmsr
			MSR = GPR[rt];
		} break;
		case 149: { // stdx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, (uint64_t(GPR[rt + 1]) << 32) | GPR[rt]);
		} break;
		case 150: { // stwcx
			if (reservation_valid && reservation_addr == (GPR[ra] + GPR[rb])) {
				mmu->Write32(reservation_addr, GPR[rt]);
				XER |= 0x200; // success
			}
			else {
				XER &= ~0x200; // fail
			}
			reservation_valid = false;
		} break;
		case 151: { // stwx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write32(addr, GPR[rt]);
		} break;
		case 167: { // stvehx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint8_t* R = (uint8_t*)&VPR[rt];
			uint16_t h = (R[0] << 8) | R[1];
			mmu->Write16(addr, h);
		} break;
		case 178: { // mtmsrd
			MSR = GPR[rt];
		} break;
		case 181: { // stdux
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write32(addr, GPR[rt]);
			GPR[ra] = addr;
		} break;
		case 183: { // stwux
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write32(addr, GPR[rt]);
			GPR[ra] = addr;
		} break;
		case 199: { // stvewx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
		} break;
		case 214: { // stdcx
			if (reservation_valid && reservation_addr == (GPR[ra] + GPR[rb])) {
				mmu->Write64(reservation_addr, (uint64_t(GPR[rt + 1]) << 32) | GPR[rt]);
				XER |= 0x200;
			}
			else XER &= ~0x200;
			reservation_valid = false;
		} break;
		case 215: { // stbx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint8_t val = GPR[rt] & 0xFF;
			mmu->Write8(addr, val);
		} break;
		case 231: { // stvx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
		} break;
		case 246: { // dcbtst
			// no-op
		} break;
		case 247: { // stbux
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write8(addr, GPR[rt] & 0xFF);
			GPR[ra] = addr;
		} break;
		case 278: { // dcbt
			// no-op
		} break;
		case 279: { // lhzx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read16(addr);
		} break;
		case 284: { // eqvx
			GPR[rt] = ~(GPR[ra] ^ GPR[rb]);
		} break;
		case 311: { // lhzux
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read16(addr);
			GPR[ra] = addr;
		} break;
		case 316: { // xorx
			GPR[rt] = GPR[ra] ^ GPR[rb];
		} break;
		case 339: { // mfspr
			GPR[rt] = SPR[ExtractBits(instr, 11, 15)];
		} break;
		case 341: { // lwax
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
			GPR[ra] = addr;
		} break;
		case 343: { // lhax
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read16(addr);
			GPR[ra] = addr;
		} break;
		case 359: { // lvxl
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->Read64(addr);
			GPR[ra] = addr;
		} break;
		case 371: { // mftb
			GPR[rt] = TBL;
		} break;
		case 373: { // lwaux
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read32(addr);
			GPR[ra] = addr + 4;
		} break;
		case 375: { // lhaux
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read16(addr);
			GPR[ra] = addr + 2;
		} break;
		case 407: { // sthx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write16(addr, GPR[rt] & 0xFFFF);
		} break;
		case 412: { // orcx
			GPR[rt] = GPR[ra] | ~GPR[rb];
		} break;
		case 439: { // sthux
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write16(addr, GPR[rt] & 0xFFFF);
			GPR[ra] = addr;
		} break;
		case 444: { // orx
			GPR[rt] = GPR[ra] | GPR[rb];
		} break;
		case 467: { // mtspr
			SPR[ExtractBits(instr, 11, 15)] = GPR[rt];
		} break;
		case 470: { // dcbi
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->DCACHE_CleanInvalidate(addr);
		} break;
		case 476: { // nandx
			GPR[rt] = ~(GPR[ra] & GPR[rb]);
		} break;
		case 487: { // stvxl
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
			GPR[ra] = addr;
		} break;
		case 512: { // mcrxr
			uint32_t crm = ExtractBits(instr, 11, 15);
			CR.value = (CR.value & ~(0xF << ((7 - crm) * 4))) | ((XER & 0xF) << ((7 - crm) * 4));
		} break;
		case 519: { // lvlx
			uint32_t addr = GPR[ra] + GPR[rb];

			VPR[rt] = mmu->LoadVectorLeft(addr);
		} break;
		case 532: { // ldbrx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t w = mmu->Read32(addr);
			GPR[rt] = invertirBytes(w);
		} break;
		case 533: { // lswx
			uint32_t addr = GPR[ra] + GPR[rb];
			GPR[rt] = mmu->Read16(addr) | (mmu->Read16(addr + 2) << 16);
		} break;
		case 534: { // lwbrx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t w = mmu->Read32(addr);
			GPR[rt] = invertirBytes(w);
		} break;
		case 535: { // lfsx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t w = mmu->Read32(addr);
			float f; memcpy(&f, &w, 4);
			FPR[rt] = f;
		} break;
		case 536: { // srwx
			uint32_t sh = GPR[rb] & 0x1F;
			GPR[rt] = GPR[ra] >> sh;
		} break;
		case 539: { // srdx
			int32_t sh = GPR[rb] & 0x1F;
			GPR[rt] = uint32_t(int32_t(GPR[ra]) >> sh);
		} break;
		case 551: { // lvrx
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->LoadVectorRight(addr);
		} break;
		case 567: { // lfsux
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t w = mmu->Read32(addr);
			float f; memcpy(&f, &w, 4);
			FPR[rt] = f;
			GPR[ra] = addr;
		} break;
		case 597: { // lswi
			uint32_t byteCount = ExtractBits(instr, 16, 21);
			uint32_t addr = GPR[ra] + GPR[rb];
			for (uint32_t i = 0;i < byteCount;i += 4) {
				GPR[rt + i / 4] = mmu->Read32(addr + i);
			}
		} break;
		case 598: { // sync
			__sync_synchronize();
		} break;
		case 599: { // lfdx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint64_t w = mmu->Read64(addr);
			double d; memcpy(&d, &w, 8);
			FPR[rt] = d;
		} break;
		case 631: { // lfdux
			uint32_t addr = GPR[ra] + GPR[rb];
			uint64_t w = mmu->Read64(addr);
			double d; memcpy(&d, &w, 8);
			FPR[rt] = d;
			GPR[ra] = addr;
		} break;
		case 647: { // stvlx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
			GPR[ra] = addr;
		} break;
		case 660: { // stdbrx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint64_t w = mmu->Read64(addr);
			mmu->Write64(addr, invertirBytes(w)); //swap64 _byteswap_uint64
		} break;
		case 661: { // stswx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write16(addr, GPR[rt] & 0xFFFF);
			mmu->Write16(addr + 2, (GPR[rt] >> 16) & 0xFFFF);
		} break;
		case 662: { // stwbrx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint32_t w = GPR[rt];
			mmu->Write32(addr, invertirBytes(w));//_byteswap_ulong(w)); //swap32
		} break;
		case 663: { // stfsx
			uint32_t addr = GPR[ra] + GPR[rb];
			float f = FPR[rt];
			uint32_t w; memcpy(&w, &f, 4);
			mmu->Write32(addr, w);
		} break;
		case 679: { // stvrx
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
		} break;
		case 695: { // stfsux
			uint32_t addr = GPR[ra] + GPR[rb];
			float f = FPR[rt];
			uint32_t w; memcpy(&w, &f, 4);
			mmu->Write32(addr, w);
			GPR[ra] = addr;
		} break;
		case 725: { // stswi
			uint32_t byteCount = ExtractBits(instr, 16, 21);
			uint32_t addr = GPR[ra] + GPR[rb];
			for (uint32_t i = 0;i < byteCount;i += 4) {
				uint32_t w = GPR[rt + i / 4];
				mmu->Write32(addr + i, w);
			}
		} break;
		case 727: { // stfdx
			uint32_t addr = GPR[ra] + GPR[rb];
			double d = FPR[rt];
			uint64_t w; memcpy(&w, &d, 8);
			mmu->Write64(addr, w);
		} break;
		case 759: { // stfdux
			uint32_t addr = GPR[ra] + GPR[rb];
			double d = FPR[rt];
			uint64_t w; memcpy(&w, &d, 8);
			mmu->Write64(addr, w);
			GPR[ra] = addr;
		} break;
		case 775: { // lvlxl
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->LoadVectorLeft(addr);
		} break;
		case 790: { // lhbrx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint16_t h = mmu->Read16(addr);
			GPR[rt] = _byteswap_ulong(h); //swap16
		} break;
		case 792: { // srawx
			int32_t sh = GPR[rb] & 0x1F;
			GPR[rt] = uint32_t(int32_t(GPR[ra]) >> sh);
		} break;
		case 794: { // sradx
			int32_t sh = GPR[rb] & 0x1F;
			int32_t va = int32_t(GPR[ra]);
			GPR[rt] = uint32_t(va >> sh);
		} break;
		case 807: { // lvrxl
			uint32_t addr = GPR[ra] + GPR[rb];
			VPR[rt] = mmu->LoadVectorRight(addr);
		} break;
		case 824: { // srawix
			int32_t sh = ExtractBits(instr, 16, 20) & 0x1F;
			int32_t va = int32_t(GPR[ra]);
			GPR[rt] = uint32_t(va >> sh);
		} break;
		case 854: { // eieio
			__sync_synchronize();
		} break;
		case 903: { // stvlxl
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
			GPR[ra] = addr;
		} break;
		case 918: { // sthbrx
			uint32_t addr = GPR[ra] + GPR[rb];
			uint16_t h = GPR[rt] & 0xFFFF;
			mmu->Write16(addr, invertirBytes(h));//__builtin_bswap16(h));
		} break;
		case 922: { // extshx
			uint32_t val = GPR[ra] & 0xFFFF;
			int16_t s = int16_t(val);
			GPR[rt] = uint32_t(s);
		} break;
		case 935: { // stvrxl
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->Write64(addr, VPR[rt]);
			GPR[ra] = addr;
		} break;
		case 954: { // extsbx
			uint8_t b = GPR[ra] & 0xFF;
			int8_t s = int8_t(b);
			GPR[rt] = uint32_t(s);
		} break;
		case 982: { // icbi
			uint32_t addr = GPR[ra] + GPR[rb];
			mmu->ICACHE_Invalidate(addr);
		} break;
		case 983: { // stfiwx
			uint32_t ba = ExtractBits(instr, 11, 15);
			uint32_t offset = ExtractBits(instr, 16, 20);
			uint32_t addr = GPR[ba] + offset;
			float f = FPR[rt];
			uint32_t w; memcpy(&w, &f, 4);
			mmu->Write32(addr, w);
		} break;
		case 986: { // extswx
			uint16_t w = mmu->Read16(GPR[ra] + GPR[rb]);
			int16_t s = int16_t(w);
			GPR[rt] = uint32_t(s);
		} break;
		default:
			//PPC_DECODER_MISS;
			std::cout << "Default method for 31 ext sub 21-30" << std::endl;
			break;
		}
		break;
	}
	case 32: { // lwz rt, d(ra)
		//uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		//int16_t D = instr & 0xFFFF;
		//GPR[rt] = mmu->Read32(GPR[ra] + D);

		uint32_t rt = (instr >> 21) & 0x1F, ra = (instr >> 16) & 0x1F;
		int16_t D = instr & 0xFFFF;
		uint32_t addr = (ra == 0 ? 0 : GPR[ra]) + D;
		if (addr % 4 != 0) {
			TriggerException(PPU_EX_ALIGNM);
			break;
		}
		GPR[rt] = mmu->Read32(addr + D);
		break;
	}
	case 33: { // lwzu
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		int16_t d = (int16_t)(instr & 0xFFFF);
		uint64_t ea = rA ? GPR[rA] + d : d;
		if (ea % 2 != 0) {
			std::cout << "Alignment exception: Unaligned access at address 0x" << std::hex << ea << std::dec << "\n";
			PC = 0x00000600; // Salta al vector de excepción de alineación
			SRR0 = PC; // Guarda PC para rfi
			SRR1 = MSR;    // Guarda MSR
			break;
		}
		try {
			mmu->Write16(ea, (uint16_t)(GPR[rS] & 0xFFFF));
			//PC += 4;
		}
		catch (const std::exception& e) {
			std::cout << "Exception triggered, vector=0x00000700, reason: " << e.what() << "\n";
			PC = 0x00000700;
		}
		break;
	}
	case 34: { // lbz
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		GPR[rt] = mmu->Read8(GPR[ra] + D);
		break;
	}
	case 35: { // lbzu
		uint32_t rs = ExtractBits(instr, 6, 10);  // source register with byte
		uint32_t ra = ExtractBits(instr, 11, 15);
		int16_t  d = instr & 0xFFFF;
		uint64_t addr = uint64_t(GPR[ra]) + uint64_t(int32_t(d));
		mmu->Write8(addr, uint8_t(GPR[rs] & 0xFF));
		break;
	}
	case 36: { // stw rs, d(ra)
		//uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		//int16_t D = instr & 0xFFFF;
		//mmu->Write32(GPR[ra] + D, GPR[rs]);
		uint32_t rs = (instr >> 21) & 0x1F;      // registro fuente
		uint32_t ra = (instr >> 16) & 0x1F;      // registro base
		int16_t  D = instr & 0xFFFF;            // desplazamiento con signo
		uint32_t addr = (ra == 0 ? 0 : GPR[ra]) + D;  // EA = RA + D
		try {
			mmu->Write32(addr, GPR[rs]);        // store word
		}
		catch (const std::exception& e) {
			TriggerException(PPU_EX_DATASTOR);
		}
		break;
	}
	case 37: { // stwu
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		mmu->Write32(addr, GPR[rs]);
		GPR[ra] = addr;
		break;
	}
	case 38: { // stb
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		int16_t d = (int16_t)(instr & 0xFFFF);
		uint64_t ea = rA ? GPR[rA] + d : d;
		try {
			mmu->Write8(ea, (uint8_t)GPR[rS]);
			std::cout << "Executing stb: Stored 0x" << std::hex << (uint32_t)GPR[rS]
				<< " ('" << (char)GPR[rS] << "') at 0x" << ea << std::dec << "\n";
				//PC += 4;
		}
		catch (const std::exception& e) {
			std::cout << "Exception triggered, vector=0x00000700, reason: " << e.what() << "\n";
			SRR0 = PC + 4; // Guardar PC de la siguiente instrucción
			SRR1 = MSR; // Guardar MSR
			PC = 0x00000700; // Vector de excepción
		}
		break;
	}
	case 39: { // stbu
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		mmu->Write8(addr, GPR[rs] & 0xFF);
		GPR[ra] = addr;
		break;
	}
	case 40: { // lhz
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		GPR[rt] = mmu->Read16(GPR[ra] + D);
		break;
	}
	case 41: { // lhzu
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		GPR[rt] = mmu->Read16(addr);
		GPR[ra] = addr;
		break;
	}
	case 42: { // lha
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		int16_t v = mmu->Read16(GPR[ra] + D);
		GPR[rt] = uint32_t(v);
		break;
	}
	case 43: { // lhau
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		int16_t v = mmu->Read16(addr);
		GPR[rt] = uint32_t(v);
		GPR[ra] = addr;
		break;
	}
	case 44: { // sth
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		mmu->Write16(GPR[ra] + D, GPR[rs] & 0xFFFF);
		break;
	}
	case 45: { // sthu
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		mmu->Write16(addr, GPR[rs] & 0xFFFF);
		GPR[ra] = addr;
		break;
	}
	case 46: { // lmw rt,..,d(ra)
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t base = GPR[ra] + D;
		for (uint32_t i = rt;i < 32;i++) {
			GPR[i] = mmu->Read32(base + 4 * (i - rt));
		}
		break;
	}
	case 47: { // stmw rs,..,d(ra)
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t base = GPR[ra] + D;
		for (uint32_t i = rs;i < 32;i++) {
			mmu->Write32(base + 4 * (i - rs), GPR[i]);
		}
		break;
	}
	case 48: { // lfs
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t w = mmu->Read32(GPR[ra] + D);
		float f; memcpy(&f, &w, 4);
		FPR[ft] = f;
		break;
	}
	case 49: { // lfsu
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		uint32_t w = mmu->Read32(addr);
		float f; memcpy(&f, &w, 4);
		FPR[ft] = f;
		GPR[ra] = addr;
		break;
	}
	case 50: { // lfd
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint64_t w = mmu->Read64(GPR[ra] + D);
		double d; memcpy(&d, &w, 8);
		FPR[ft] = d;
		break;
	}
	case 51: { // lfdu
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		uint64_t w = mmu->Read64(addr);
		double d; memcpy(&d, &w, 8);
		FPR[ft] = d;
		GPR[ra] = addr;
		break;
	}
	case 52: { // stfs
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		float f = FPR[ft];
		uint32_t w; memcpy(&w, &f, 4);
		mmu->Write32(GPR[ra] + D, w);
		break;
	}
	case 53: { // stfsu
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		float f = FPR[ft];
		uint32_t w; memcpy(&w, &f, 4);
		mmu->Write32(addr, w);
		GPR[ra] = addr;
		break;
	}
	case 54: { // stfd
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		double d = FPR[ft];
		uint64_t w; memcpy(&w, &d, 8);
		mmu->Write64(GPR[ra] + D, w);
		break;
	}
	case 55: { // stfdu
		uint32_t ft = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15);
		int16_t D = instr & 0xFFFF;
		uint32_t addr = GPR[ra] + D;
		double d = FPR[ft];
		uint64_t w; memcpy(&w, &d, 8);
		mmu->Write64(addr, w);
		GPR[ra] = addr;
		break;
	}
	case 58: {
		uint32_t sub = ExtractBits(instr, 30, 31);
		uint32_t rt = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15), rb = ExtractBits(instr, 16, 20);
		switch (sub) {
		case 0: { // ld
			int16_t D = instr & 0xFFFF;
			uint64_t w = mmu->Read64(GPR[ra] + D);
			GPR[rt] = uint32_t(w >> 32);
			GPR[rt + 1] = uint32_t(w);
			break;
		}
		case 1: { // ldu
			int16_t D = instr & 0xFFFF;
			uint32_t addr = GPR[ra] + D;
			uint64_t w = mmu->Read64(addr);
			GPR[rt] = uint32_t(w >> 32);
			GPR[rt + 1] = uint32_t(w);
			GPR[ra] = addr;
			break;
		}
		case 2: { // lwa
			int16_t D = instr & 0xFFFF;
			uint32_t addr = GPR[ra] + D;
			uint64_t w = mmu->Read64(addr);
			GPR[rt] = uint32_t(w >> 32);
			GPR[rt + 1] = uint32_t(w);
			LR = addr;
			break;
		}
		default: {
			//PPC_DECODER_MISS;
		}
			   break;
		}
		break;
	}
	case 59: {
		uint32_t sub = ExtractBits(instr, 26, 30);
		uint32_t ft = ExtractBits(instr, 6, 10), fa = ExtractBits(instr, 11, 15), fb = ExtractBits(instr, 16, 20);
		float* FA = reinterpret_cast<float*>(&FPR[fa]);
		float* FB = reinterpret_cast<float*>(&FPR[fb]);
		float* FR = reinterpret_cast<float*>(&FPR[ft]);
		switch (sub) {
		case 18: for (int i = 0;i < 1;i++) FR[i] = FA[i] / FB[i]; break;  // fdivsx
		case 20: for (int i = 0;i < 1;i++) FR[i] = FA[i] - FB[i]; break;  // fsubsx
		case 21: for (int i = 0;i < 1;i++) FR[i] = FA[i] + FB[i]; break;  // faddsx
		case 22: FR[0] = sqrtf(FA[0]); break;                        // fsqrtsx
		case 24: FR[0] = 1.0f / FB[0]; break;                          // fresx
		case 25: for (int i = 0;i < 1;i++) FR[i] = FA[i] * FB[i]; break;  // fmulsx
		case 28: FR[0] = FA[0] * FB[0] - FR[0]; break;                 // fmsubsx
		case 29: FR[0] = FA[0] * FB[0] + FR[0]; break;                 // fmaddsx
		case 30: FR[0] = -(FA[0] * FB[0]) - FR[0]; break;              // fnmsubsx
		case 31: FR[0] = -(FA[0] * FB[0]) + FR[0]; break;              // fnmaddsx
		default: {}//PPC_DECODER_MISS;
		}
		break;
	}
	case 62: {
		uint32_t sub = ExtractBits(instr, 30, 31);
		uint32_t rs = ExtractBits(instr, 6, 10), ra = ExtractBits(instr, 11, 15), rb = ExtractBits(instr, 16, 20);
		switch (sub) {
		case 0: { // std
			int16_t D = instr & 0xFFFF;
			uint64_t w = (uint64_t(GPR[rs]) << 32) | GPR[rs + 1];
			mmu->Write64(GPR[ra] + D, w);
		} break;
		case 1: { // stdu
			int16_t D = instr & 0xFFFF;
			uint32_t addr = GPR[ra] + D;
			uint64_t w = (uint64_t(GPR[rs]) << 32) | GPR[rs + 1];
			mmu->Write64(addr, w);
			GPR[ra] = addr;
		} break;
		}
		break;
	}
	case 63: {
		uint32_t sub1 = ExtractBits(instr, 21, 30);
		uint32_t sub2 = ExtractBits(instr, 26, 30);
		uint32_t ft = ExtractBits(instr, 6, 10), fb = ExtractBits(instr, 16, 20);
		float* FA = reinterpret_cast<float*>(&FPR[ExtractBits(instr, 11, 15)]);
		float* FB = reinterpret_cast<float*>(&FPR[fb]);
		float* FR = reinterpret_cast<float*>(&FPR[ft]);
		double* DA = reinterpret_cast<double*>(&FPR[ExtractBits(instr, 11, 15)]);
		double* DB = reinterpret_cast<double*>(&FPR[fb]);
		double* DR = reinterpret_cast<double*>(&FPR[ft]);
		switch (sub1) {
		case   0: // fcmpu
			CR.fields.cr0 = (FA[0] < FB[0] ? 8 : 0) | (FA[0] > FB[0] ? 4 : 0) | (FA[0] == FB[0] ? 2 : 0);
			break;
		case  12: FR[0] = FA[0]; break;            // frspx
		case  14: FR[0] = floorf(FA[0]); break;    // fctiwx
		case  15: FR[0] = floorf(FA[0]); break;    // fctiwzx
		case  32: // fcmpo
			CR.fields.cr0 = (FA[0] < FB[0] ? 8 : 0) | (FA[0] > FB[0] ? 4 : 0) | (FA[0] == FB[0] ? 2 : 0);
			break;
		case  38: FPR[ft] = float(int((CR.value >> 31) & 1)); break; // mtfsb1x
		case  40: FR[0] = -FA[0]; break;          // fnegx
		case  64: { // mcrfs
			uint32_t crm = ExtractBits(instr, 11, 15);
			uint32_t mask = ExtractBits(instr, 6, 10) << ((7 - crm) * 4);
			FPSCR = (FPSCR & ~mask) | (CR.value & mask);
		} break;
		case  70: CR.value = (CR.value & ~(1 << ((7 - ExtractBits(instr, 11, 15)) * 4 + 2))) |
			(((FPSCR >> ((7 - ExtractBits(instr, 11, 15)) * 4 + 2)) & 1) << ((7 - ExtractBits(instr, 11, 15)) * 4 + 2));
			break; // mtfsb0x
		case  72: FPR[ft] = float(FPSCR); break;  // fmrx
		case 134: FPSCR = GPR[ExtractBits(instr, 6, 10)]; break; // mtfsfix
		case 136: FR[0] = fabsf(-FA[0]); break;  // fnabsx
		case 264: FR[0] = fabsf(FA[0]); break;   // fabsx
		case 583: FPR[ft] = float(TBL); break;    // mffsx
		case 711: FPSCR = GPR[ExtractBits(instr, 6, 10)]; break; // mtfsfx
		case 814: FR[0] = floorf(FA[0]); break;  // fctidx
		case 815: FR[0] = floorf(FA[0]); break;  // fctidzx
		case 846: FR[0] = floorf(FA[0]); break;  // fcfidx
		default: break;
		}
		switch (sub2) {
		case 18: FR[0] = FA[0] / FB[0]; break;    // fdivx
		case 20: FR[0] = FA[0] - FB[0]; break;    // fsubx
		case 21: FR[0] = FA[0] + FB[0]; break;    // faddx
		case 22: FR[0] = sqrtf(FA[0]); break;     // fsqrtx
		case 23: FR[0] = (FA[0] >= 0 ? FA[0] : -FA[0]); break; // fselx
		case 25: FR[0] = FA[0] * FB[0]; break;    // fmulx
		case 26: FR[0] = 1.0f / sqrtf(FA[0]); break; // frsqrtex
		case 28: FR[0] = FA[0] * FB[0] - FR[0]; break; // fmsubx
		case 29: FR[0] = FA[0] * FB[0] + FR[0]; break; // fmaddx
		case 30: FR[0] = -(FA[0] * FB[0]) - FR[0]; break;// fnmsubx
		case 31: FR[0] = -(FA[0] * FB[0]) + FR[0]; break;// fnmaddx
		default: break;
		}
		break;
	}
	default: {
		LOG_ERROR("[CPU]", "Unimplemented opcode %d (instr=0x%08X)", opcode, instr);
		TriggerException(PPU_EX_PROG);
		//haltInvalidOpcode(opcode);
		break;
	}
	}
}
