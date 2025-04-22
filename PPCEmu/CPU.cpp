/* Made by Slam */
#include "CPU.h"
#include "Log.h"
#include <iostream>
#include <atomic>
#include <cmath>

CPU::CPU(MMU* mmu)
	: mmu(mmu), PC(0), LR(0), CTR(0), XER(0),
	MSR(0), FPSCR(0), HID4(0),
	GQR{ 0 }, SPRG0(0), SPRG1(0), SPRG2(0), SPRG3(0),
	GPR{ 0 }, FPR{ 0 }, VPR{ 0 }, SPR{ 0 }, running(false)
{
	CR.value = 0;
	SPR[1009] = 0;   // PIR  (processor ID), p.ej. 1
	SPR[1023] = 0;   // SPR1023, valor fijo
	SPR[318] = 0;          // DEC   (decrementer), o 0

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
	SPR[1009] = 0;  // PIR fija
	SPR[1023] = 0;  // SPR1023 fija
	SPR[318] = 0;  // DEC (o el valor que prefieras)

	this->GPR = GPR;
	FPR.fill(0.0);
	VPR.fill(uint128_t{ 0, 0 });
	SPR.fill(0);

	running = true;
	LOG_INFO("CPU", "CPU reset, PC set to 0x%08X", PC);
}

void CPU::Reset() {
	GPR.fill(0);
	FPR.fill(0.0);
	VPR.fill(uint128_t{ 0, 0 });
	SPR.fill(0);

	PC = 0;
	LR = 0;
	CR.value = 0;
	CTR = 0;
	XER = 0;
	FPSCR = 0;
	MSR = 0;
	HID4 = 0;

	running = true;
	LOG_INFO("CPU", "CPU reset, PC unchanged at 0x%08X", PC);
}

// Maneja instrucciones secuencialmente
void CPU::Step() {
	uint32_t instruction = mmu->Read32(PC);
	std::cout << "[DEBUG] PC=0x" << std::hex << PC << ", Instruction=0x" << instruction << std::endl;
	if (!running)return;
	if (DEC > 0) {
		DEC--;
		if (DEC == 0 && (MSR & 0x8000)) { // EE bit enabled
			TriggerException(0x900);
		}
	}
	// Decode and execute instruction
	try {
		DecodeExecute(instruction);
	}
	catch (const std::exception& e) {
		std::cerr << "[ERROR] Halt at PC=0x" << std::hex << PC << ": " << e.what() << std::endl;
		LOG_INFO("CPU", "PC=0x%08X stw r5 (0x%08X) -> [0x%08X]", PC, GPR[5], GPR[4]);
		LOG_INFO("CPU", "PC=0x%08X r4=0x%08X", PC, GPR[4]);
		// Dump registers, FPSCR, etc.
		DumpRegisters();
		throw; // Or handle gracefully
	}
	PC += 4;
}

// Captura la instrucción del momento
uint32_t CPU::FetchInstruction() {
	return mmu->Read32(PC);
}

// Manejo de excepciones
void CPU::TriggerException(uint32_t vector) {
	SRR0 = PC; // Guardar PC actual
	SRR1 = MSR; // Guardar estado de MSR
	MSR &= ~0x8000; // Deshabilitar interrupciones externas (EE=0)
	PC = vector; // Saltar al vector de excepción
	LOG_INFO("CPU", "Exception triggered, vector=0x%08X", vector);
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
	out.write(reinterpret_cast<const char*>(VPR.data()), VPR.size() * sizeof(uint128_t));
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
	in.read(reinterpret_cast<char*>(VPR.data()), VPR.size() * sizeof(uint128_t));
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

void CPU::DecodeExecute(uint32_t instr) {
	uint32_t opcode = (instr >> 26) & 0x3F;	
	uint8_t rs = (instr >> 21) & 0x1F;
	uint8_t ra = (instr >> 16) & 0x1F;
	int16_t offset = instr & 0xFFFF;
	uint32_t addr = GPR[ra] + offset;
	/*Hook FB*/
	auto check_framebuffer_hook = [&](uint32_t addr, uint32_t val, const char* op) {		
		if ((addr >= 0xEC800000ULL && addr < 0xEC810000ULL + (640 * 480 * 4)) ||
			(addr >= 0xC0000000ULL && addr < 0xC0000000ULL + (640 * 480 * 4)) ||
			(addr >= 0xD0000000ULL && addr < 0xD0000000ULL + (640 * 480 * 4)) ||			
			(addr >= 0xE0000000ULL && addr < 0xE0000000ULL + (640 * 480 * 4)) ||
			(addr >= 0x9E000000ULL && addr < 0x9E000000ULL + (640 * 480 * 4)) ||
			(addr >= 0x8000000000000000ULL && addr < 0x80000200FFFFFFFFULL) ||
			(addr >= 0x80000000 && addr < 0x90000000) ||
			(addr >= 0x9E000000 && addr < 0x9E000000 + (640 * 480 * 4)) ||
			(addr >= 0x7FEA0000ULL && addr < 0x7FEB0000ULL + (640 * 480 * 4))) {
			std::cout << "[" << op << " FRAMEBUFFER] PC=0x" << std::hex << PC
				<< " Addr=0x" << addr << " Val=0x" << val << std::endl;
			std::cout << "[STW FRAMEBUFFER 9E000000] PC=0x" << std::hex << PC
				<< " Addr=0x" << addr << " Val=0x" << val << std::endl;
		}
	};

	switch (opcode) {
	case 0: { // ?
		LOG_INFO("CPU", "Unknow OP at PC 0x%08X", PC);
		//running = false;
		return;
	}
	case  3: { // twi — 0x03 hex = 3 decimal
		uint32_t TO = (instr >> 21) & 0x1F;
		uint32_t BI = (instr >> 16) & 0x1F;
		int16_t  AA = instr & 0xFFFF;
		LOG_WARNING("CPU", "twi TO=%02X, BI=%d, AA=0x%04X (no implementado)", TO, BI, AA);
		running = false;  // o bien TriggerException(…)
		break;
	}

	case 4: { // VMX: vaddubm, vand, vor, etc.
		uint32_t vD = (instr >> 21) & 0x1F;
		uint32_t vA = (instr >> 16) & 0x1F;
		uint32_t vB = (instr >> 11) & 0x1F;
		uint32_t vC = (instr >> 6) & 0x1F;
		uint32_t subopcode = (instr >> 6) & 0x1F;
		switch (subopcode) {
		case 0: { // vaddubm		
			for (int i = 0; i < 16; i++) {
				uint8_t a = ((uint8_t*)&VPR[vA])[i];
				uint8_t b = ((uint8_t*)&VPR[vB])[i];
				((uint8_t*)&VPR[vD])[i] = a + b;
			}
			/*for (int i = 0; i < 16; ++i) {
				uint8_t a = ((i < 8) ? (VPR[vA].lo >> (56 - i * 8)) : (VPR[vA].hi >> (56 - (i - 8) * 8))) & 0xFF;
				uint8_t b = ((i < 8) ? (VPR[vB].lo >> (56 - i * 8)) : (VPR[vB].hi >> (56 - (i - 8) * 8))) & 0xFF;
				uint64_t mask = ~(0xFFULL << (56 - (i % 8) * 8));
				if (i < 8) VPR[vD].lo = (VPR[vD].lo & mask) | (uint64_t)(a + b) << (56 - i * 8);
				else       VPR[vD].hi = (VPR[vD].hi & mask) | (uint64_t)(a + b) << (56 - (i - 8) * 8);
			}*/
			break;
		}
		case 4: { // vand
			VPR[vD].lo = VPR[vA].lo & VPR[vB].lo;
			VPR[vD].hi = VPR[vA].hi & VPR[vB].hi;
			LOG_INFO("CPU", "vand v%d = v%d & v%d", vD, vA, vB);
			break;
		}
		case 8: { // vslb
			VPR[vD].lo = VPR[vA].lo << (VPR[vB].lo & 0x7);
			VPR[vD].hi = VPR[vA].hi << (VPR[vB].hi & 0x7);
			LOG_INFO("CPU", "vslb v%d = v%d << v%d", vD, vA, vB);
			break;
		}
		case 9: { // vsrb
			VPR[vD].lo = VPR[vA].lo >> (VPR[vB].lo & 0x7);
			VPR[vD].hi = VPR[vA].hi >> (VPR[vB].hi & 0x7);
			LOG_INFO("CPU", "vsrb v%d = v%d >> v%d", vD, vA, vB);
			break;
		}
		case 10: { // vcmpequb
			VPR[vD].lo = (VPR[vA].lo == VPR[vB].lo) ? 0xFFFFFFFFFFFFFFFF : 0;
			VPR[vD].hi = (VPR[vA].hi == VPR[vB].hi) ? 0xFFFFFFFFFFFFFFFF : 0;
			LOG_INFO("CPU", "vcmpequb v%d = v%d == v%d", vD, vA, vB);
			break;
		}
		case 2: { // vmuloub
			VPR[vD].lo = (VPR[vA].lo & 0xFF) * (VPR[vB].lo & 0xFF);
			VPR[vD].hi = (VPR[vA].hi & 0xFF) * (VPR[vB].hi & 0xFF);
			LOG_INFO("CPU", "vmuloub v%d = v%d * v%d", vD, vA, vB);
			break;
		}
		case 43: { // vperm
			for (int i = 0; i < 16; ++i) {
				uint8_t sel = (VPR[vC].lo >> (i * 5)) & 0x1F; // 5 bits por índice
				uint8_t* src = (sel < 16) ? (uint8_t*)&VPR[vA] : (uint8_t*)&VPR[vB];
				((uint8_t*)&VPR[vD])[i] = src[sel % 16]; // Acceso a 16 bytes por vector
			}
			LOG_INFO("CPU", "vperm v%d, v%d, v%d, v%d", vD, vA, vB, vC);
			break;
		}

		case 44: { // vspltw
			uint32_t UIMM = (instr >> 11) & 0x1F;
			uint32_t word = (VPR[vB].lo >> (32 * (3 - UIMM))) & 0xFFFFFFFF;
			VPR[vD].lo = VPR[vD].hi = (uint64_t)word * 0x0001000100010001ULL;
			LOG_INFO("CPU", "vspltw v%d, v%d, UIMM=%d", vD, vB, UIMM);
			break;
		}
			   // Nueva instrucción VMX128: vpermwi		
			   // Nueva instrucción VMX128: vsldoi
		case 45: { // vspltb
			uint32_t UIMM = (instr >> 11) & 0x1F;
			uint8_t byte = (VPR[vB].lo >> (8 * (15 - UIMM))) & 0xFF;
			VPR[vD].lo = VPR[vD].hi = (uint64_t)byte * 0x0101010101010101ULL;
			LOG_INFO("CPU", "vspltb v%d, v%d, UIMM=%d", vD, vB, UIMM);
			break;
		}
		case 46: { // vsldoi
			uint32_t sh = (instr >> 6) & 0xF;
			uint8_t* dst = (uint8_t*)&VPR[vD];
			uint8_t* srcA = (uint8_t*)&VPR[vA];
			uint8_t* srcB = (uint8_t*)&VPR[vB];
			for (int i = 0; i < 16; ++i) {
				dst[i] = (i + sh < 16) ? srcA[i + sh] : srcB[i + sh - 16];
			}
			LOG_INFO("CPU", "vsldoi v%d, v%d, v%d, sh=%d", vD, vA, vB, sh);
			break;
		}
		case 47: { // vpermwi (VMX128)
			uint32_t imm = (instr >> 6) & 0xFF;
			for (int i = 0; i < 4; ++i) {
				uint32_t sel = (imm >> (i * 2)) & 0x3;
				((uint32_t*)&VPR[vD])[i] = ((uint32_t*)&VPR[vB])[sel];
			}
			LOG_INFO("CPU", "vpermwi v%d, v%d, imm=0x%02X", vD, vB, imm);
			break;
		}
		case 48: { // vpmsum (nueva)
			// vpmsum realiza suma de productos módulo para bytes (8-bit)
			uint64_t sum_lo = 0, sum_hi = 0;
			for (int i = 0; i < 8; ++i) {
				uint8_t a_lo = (VPR[vA].lo >> (i * 8)) & 0xFF;
				uint8_t b_lo = (VPR[vB].lo >> (i * 8)) & 0xFF;
				uint8_t a_hi = (VPR[vA].hi >> (i * 8)) & 0xFF;
				uint8_t b_hi = (VPR[vB].hi >> (i * 8)) & 0xFF;
				sum_lo += static_cast<uint64_t>(a_lo) * b_lo;
				sum_hi += static_cast<uint64_t>(a_hi) * b_hi;
			}
			VPR[vD].lo = sum_lo;
			VPR[vD].hi = sum_hi;
			LOG_INFO("CPU", "vpmsum v%d = sum(v%d * v%d)", vD, vA, vB);
			break;
		}
		case 49: { // vpkpx (nueva)
			// vpkpx empaqueta píxeles de 32-bit (ARGB) a 16-bit (5:6:5)
			uint16_t* dst = (uint16_t*)&VPR[vD];
			uint32_t* src_a = (uint32_t*)&VPR[vA];
			uint32_t* src_b = (uint32_t*)&VPR[vB];
			for (int i = 0; i < 4; ++i) {
				// Extraer componentes ARGB de vA y vB
				uint32_t pixel_a = src_a[i];
				uint32_t pixel_b = src_b[i];
				// Convertir a 5:6:5 (R:5, G:6, B:5)
				uint16_t r_a = (pixel_a >> 19) & 0x1F; // 5 bits rojo
				uint16_t g_a = (pixel_a >> 10) & 0x3F; // 6 bits verde
				uint16_t b_a = (pixel_a >> 3) & 0x1F;  // 5 bits azul
				uint16_t r_b = (pixel_b >> 19) & 0x1F;
				uint16_t g_b = (pixel_b >> 10) & 0x3F;
				uint16_t b_b = (pixel_b >> 3) & 0x1F;
				// Empaquetar en vD
				dst[i] = (r_a << 11) | (g_a << 5) | b_a;
				dst[i + 4] = (r_b << 11) | (g_b << 5) | b_b;
			}
			LOG_INFO("CPU", "vpkpx v%d = pack(v%d, v%d)", vD, vA, vB);
			break;
		}
		default:
			LOG_WARNING("CPU", "Unknown VMX subopcode 0x%02X", subopcode);
			running = false;
		}
		break;
	}
	case 6: { // vor
		uint32_t vD = (instr >> 21) & 0x1F;
		uint32_t vA = (instr >> 16) & 0x1F;
		uint32_t vB = (instr >> 11) & 0x1F;
		VPR[vD].lo = VPR[vA].lo | VPR[vB].lo;
		VPR[vD].hi = VPR[vA].hi | VPR[vB].hi;
		LOG_INFO("CPU", "vor v%d = v%d | v%d", vD, vA, vB);
		break;
	}
	case 7: { // mulli
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int32_t simm = static_cast<int32_t>(static_cast<int16_t>(instr & 0xFFFF));
		GPR[rD] = static_cast<uint32_t>(static_cast<int32_t>(GPR[rA]) * simm);
		LOG_INFO("CPU", "mulli r%d = r%d * %d (0x%04X)", rD, rA, simm, simm);
		break;
	}
	case 8: { // subfic
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t imm = instr & 0xFFFF;
		int32_t a = static_cast<int32_t>(GPR[rA]);
		int32_t imm_ext = static_cast<int32_t>(imm);
		int64_t result = static_cast<int64_t>(imm_ext) - static_cast<int64_t>(a);
		GPR[rD] = static_cast<uint32_t>(result & 0xFFFFFFFF);
		XER &= ~0x20000000;
		if (imm_ext >= a) {
			XER |= 0x20000000;
		}
		break;
	}
	case 9: { // stw rS, d(rA)
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t base = (rA == 0 ? 0 : GPR[rA]);
		uint32_t addr = base + d;
		mmu->Write32(addr, GPR[rS]);
		LOG_INFO("CPU", "stw r%d to [0x%08X] = 0x%08X", rS, addr, GPR[rS]);		

		break;
	}
	case 10: { // cmpl
		uint32_t crfD = (instr >> 23) & 0x7;
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rB = (instr >> 11) & 0x1F;
		uint32_t a = GPR[rA], b = GPR[rB];
		uint32_t cr = (a < b) ? 0x8 : (a > b) ? 0x4 : 0x2;
		CR.value = (CR.value & ~(0xF << (28 - 4 * crfD))) | (cr << (28 - 4 * crfD));
		LOG_INFO("CPU", "cmpl cr%d, r%d, r%d", crfD, rA, rB);
		break;
	}
	case 11: { // cmp
		uint32_t crfD = (instr >> 23) & 0x7;
		int32_t a = static_cast<int32_t>(GPR[(instr >> 16) & 0x1F]);
		int32_t b = static_cast<int32_t>(GPR[(instr >> 11) & 0x1F]);
		uint32_t cr;
		if (a < b)       cr = 0x8;  // Less than (bit 31)
		else if (a > b)  cr = 0x4;  // Greater than (bit 30)
		else             cr = 0x2;  // Equal (bit 29)
		// Asegurar que solo modificamos los 4 bits del campo cr0
		uint32_t shift = (28 - 4 * crfD);
		CR.value = (CR.value & ~(0xF << shift)) | (cr << shift);
		LOG_INFO("CPU", "cmp cr%d, r%d, r%d -> CR=0x%08X",
			crfD, (instr >> 16) & 0x1F, (instr >> 11) & 0x1F, CR.value);
		break;
		/*uint32_t crfD = (instr >> 23) & 0x7;
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rB = (instr >> 11) & 0x1F;
		int32_t a = static_cast<int32_t>(GPR[rA]), b = static_cast<int32_t>(GPR[rB]);
		uint32_t cr = (a < b) ? 0x8 : (a > b) ? 0x4 : 0x2;
		CR.value = (CR.value & ~(0xF << (28 - 4 * crfD))) | (cr << (28 - 4 * crfD));
		LOG_INFO("CPU", "cmp cr%d, r%d, r%d", crfD, rA, rB);
		break;*/
	}
	case 12: { // addic
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t simm = static_cast<int16_t>(instr & 0xFFFF);
		uint64_t result = static_cast<uint64_t>(GPR[rA]) + simm;
		GPR[rD] = static_cast<uint32_t>(result);
		XER = (result > 0xFFFFFFFF) ? (XER | 0x20000000) : (XER & ~0x20000000);
		LOG_INFO("CPU", "addic r%d = r%d + 0x%04X", rD, rA, simm);
		break;
	}
	case 13: { // addic.
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t simm = static_cast<int16_t>(instr & 0xFFFF);
		uint64_t result = static_cast<uint64_t>(GPR[rA]) + simm;
		GPR[rD] = static_cast<uint32_t>(result);
		XER = (result > 0xFFFFFFFF) ? (XER | 0x20000000) : (XER & ~0x20000000);
		CR.value = (GPR[rD] == 0) ? 0x2 : (GPR[rD] & 0x80000000) ? 0x8 : 0x4;
		LOG_INFO("CPU", "addic. r%d = r%d + 0x%04X", rD, rA, simm);
		break;
	}
	case 14: { // addi (Corregido)
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t imm = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t a = (rA == 0) ? 0 : GPR[rA];
		GPR[rD] = a + imm;
		uint64_t result = static_cast<uint64_t>(a) + imm;
		GPR[rD] = static_cast<uint32_t>(result);
		XER = (result > 0xFFFFFFFF) ? (XER | 0x20000000) : (XER & ~0x20000000);
		CR.value = (GPR[rD] == 0) ? 0x2 : (GPR[rD] & 0x80000000) ? 0x8 : 0x4;
		LOG_INFO("CPU", "addi r%d = r%d + 0x%04X", rD, rA, imm);
		break;
	}
	case 15: { // addis
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t simm = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t a = (rA == 0) ? 0 : GPR[rA];
		GPR[rD] = (rA == 0 ? 0 : GPR[rA]) + (simm << 16);
		uint64_t result = static_cast<uint64_t>(a) + simm;
		GPR[rD] = static_cast<uint32_t>(result);
		XER = (result > 0xFFFFFFFF) ? (XER | 0x20000000) : (XER & ~0x20000000);
		CR.value = (GPR[rD] == 0) ? 0x2 : (GPR[rD] & 0x80000000) ? 0x8 : 0x4;
		LOG_INFO("CPU", "addis r%d = r%d + 0x%04X", rD, rA, simm);
		break;
	}
	case 16: { // bc
		uint32_t BO = (instr >> 21) & 0x1F;
		uint32_t BI = (instr >> 16) & 0x1F;
		int16_t BD = (instr & 0xFFFC);
		uint32_t target = PC + BD;

		// 1. Manejo del CTR (BO[2] = 0 → decrementar y verificar)
		bool ctr_ok = true;
		if ((BO & 0x4) == 0) {
			CTR--;
			bool ctr_zero = (CTR == 0);
			ctr_ok = (ctr_zero == ((BO >> 1) & 1)); // BO[1] → 0: CTR≠0, 1: CTR==0
		}

		// 2. Verificación del bit CR (BO[3] = 0 → verificar)
		bool cond_ok = true;
		if ((BO & 0x10) == 0) {
			bool cr_bit = (CR.value >> (31 - BI)) & 1;
			cond_ok = (cr_bit == ((BO >> 0) & 1)); // BO[0] → 0: bit=0, 1: bit=1
		}

		if (ctr_ok && cond_ok) {
			PC = target - 4; // Restamos 4 porque PC se incrementará después
			LOG_INFO("CPU", "Branch TAKEN to 0x%08X", target);
		}
		else {
			LOG_INFO("CPU", "Branch NOT taken");
		}
		LOG_INFO("CPU", "Post-CMP: r4=0x%08X, r0=0x%08X, CR=0x%08X",
			GPR[4], GPR[0], CR.value);
		LOG_INFO("CPU", "BC: BO=0x%X, BI=%d, CR[%d]=%d, CTR=%d, ctr_ok=%d, cond_ok=%d",
			BO, BI, BI, (CR.value >> (31 - BI)) & 1, CTR, ctr_ok, cond_ok);
		break;
	}
	case 17: { // sc (Corregido)
		// Genera excepción de system call (vector 0xC00)
		TriggerException(0xC00);
		LOG_INFO("CPU", "System call at PC 0x%08X", PC);
		break;
	}
	case 18: { // b, bl
		int32_t LI = instr & 0x03FFFFFC;
		if (LI & 0x02000000) LI |= 0xFC000000;
		bool AA = (instr & 0x2) != 0;
		bool LK = (instr & 0x1) != 0;
		uint32_t target = AA ? LI : (PC + LI);
		if (LK) LR = PC + 4;
		PC = target - 4;
		LOG_INFO("CPU", "b%s to 0x%08X (AA=%d)", LK ? "l" : "", target, AA);
		return;
	}
	case 19: {
		uint32_t BO = (instr >> 21) & 0x1F;
		uint32_t BI = (instr >> 16) & 0x1F;
		uint32_t XO = (instr >> 1) & 0x3FF;
		switch (XO) {
		case 4: { // twi
			uint32_t TO = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			int32_t a = static_cast<int32_t>(GPR[rA]);
			int32_t b = static_cast<int32_t>(GPR[rB]);
			bool trap = false;
			if ((TO & 0x10) && a < b) trap = true;
			if ((TO & 0x08) && a > b) trap = true;
			if ((TO & 0x04) && a == b) trap = true;
			if ((TO & 0x02) && static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) trap = true;
			if ((TO & 0x01) && static_cast<uint32_t>(a) > static_cast<uint32_t>(b)) trap = true;
			if (trap) {
				TriggerException(0x700);
				LOG_INFO("CPU", "twi trap TO=%d", TO);
			}
			break;
		}
		case 16: { // blr
			bool doBranch = true;
			if ((BO & 0b10000) == 0) {
				CTR -= 1;
				if (((CTR != 0) && ((BO & 0b00010) == 0)) || ((CTR == 0) && ((BO & 0b00010) != 0)))
					doBranch = false;
			}
			if ((BO & 0b00100) == 0) {
				bool crBit = (CR.value >> (31 - BI)) & 1;
				if (((crBit == 0) && ((BO & 0b00001) == 0)) || ((crBit == 1) && ((BO & 0b00001) != 0)))
					doBranch = false;
			}
			if (doBranch) {
				LOG_INFO("CPU", "blr to 0x%08X", LR);
				PC = LR;
				return;
			}
			else {
				LOG_INFO("CPU", "blr branch not taken");
			}
			break;
		}
		case 25: { // rlwinm
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t SH = (instr >> 11) & 0x1F;
			uint32_t MB = (instr >> 6) & 0x1F;
			uint32_t ME = (instr >> 1) & 0x1F;
			uint32_t val = (GPR[rS] << SH) | (GPR[rS] >> (32 - SH));
			uint32_t mask = MaskFromMBME(MB, ME);
			GPR[rA] = val & mask;
			PC += 4;
			break;
		}
		case 528: { // bctr
			bool doBranch = true;
			if ((BO & 0b10000) == 0) {
				CTR -= 1;
				if (((CTR != 0) && ((BO & 0b00010) == 0)) || ((CTR == 0) && ((BO & 0b00010) != 0)))
					doBranch = false;
			}
			if ((BO & 0b00100) == 0) {
				bool crBit = (CR.value >> (31 - BI)) & 1;
				if (((crBit == 0) && ((BO & 0b00001) == 0)) || ((crBit == 1) && ((BO & 0b00001) != 0)))
					doBranch = false;
			}
			if (doBranch) {
				LOG_INFO("CPU", "bctr to 0x%08X", CTR);
				PC = CTR;
				return;
			}
			else {
				LOG_INFO("CPU", "bctr branch not taken");
			}
			break;
		}
		case 50: { // crand
			CR.value &= ~(1 << (31 - BI));
			CR.value |= ((((CR.value >> (31 - ((instr >> 21) & 0x1F))) & 1) &
				((CR.value >> (31 - ((instr >> 16) & 0x1F))) & 1))
				<< (31 - BI));
			break;
		}
		case 33: { // crxor
			CR.value &= ~(1 << (31 - BI));
			CR.value |= ((((CR.value >> (31 - ((instr >> 21) & 0x1F))) & 1) ^
				((CR.value >> (31 - ((instr >> 16) & 0x1F))) & 1))
				<< (31 - BI));
			break;
		}
		case 449: { // isync
			LOG_INFO("CPU", "isync (pipeline flushed)");
			break;
		}
		case 274:  {// trap
			LOG_INFO("CPU", "NOP trap XO=274 en PC=0x%08X", PC);
			break;
		}
		case 18: { // rfi
			if (MSR & 0x4000) TriggerException(0x700); // Privileged instruction
			if (MSR & 0x00010000) TriggerException(0x700); // Privilege violation
			MSR = SRR1;
			PC = SRR0;
			LOG_INFO("CPU", "rfi to 0x%08X", PC);
			break;
		}
		case 150: { // mfmsr
			uint32_t rD = (instr >> 21) & 0x1F;
			GPR[rD] = MSR;
			LOG_INFO("CPU", "mfmsr r%d = 0x%08X", rD, MSR);
			break;
		}
		default:
			LOG_WARNING("CPU", "Unsupported XL-form opcode: XO=%d", XO);
			//running = false;
			break;
		}
		break;
	}
	case 20: { // rlwimi
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t SH = (instr >> 11) & 0x1F;
		uint32_t MB = (instr >> 6) & 0x1F;
		uint32_t ME = (instr >> 1) & 0x1F;
		uint32_t rotated = (GPR[rS] << SH) | (GPR[rS] >> (32 - SH));
		//uint32_t mask = ((1ULL << (ME - MB + 1)) - 1) << (31 - ME);
		uint32_t mask = MaskFromMBME(MB, ME);
		GPR[rA] = (GPR[rA] & ~mask) | (rotated & mask);
		LOG_INFO("CPU", "rlwimi r%d, r%d, SH=%d, MB=%d, ME=%d", rA, rS, SH, MB, ME);
		break;
	}
	case 21: { // rlwinm
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t SH = (instr >> 11) & 0x1F;
		uint32_t MB = (instr >> 6) & 0x1F;
		uint32_t ME = (instr >> 1) & 0x1F;
		uint32_t rotated = (GPR[rS] << SH) | (GPR[rS] >> (32 - SH));
		//uint32_t mask = ((1ULL << (ME - MB + 1)) - 1) << (31 - ME);
		uint32_t mask = MaskFromMBME(MB, ME);
		GPR[rA] = rotated & mask;
		LOG_INFO("CPU", "rlwinm r%d, r%d, SH=%d, MB=%d, ME=%d", rA, rS, SH, MB, ME);
		break;
	}
	case 22: { // lhz
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t ea = (rA == 0 ? 0 : GPR[rA]) + d;
		GPR[rD] = mmu->Read16(ea);
		break;
	}
	case 23: { // lha (sign‑extended)
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t ea = (rA == 0 ? 0 : GPR[rA]) + d;
		GPR[rD] = int32_t(int16_t(mmu->Read16(ea)));
		break;
	}
	case 24: { // oris
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint16_t uimm = instr & 0xFFFF;
		GPR[rA] = GPR[rS] | (static_cast<uint32_t>(uimm) << 16);
		LOG_INFO("CPU", "oris r%d = r%d | 0x%04X0000 -> 0x%08X", rS, rA, uimm, GPR[rS]);
		PC += 4;
		break;
	}
	case 25: { // ori — 0x19 hex = 25 decimal
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint16_t uimm = instr & 0xFFFF;
		GPR[rA] = GPR[rS] | uimm;
		LOG_INFO("CPU", "ori r%d = r%d | 0x%04X", rA, rS, uimm);
		break;
	}
	case 26: { // xori
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint16_t uimm = instr & 0xFFFF;
		GPR[rA] = GPR[rS] ^ uimm;
		LOG_INFO("CPU", "xori r%d = r%d ^ 0x%04X", rA, rS, uimm);
		break;
	}
	case 28: { // andi.
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint16_t uimm = instr & 0xFFFF;
		GPR[rA] = GPR[rS] & uimm;
		CR.value = (GPR[rA] == 0) ? 0x2 : (GPR[rA] & 0x80000000) ? 0x8 : 0x4;
		LOG_INFO("CPU", "andi r%d = r%d & 0x%04X", rA, rS, uimm);
		break;
	}
	case 30: {
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rB = (instr >> 11) & 0x1F;
		uint32_t XO = (instr >> 1) & 0x3FF;

		switch (XO) {
		case 135: { // rldicl
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t SH = ((instr >> 11) & 0x1F) | ((instr >> 1) & 0x20);
			uint32_t MB = (instr >> 6) & 0x3F;
			uint64_t rotated = (static_cast<uint64_t>(GPR[rS]) << SH) | (static_cast<uint64_t>(GPR[rS]) >> (64 - SH));
			uint64_t mask = (MB == 0) ? ~0ULL : ~(~0ULL << (64 - MB));
			GPR[rA] = static_cast<uint32_t>(rotated & mask);
			LOG_INFO("CPU", "rldicl r%d = r%d << %d, mask %d", rA, rS, SH, MB);
			break;
		}
		case 198: { // extsw
			GPR[rD] = (int32_t)GPR[rA];
			LOG_INFO("CPU", "extsw r%d = (int32_t)r%d", rD, rA);
			break;
		}
		case 444: { // orc
			GPR[rD] = GPR[rA] | (~GPR[rB]);
			LOG_INFO("CPU", "orc r%d = r%d | ~r%d", rD, rA, rB);
			break;
		}
		case 476: { // nand
			GPR[rD] = ~(GPR[rA] & GPR[rB]);
			LOG_INFO("CPU", "nand r%d = ~(r%d & r%d)", rD, rA, rB);
			break;
		}
		case 982: { // rldcr
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t ME = (instr >> 6) & 0x3F;
			uint64_t rotated = (static_cast<uint64_t>(GPR[rS]) << (GPR[rB] & 0x3F)) | (static_cast<uint64_t>(GPR[rS]) >> (64 - (GPR[rB] & 0x3F)));
			uint64_t mask = (ME == 63) ? ~0ULL : (~0ULL >> ME);
			GPR[rA] = static_cast<uint32_t>(rotated & mask);
			LOG_INFO("CPU", "rldcr r%d = r%d << r%d, mask %d", rA, rS, rB, ME);
			break;
		}
		case 995: { // cntlzw
			uint32_t val = GPR[rA];
			uint32_t count = 0;
			for (int i = 31; i >= 0; --i) {
				if ((val >> i) & 1) break;
				count++;
			}
			GPR[rD] = count;
			LOG_INFO("CPU", "cntlzw r%d = clz(r%d) = %u", rD, rA, count);
			break;
		}
		case 807: { // stub para el ext_opcode 807
			LOG_INFO("CPU", "nop (ext_opcode 807) en PC=0x%08X", PC);

			break;
		}
		case 0x112:{ // el ext_opcode 0x112 decimal 274
			LOG_INFO("CPU", "NOP ext_opcode %u en PC=0x%08X", XO, PC);
			break;
		}
		default:
			LOG_WARNING("CPU", "NOP desconocido XO=%u en opcode 30 at PC=0x%08X", XO, PC);
			//running = false;
			break;
		}
		break;
	}
	case 31:{
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rB = (instr >> 11) & 0x1F;
		uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5;
		uint32_t ext_opcode = (instr >> 1) & 0x3FF;
		switch (ext_opcode) {
		case 8: { // subf
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rD] = GPR[rB] - GPR[rA];
			LOG_INFO("CPU", "subf r%d = r%d - r%d", rD, rB, rA);
			break;
		}
		case 28: { // and
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rA] = GPR[rS] & GPR[rB];
			LOG_INFO("CPU", "and r%d = r%d & r%d", rA, rS, rB);
			break;
		}
		case 60: { // andc
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rA] = GPR[rS] & ~GPR[rB];
			LOG_INFO("CPU", "andc r%d = r%d & ~r%d", rA, rS, rB);
			break;
		}
		case 83: { // mfmsr
			if (MSR & 0x4000) {
				TriggerException(0xC00);
				break;
			}
			GPR[rD] = MSR;
			LOG_INFO("CPU", "mfmsr r%d = MSR (0x%08X)", rD, MSR);
			break;
		}
		case 19: { // rfi
			if (MSR & 0x4000) {
				TriggerException(0xC00);
				break;
			}
			PC = SRR0;
			MSR = SRR1;
			LOG_INFO("CPU", "rfi to PC=0x%08X, MSR=0x%08X", PC, MSR);
			break;
		}
		case 98: { // lvx (VMX128 load vector)
			uint32_t ea = rA ? GPR[rA] + GPR[rB] : GPR[rB];
			uint8_t buffer[16];
			mmu->Read(ea, buffer, 16);
			for (int i = 0; i < 16; ++i) {
				VPR[rD].lo = (VPR[rD].lo & ~(0xFFULL << (56 - i * 8))) | ((uint64_t)buffer[i] << (56 - i * 8));
			}
			LOG_INFO("CPU", "lvx v%d, r%d, r%d (EA=0x%08X)", rD, rA, rB, ea);
			break;
		}
		case 150: { // stwcx.
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + (rB == 0 ? 0 : GPR[rB]);
			bool success = reservation_valid && (reservation_addr == addr);
			if (success) {
				mmu->Write32(addr, GPR[rS]);
			}
			CR.value = (CR.value & ~0x4) | (success ? 0x4 : 0); // CR0[EQ]
			reservation_valid = false;
			LOG_INFO("CPU", "stwcx. %s at 0x%08X", success ? "success" : "fail", addr);
			break;
		}
		case 226: { // stvx (VMX128 store vector)
			uint32_t ea = rA ? GPR[rA] + GPR[rB] : GPR[rB];
			uint8_t buffer[16];
			for (int i = 0; i < 16; ++i) {
				buffer[i] = (VPR[rS].lo >> (56 - i * 8)) & 0xFF;
			}
			mmu->Write(ea, buffer, 16);
			LOG_INFO("CPU", "stvx v%d, r%d, r%d (EA=0x%08X)", rS, rA, rB, ea);
			break;
		}
		case 20: { // sc (system call)
			TriggerException(0xC00); // System Call Interrupt
			LOG_INFO("CPU", "sc at PC=0x%08X", PC);
			break;
		}
		case 178: { // mtmsr
			if (MSR & 0x4000) { // Si en modo usuario (PR=1)
				TriggerException(0x700); // Program exception
				break;
			}
			if (MSR & 0x00010000) TriggerException(0x700); // Privilege violation
			MSR = GPR[rS];
			LOG_INFO("CPU", "mtmsr r%d (0x%08X)", rS, GPR[rS]);
			break;
		}
		case 235: { // mulhw
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			int64_t result = static_cast<int64_t>(static_cast<int32_t>(GPR[rA])) * static_cast<int32_t>(GPR[rB]);
			GPR[rD] = static_cast<uint32_t>(result >> 32);
			LOG_INFO("CPU", "mulhw r%d = r%d * r%d", rD, rA, rB);
			break;
		}
		case 266: { // add
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rD] = GPR[rA] + GPR[rB];
			LOG_INFO("CPU", "add r%d = r%d + r%d", rD, rA, rB);
			break;
		}
		case 323: { // mfspr (Corregido)
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5;
			switch (spr) {
			case 8: GPR[rD] = LR; break; // LR
			case 9: GPR[rD] = CTR; break; // CTR
			case 22:  GPR[rD] = DEC; SPR[22] = DEC; break; // DEC
			case 268: GPR[rD] = TBL; SPR[268] = TBL; break;// TBL (Time Base Lower)
			case 269: GPR[rD] = TBU; SPR[269] = TBU; break;// TBU (Time Base Upper)
			case 312: GPR[rD] = SPRG0; break; // SPRG0
			case 313: GPR[rD] = SPRG1; break; // SPRG1
			case 314: GPR[rD] = SPRG2; break; // SPRG2
			case 315: GPR[rD] = SPRG3; break; // SPRG3
			case 318:{
				GPR[rD] = DEC; // mfspr
				break;
			}
			case 921: {
				GPR[rD] = SPR[921];
				LOG_INFO("CPU", "mfspr r%d = SPR%d (0x%08X)", rD, 921, SPR[921]);
				break;
			}
			case 922: GPR[rD] = GQR[2]; break; // GQR2
			case 998: {
				GPR[rD] = SPR[998];
				LOG_INFO("CPU", "mfspr r%d = SPR998 (0x%08X)", rD, SPR[998]);
				break;
			}
			case 284: GPR[rD] = SPR[284]; break; // DEC (Decrementer)
			case 1008: GPR[rD] = SPR[1008]; break; // USPRG0
			case 1009: {
				GPR[rD] = SPR[1009];
				//LOG_INFO("CPU", "mfspr r%d = PIR (0x%08X)", rD, SPR[1009]);
				break;
			}
			case 1010: GPR[rD] = SPR[1010]; break; // HID0
			case 1011: GPR[rD] = SPR[1011]; break; // HID1
			case 1012: GPR[rD] = SPRG1; /*o el registro que toque*/ break;
			case 1017: GPR[rD] = HID4; break;
			case 1023: {
				GPR[rD] = SPR[1023];
				break;
			}
			default:{				
				//running = false;
				LOG_WARNING("CPU", "Unknown mfspr SPR%u at PC=0x%08X", spr, PC);
				GPR[rD] = 0x00000000; // o valor razonable
			}
			break;
			}
			LOG_INFO("CPU", "mfspr r%d = SPR%d", rD, spr);
			if (spr < SPR.size()) {
				GPR[rD] = SPR[spr];
				LOG_INFO("CPU", "mfspr r%u = SPR%u (0x%08X)", rD, spr, SPR[spr]);
			}
			else {
				GPR[rD] = 0;
				LOG_WARNING("CPU", "mfspr r%u = SPR%u fuera de rango, devolviendo 0", rD, spr);				
			}
			break;
		}
		case 339: { // mfspr (mfdec, mftb)			
			uint32_t rD = (instr >> 21) & 0x1F;
			//uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5;
			uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 6) & 0x3E0);

			if (spr == 8) {
				GPR[rD] = LR;
				LOG_INFO("CPU", "mflr r%d", rD);
			}
			else if (spr == 9) {
				GPR[rD] = CTR;
				LOG_INFO("CPU", "mfctr r%d", rD);
			}
			else if (spr == 22) { // mfdec
				GPR[rD] = DEC; // Use DEC member variable
				SPR[22] = DEC; // Keep SPR array in sync
				LOG_INFO("CPU", "mfdec r%d = DEC (0x%08X)", rD, DEC);
			}
			else if (spr == 284) { // mfdec
				//if (((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5 == 284) {
				uint32_t rD = (instr >> 21) & 0x1F;
				GPR[rD] = SPR[284];
				LOG_INFO("CPU", "mfdec r%d = DEC (0x%08X)", rD, SPR[284]);
				//}						
			}
			else if (spr == 268 || spr == 269) { // mftb
				GPR[rD] = (spr == 268) ? TBL : TBU;
				SPR[spr] = GPR[rD]; // Keep SPR array in sync
				LOG_INFO("CPU", "mftb r%d = SPR%d (0x%08X)", rD, spr, GPR[rD]);
			}
			else if (spr == 323) { // mftb
				uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5;
				if (spr == 268 || spr == 269) {
					uint32_t rD = (instr >> 21) & 0x1F;
					GPR[rD] = SPR[spr];
					LOG_INFO("CPU", "mftb r%d = SPR%d (0x%08X)", rD, spr, SPR[spr]);
				}
			}else if (spr < 1024) {
				GPR[rD] = SPR[spr];
				LOG_INFO("CPU", "mfspr r%u = SPR[%u] = 0x%08X", rD, spr, SPR[spr]);
			}
			else if (MSR & 0x4000) { // Check MSR[PR] (user mode)
				TriggerException(0xC00); // Program Interrupt
				break;
			}
			else {
				LOG_WARNING("CPU", "Unknown mfspr SPR%d at PC 0x%08X", spr, PC);
				//running = false;
			}
			break;
		}
		case 444: { // or
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rA] = GPR[rS] | GPR[rB];
			LOG_INFO("CPU", "or r%d = r%d | r%d", rA, rS, rB);
			break;
		}
		case 467: { // mtspr (Corregido)
			uint32_t rS = (instr >> 21) & 0x1F;
			//uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5;
			uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 6) & 0x3E0);

			if (MSR & 0x4000) { TriggerException(0xC00);break; }
			if (spr < 1024) {
				SPR[spr] = GPR[rS];
				LOG_INFO("CPU", "mtspr SPR[%u] = r%u = 0x%08X", spr, rS, GPR[rS]);
			}
			switch (spr) {
			case 8: LR = GPR[rS]; break; // LR
			case 9: CTR = GPR[rS]; break; // CTR
			case 22: { // mtdec
				DEC = GPR[rS];
				SPR[22] = DEC; // Keep SPR array in sync
				LOG_INFO("CPU", "mtdec DEC = r%d (0x%08X)", rS, GPR[rS]);
				break;
			}
			case 269: SPR[269] = GPR[rS]; break; // TBU
			case 284: SPR[284] = GPR[rS]; break; // DEC
			case 312: SPRG0 = GPR[rS]; break; // SPRG0
			case 313: SPRG1 = GPR[rS]; break; // SPRG1
			case 314: SPRG2 = GPR[rS]; break; // SPRG2
			case 315: SPRG3 = GPR[rS]; break; // SPRG3
			case 318: {
				DEC = GPR[rS];
				LOG_INFO("CPU", "mtspr DEC (SPR318) = r%d (0x%08X)", rS, GPR[rS]);
				break;
			}
			case 319: HID4 = GPR[rS]; break; // HID4
			case 467: { // mttb
				uint32_t spr = ((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5;
				if (spr == 268 || spr == 269) {
					uint32_t rS = (instr >> 21) & 0x1F;
					SPR[spr] = GPR[rS];
					LOG_INFO("CPU", "mttb SPR%d = r%d (0x%08X)", spr, rS, GPR[rS]);
				}
				break;
			}
			case 921: {
				SPR[921] = GPR[rS];
				LOG_INFO("CPU", "mtspr SPR921 = r%d (0x%08X)", rS, GPR[rS]);
				break;
			}
			case 922: GQR[2] = GPR[rS]; break; // GQR2
			case 998: {
				SPR[998] = GPR[rS];
				LOG_INFO("CPU", "mtspr SPR998 = r%d (0x%08X)", rS, GPR[rS]);
				break;
			}
			case 1008: SPR[1008] = GPR[rS]; break; // USPRG0
			case 1009: {
				SPR[1009] = GPR[rS];
				LOG_INFO("CPU", "mtspr PIR = r%d (0x%08X)", rS, GPR[rS]);
				break;
			}
			case 1010: SPR[1010] = GPR[rS]; break; // HID0
			case 1011: SPR[1011] = GPR[rS]; break; // HID1
			case 1012: SPRG1 = GPR[rS]; break;
			case 1017: HID4 = GPR[rS]; break;case 268: SPR[268] = GPR[rS]; break; // TBL

			default:
				LOG_WARNING("CPU", "Unsupported SPR %d in mtspr", spr);
				//running = false;
			}
			LOG_INFO("CPU", "mtspr SPR%d = r%d (0x%08X)", spr, rS, GPR[rS]);
			// Generico: escribe cualquier SPR en el array
			if (spr < SPR.size()) {
				SPR[spr] = GPR[rS];
				LOG_INFO("CPU", "mtspr SPR%u = r%u (0x%08X)", spr, rS, SPR[spr]);
			}
			else {
				LOG_WARNING("CPU", "mtspr SPR%u fuera de rango, ignorado", spr);
			}
			break;
		}
		case 371: { // mfmsr
			uint32_t rD = (instr >> 21) & 0x1F;
			GPR[rD] = MSR;
			LOG_INFO("CPU", "mfmsr r%d = 0x%08X", rD, MSR);
			break;
		}
		case 459: { // divwu
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			if (GPR[rB] != 0) {
				GPR[rD] = GPR[rA] / GPR[rB];
				LOG_INFO("CPU", "divwu r%d = r%d / r%d", rD, rA, rB);
			}
			else {
				LOG_WARNING("CPU", "Division by zero in divwu");
				running = false;
			}
			break;
		}
		case 537: { // srw
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rA] = GPR[rS] >> (GPR[rB] & 0x1F);
			LOG_INFO("CPU", "srw r%d = r%d >> r%d", rA, rS, rB);
			break;
		}
		case 792: { // sraw
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			int32_t value = static_cast<int32_t>(GPR[rS]);
			GPR[rA] = value >> (GPR[rB] & 0x1F);
			LOG_INFO("CPU", "sraw r%d = r%d >> r%d", rA, rS, rB);
			break;
		}
		case 824: { // srawi
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t SH = (instr >> 11) & 0x1F;
			int32_t value = static_cast<int32_t>(GPR[rS]);
			GPR[rA] = value >> SH;
			LOG_INFO("CPU", "srawi r%d = r%d >> %d", rA, rS, SH);
			break;
		}
		case 24: { // slw
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			GPR[rA] = GPR[rS] << (GPR[rB] & 0x1F);
			LOG_INFO("CPU", "slw r%d = r%d << r%d", rA, rS, rB);
			break;
		}
		case 40: { // neg
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			GPR[rD] = -static_cast<int32_t>(GPR[rA]);
			LOG_INFO("CPU", "neg r%d = -r%d", rD, rA);
			break;
		}
		case 331: { // divw
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			if (GPR[rB] != 0) {
				GPR[rD] = static_cast<int32_t>(GPR[rA]) / static_cast<int32_t>(GPR[rB]);
				LOG_INFO("CPU", "divw r%d = r%d / r%d", rD, rA, rB);
			}
			else {
				LOG_WARNING("CPU", "Division by zero in divw");
				running = false;
			}
			break;
		}
		case 922: { // extsb
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			GPR[rA] = static_cast<int32_t>(static_cast<int8_t>(GPR[rS] & 0xFF));
			LOG_INFO("CPU", "extsb r%d = r%d", rA, rS);
			break;
		}
		case 954: { // extsh
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rS = (instr >> 21) & 0x1F;
			GPR[rA] = static_cast<int32_t>(static_cast<int16_t>(GPR[rS] & 0xFFFF));
			LOG_INFO("CPU", "extsh r%d = r%d", rA, rS);
			break;
		}
		case 289: { // mfctr
			uint32_t rD = (instr >> 21) & 0x1F;
			GPR[rD] = CTR;
			LOG_INFO("CPU", "mfctr r%d", rD);
			break;
		}
		case 451: { // mtctr
			uint32_t rS = (instr >> 21) & 0x1F;
			CTR = GPR[rS];
			LOG_INFO("CPU", "mtctr r%d", rS);
			break;
		}
		case 136: { // lwarx
			uint32_t rD = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + (rB == 0 ? 0 : GPR[rB]);
			GPR[rD] = mmu->Read32(addr);
			LOG_INFO("CPU", "lwarx r%d from 0x%08X", rD, addr);
			break;
		}
		case 146: { // mtmsr
			if (MSR & 0x4000) {
				TriggerException(0xC00);
				break;
			}
			MSR = GPR[rS];
			LOG_INFO("CPU", "mtmsr MSR = r%d (0x%08X)", rS, MSR);
			break;
		}
		case 598: { // isync (Corregido)
			// Simula vaciado de pipeline
			LOG_INFO("CPU", "isync (pipeline flushed)");
			break;
		}
		case 247: { // stwux (Corregido)
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = GPR[rA] + GPR[rB];
			mmu->Write32(addr, GPR[rS]);
			GPR[rA] = addr;
			LOG_INFO("CPU", "stwux r%d to 0x%08X, update r%d", rS, addr, rA);
			break;
		}
		case 86: { // dcbf
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + (rB == 0 ? 0 : GPR[rB]);
			// Simula flush de bloque de caché (invalidate y write-back)
			LOG_INFO("CPU", "dcbf at 0x%08X", addr);
			break;
		}
		case 54: { // dcbst
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + (rB == 0 ? 0 : GPR[rB]);
			// Simula write-back de bloque de caché
			LOG_INFO("CPU", "dcbst at 0x%08X", addr);
			break;
		}
		case 278: { // dcbt
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + (rB == 0 ? 0 : GPR[rB]);
			// Simula precarga de bloque de caché
			LOG_INFO("CPU", "dcbt at 0x%08X", addr);
			break;
		}
		case 1014: { // icbi
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + (rB == 0 ? 0 : GPR[rB]);
			// Simula invalidación de bloque de caché de instrucciones
			LOG_INFO("CPU", "icbi at 0x%08X", addr);
			break;
		}
		case 306: { // tlbie
			uint32_t rB = (instr >> 11) & 0x1F;
			// Simula invalidación de entrada TLB
			LOG_INFO("CPU", "tlbie r%d", rB);
			break;
		}
		case 370: { // tlbsync
			// Simula sincronización de TLB
			LOG_INFO("CPU", "tlbsync");
			break;
		}
		case 284: { // mtdec
			//if (((instr >> 16) & 0x1F) | ((instr >> 11) & 0x1F) << 5 == 284) {
			uint32_t rS = (instr >> 21) & 0x1F;
			SPR[284] = GPR[rS];
			LOG_INFO("CPU", "mtdec DEC = r%d (0x%08X)", rS, GPR[rS]);
			//}
			break;
		}
		case 131: { // wrteei
			uint32_t E = (instr >> 15) & 1;
			MSR = (MSR & ~0x8000) | (E << 15);
			LOG_INFO("CPU", "wrteei EE=%d", E);
			break;
		}
		case 130: { // wrtee
			uint32_t rS = (instr >> 21) & 0x1F;
			MSR = (MSR & ~0x8000) | (GPR[rS] & 0x8000);
			LOG_INFO("CPU", "wrtee EE = r%d", rS);
			break;
		}
		case 4: { // twi TO, BI, AA
			uint32_t TO = (instr >> 21) & 0x1F;
			uint32_t BI = (instr >> 16) & 0x1F;
			int16_t  AA = instr & 0xFFFF;            // displacement
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t rB = (instr >> 11) & 0x1F;
			int32_t  a = static_cast<int32_t>(GPR[rA]);
			int32_t  b = static_cast<int32_t>(GPR[rB]);
			bool     trap = false;

			// condiciones de trap según los bits de TO:
			if ((TO & 0x10) && (a < b)) trap = true;
			if ((TO & 0x08) && (a > b)) trap = true;
			if ((TO & 0x04) && (a == b)) trap = true;
			if ((TO & 0x02) && (static_cast<uint32_t>(a) < static_cast<uint32_t>(b))) trap = true;
			if ((TO & 0x01) && (static_cast<uint32_t>(a) > static_cast<uint32_t>(b))) trap = true;

			if (trap) {
				TriggerException(0x700);              // vector de excepción de programa
				LOG_INFO("CPU", "twi trap (TO=0x%02X) at PC=0x%08X", TO, PC);
			}
			else {
				LOG_INFO("CPU", "twi not taken (TO=0x%02X)", TO);
			}
			break;
		}
		case 0x112: { // rlwinm
			uint32_t rS = (instr >> 21) & 0x1F;
			uint32_t rA = (instr >> 16) & 0x1F;
			uint32_t SH = (instr >> 11) & 0x1F;
			uint32_t MB = (instr >> 6) & 0x1F;
			uint32_t ME = (instr >> 1) & 0x1F;

			uint32_t mask = ((0xFFFFFFFF >> MB) & (0xFFFFFFFF << (31 - ME)));
			GPR[rA] = ((GPR[rS] << SH) | (GPR[rS] >> (32 - SH))) & mask;

			LOG_INFO("CPU", "rlwinm r%u = (r%u << %u | r%u >> %u) & 0x%08X -> 0x%08X",
				rA, rS, SH, rS, 32 - SH, mask, GPR[rA]);
			break;
		}
		default:
			LOG_WARNING("CPU", "Unknown ext_opcode 0x%03X", ext_opcode);
			//running = false;
		}
		break;
	}
	case 32: { // lwz rD, d(rA)
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		if (addr % 4 != 0) TriggerException(0x600); // Alignment exception
		uint8_t  b[4];
		mmu->Read(addr, b, 4);
		GPR[rD] = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]; // Correcto para big-endian
		LOG_INFO("CPU", "lwz r%d from [0x%08X] = 0x%08X", rD, addr, GPR[rD]);
		break;
	}
	case 33: { // lwzu rD, d(rA) ; update rA
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		GPR[rA] = addr;
		uint8_t b[4];
		mmu->Read(addr, b, 4);
		GPR[rD] = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
		LOG_INFO("CPU", "lwzu r%d from [0x%08X], r%d←0x%08X", rD, addr, rA, addr);
		break;
	}
	case 34: { // lbz rD, d(rA)
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		uint8_t  v;
		mmu->Read(addr, &v, 1);
		GPR[rD] = v;
		LOG_INFO("CPU", "lbz r%d from [0x%08X] = 0x%02X", rD, addr, v);
		break;
	}
	case 35: { // lbzu rD, d(rA)
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		GPR[rA] = addr;
		uint8_t  v;
		mmu->Read(addr, &v, 1);
		GPR[rD] = v;
		LOG_INFO("CPU", "lbzu r%d from [0x%08X], r%d←0x%08X", rD, addr, rA, addr);
		break;
	}
	case 36: { // stw
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t ea = (rA == 0 ? 0 : GPR[rA]) + d;
		if (addr % 4 != 0) TriggerException(0x600); // Alignment exception
		mmu->Write32(ea, GPR[rS]);
		break;
	}
	case 37: { // stwu
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t ea = (rA == 0 ? 0 : GPR[rA]) + d;
		mmu->Write32(ea, GPR[rS]);
		GPR[rA] = ea;
		break;
	}
	case 38: { // stb rS, d(rA)
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		uint8_t  v = uint8_t(GPR[rS] & 0xFF);
		mmu->Write(addr, &v, 1);
		LOG_INFO("CPU", "stb r%d to [0x%08X] = 0x%02X", rS, addr, v);
		break;
	}
	case 39: { // stbu rS, d(rA)
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t  d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		GPR[rA] = addr;
		uint8_t v = uint8_t(GPR[rS] & 0xFF);
		mmu->Write(addr, &v, 1);
		LOG_INFO("CPU", "stbu r%d to [0x%08X], r%d←0x%08X", rS, addr, rA, addr);
		break;
	}
	case 40: { // lhz
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		GPR[rD] = mmu->Read32(addr);
		LOG_INFO("CPU", "lhz r%d from 0x%08X", rD, addr);
		break;
	}
	case 41: { // stfs
		uint32_t frS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		float val = static_cast<float>(FPR[frS]);
		mmu->Write32(addr, *reinterpret_cast<uint32_t*>(&val));
		PC += 4;
		break;
	}
	case 43: { // sth
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = instr & 0xFFFF;
		uint32_t addr = (rA == 0 ? 0 : GPR[rA]) + d;
		mmu->Write32(addr, GPR[rS] & 0xFFFF);
		PC += 4;
		break;
	}
	case 44: { // sth
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		mmu->Write32(addr, static_cast<u16>(GPR[rS]));
		LOG_INFO("CPU", "sth r%d to 0x%08X", rS, addr);
		break;
	}
	case 46: { // lmw
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		for (uint32_t i = rD; i < 32; ++i) {
			GPR[i] = mmu->Read32(addr);
			addr += 4;
		}
		LOG_INFO("CPU", "lmw r%d from 0x%08X", rD, addr);
		break;
	}
	case 47: { // stmw
		uint32_t rS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		for (uint32_t i = rS; i < 32; ++i) {
			mmu->Write32(addr, GPR[i]);
			addr += 4;
		}
		LOG_INFO("CPU", "stmw r%d to 0x%08X", rS, addr);
		break;
	}
	case 48: { // lfs
		uint32_t frD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		uint32_t value = mmu->Read32(addr);
		FPR[frD] = *reinterpret_cast<float*>(&value);
		LOG_INFO("CPU", "lfs fr%d from 0x%08X", frD, addr);
		break;
	}
	case 49: { // lfsu
		uint32_t frD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = GPR[rA] + d;
		uint32_t value = mmu->Read32(addr);
		FPR[frD] = *reinterpret_cast<float*>(&value);
		GPR[rA] = addr;
		LOG_INFO("CPU", "lfsu fr%d from 0x%08X, update r%d", frD, addr, rA);
		break;
	}
	case 50: { // stfs
		uint32_t frS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		uint32_t value = *reinterpret_cast<uint32_t*>(&FPR[frS]);
		mmu->Write32(addr, value);
		LOG_INFO("CPU", "stfs fr%d to 0x%08X", frS, addr);
		break;
	}
	case 51: { // stfsu
		uint32_t frS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = GPR[rA] + d;
		uint32_t value = *reinterpret_cast<uint32_t*>(&FPR[frS]);
		mmu->Write32(addr, value);
		GPR[rA] = addr;
		LOG_INFO("CPU", "stfsu fr%d to 0x%08X, update r%d", frS, addr, rA);
		break;
	}
	case 52: { // lfd
		uint32_t frD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		uint64_t value = mmu->Read64(addr);
		FPR[frD] = *reinterpret_cast<double*>(&value);
		LOG_INFO("CPU", "lfd fr%d from 0x%08X", frD, addr);
		break;
	}
	case 54: { // stfd
		uint32_t frS = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t d = static_cast<int16_t>(instr & 0xFFFF);
		uint32_t addr = (rA == 0 ? d : GPR[rA] + d);
		uint64_t value = *reinterpret_cast<uint64_t*>(&FPR[frS]);
		mmu->Write64(addr, value);
		LOG_INFO("CPU", "stfd fr%d to 0x%08X", frS, addr);
		break;
	}
	case 58: { // ld, std
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		int16_t ds = static_cast<int16_t>(instr & 0xFFFC);
		uint32_t addr = (rA == 0 ? ds : GPR[rA] + ds);
		if ((instr & 0x3) == 0) { // ld
			GPR[rD] = mmu->Read32(addr);
			LOG_INFO("CPU", "ld r%d from 0x%08X", rD, addr);
		}
		else if ((instr & 0x3) == 1) { // std
			mmu->Write64(addr, GPR[rD]);
			LOG_INFO("CPU", "std r%d to 0x%08X", rD, addr);
		}
		break;
	}
	case 59: { // fadd, fsub, fmul, fdiv
		uint32_t frD = (instr >> 21) & 0x1F;
		uint32_t frA = (instr >> 16) & 0x1F;
		uint32_t frB = (instr >> 11) & 0x1F;
		uint32_t ext_opcode = (instr >> 1) & 0x1F;
		switch (ext_opcode) {
		case 4: { // fsubs (single-precision floating-point subtract)
			FPR[frD] = FPR[frA] - FPR[frB];
			FPSCR &= ~(FPSCRegs.FN | FPSCRegs.FP | FPSCRegs.FZ | FPSCRegs.FU); // Clear FPCC bits
			if (isnan(FPR[frD])) {
				FPSCR |= FPSCRegs.FU | FPSCRegs.VX | FPSCRegs.FX;
			}
			else if (FPR[frD] == 0.0f) {
				FPSCR |= FPSCRegs.FZ;
			}
			else if (FPR[frD] < 0.0f) {
				FPSCR |= FPSCRegs.FN;
			}
			else {
				FPSCR |= FPSCRegs.FP;
			}
			if (isinf(FPR[frD])) {
				FPSCR |= FPSCRegs.OX | FPSCRegs.FX;
			}
			PC += 4;
			break;
		}
		case 21: { // fadd
			FPR[frD] = FPR[frA] + FPR[frB];			
			// Actualizar FPSCR
			if (std::isnan(FPR[frD])) FPSCR |= FPSCRegs.VX;
			else if (FPR[frD] == 0.0) FPSCR |= FPSCRegs.FZ;
			LOG_INFO("CPU", "fadd fr%d = fr%d + fr%d", frD, frA, frB);
			break;
		}
		case 18: { // fsub
			FPR[frD] = FPR[frA] - FPR[frB];
			LOG_INFO("CPU", "fsub fr%d = fr%d - fr%d", frD, frA, frB);
			break;
		}
		case 20: { // fmul
			FPR[frD] = FPR[frA] * FPR[frB];
			LOG_INFO("CPU", "fmul fr%d = fr%d * fr%d", frD, frA, frB);
			break;
		}
		case 25: { // fdiv
			if (FPR[frB] != 0.0) {
				FPR[frD] = FPR[frA] / FPR[frB];
				LOG_INFO("CPU", "fdiv fr%d = fr%d / fr%d", frD, frA, frB);
			}
			else {
				LOG_WARNING("CPU", "Division by zero in fdiv");
				running = false;
			}
			break;
		}
		case 15: { // fres
			double b = FPR[frB];
			double est = 1.0 / b;
			est = est * (2.0 - b * est); // Una iteración de Newton-Raphson
			FPR[frD] = est;
			break;
		}
		case 24: { // frsqrte
			if (FPR[frB] >= 0.0) {
				FPR[frD] = 1.0 / std::sqrt(FPR[frB]);
				LOG_INFO("CPU", "frsqrte fr%d = 1 / sqrt(fr%d)", frD, frB);
			}
			else {
				LOG_WARNING("CPU", "Invalid reciprocal sqrt in frsqrte");
				running = false;
			}
			break;
		}
		default:
			LOG_WARNING("CPU", "Unknown FP opcode 0x%02X", ext_opcode);
			//running = false;
		}
		break;
	}
	case 63: {
		uint32_t crfD = (instr >> 23) & 0x7;
		uint32_t frD = (instr >> 21) & 0x1F;
		uint32_t frA = (instr >> 16) & 0x1F;
		uint32_t frB = (instr >> 11) & 0x1F;
		uint32_t frC = (instr >> 6) & 0x1F;
		uint32_t XO = (instr >> 1) & 0x3FF;
		switch (XO) {
		case 0: { // fcmpu
			double a = FPR[frA], b = FPR[frB];
			uint32_t cr = (a < b) ? 0x8 : (a > b) ? 0x4 : 0x2;
			if (std::isnan(a) || std::isnan(b)) {
				FPSCR |= 0x80000000; // Set VX (invalid operation)
				cr = 0x1;
			}
			CR.value = (CR.value & ~(0xF << (28 - 4 * crfD))) | (cr << (28 - 4 * crfD));
			LOG_INFO("CPU", "fcmpu cr%d, fr%d, fr%d", crfD, frA, frB);
			break;
		}
		case 12: { // frsp
			FPR[frD] = static_cast<float>(FPR[frB]);
			LOG_INFO("CPU", "frsp fr%d = (float)fr%d", frD, frB);
			break;
		}
		case 15: { // fres
			if (FPR[frB] != 0.0) {
				FPR[frD] = 1.0 / FPR[frB];
				LOG_INFO("CPU", "fres fr%d = 1 / fr%d", frD, frB);
			}
			else {
				FPSCR |= 0x80000000; // Set VX
				LOG_WARNING("CPU", "Invalid reciprocal in fres");
				running = false;
			}
			break;
		}
		case 18: { // fmr
			FPR[frD] = FPR[frB];
			LOG_INFO("CPU", "fmr f%d = f%d", frD, frB);
			break;
		}
		case 18 | 0x20: { // fadd
			FPR[frD] = FPR[frA] + FPR[frB];
			LOG_INFO("CPU", "fadd f%d = f%d + f%d", frD, frA, frB);
			break;
		}
		case 18 | 0x40: { // fdiv
			if (FPR[frB] != 0.0)
				FPR[frD] = FPR[frA] / FPR[frB];
			else
				LOG_WARNING("CPU", "fdiv by zero at PC=0x%08X", PC);
			LOG_INFO("CPU", "fdiv f%d = f%d / f%d", frD, frA, frB);
			break;
		}
		case 20 | 0x20: { // fsub
			FPR[frD] = FPR[frA] - FPR[frB];
			LOG_INFO("CPU", "fsub f%d = f%d - f%d", frD, frA, frB);
			break;
		}
		case 25 | 0x20: { // fmul
			FPR[frD] = FPR[frA] * FPR[frC];
			LOG_INFO("CPU", "fmul f%d = f%d * f%d", frD, frA, frC);
			break;
		}
		case 21: { // fneg
			FPR[frD] = -FPR[frB];
			LOG_INFO("CPU", "fneg f%d = -f%d", frD, frB);
			break;
		}
		case 24: { // frsqrte
			if (FPR[frB] >= 0.0) {
				FPR[frD] = 1.0 / std::sqrt(FPR[frB]);
				LOG_INFO("CPU", "frsqrte fr%d = 1 / sqrt(fr%d)", frD, frB);
			}
			else {
				FPSCR |= 0x80000000; // Set VX
				LOG_WARNING("CPU", "Invalid reciprocal sqrt in frsqrte");
				running = false;
			}
			break;
		}
		case 25: { // fsel
			FPR[frD] = (FPR[frA] >= 0.0) ? FPR[frC] : FPR[frB];
			LOG_INFO("CPU", "fsel fr%d = fr%d >= 0 ? fr%d : fr%d", frD, frA, frC, frB);
			break;
		}
		case 29: { // fmsub (nueva)
			FPR[frD] = FPR[frA] * FPR[frC] - FPR[frB];
			LOG_INFO("CPU", "fmsub fr%d = fr%d * fr%d - fr%d", frD, frA, frC, frB);
			break;
		}
		case 31: { // fnmsub (nueva)
			FPR[frD] = -(FPR[frA] * FPR[frC] - FPR[frB]);
			LOG_INFO("CPU", "fnmsub fr%d = -(fr%d * fr%d - fr%d)", frD, frA, frC, frB);
			break;
		}
		case 28: { // fmadd (nueva)
			FPR[frD] = FPR[frA] * FPR[frC] + FPR[frB];
			LOG_INFO("CPU", "fmadd fr%d = fr%d * fr%d + fr%d", frD, frA, frC, frB);
			break;
		}
		case 30: { // fnmadd (nueva)
			FPR[frD] = -(FPR[frA] * FPR[frC] + FPR[frB]);
			LOG_INFO("CPU", "fnmadd fr%d = -(fr%d * fr%d + fr%d)", frD, frA, frC, frB);
			break;
		}
		case 32: { // fsqrt
			if (FPR[frB] >= 0.0) {
				FPR[frD] = std::sqrt(FPR[frB]);
				LOG_INFO("CPU", "fsqrt fr%d = sqrt(fr%d)", frD, frB);
			}
			else {
				FPSCR |= 0x80000000; // Set VX
				LOG_WARNING("CPU", "Invalid sqrt in fsqrt");
				running = false;
			}
			break;
		}
		case 70: { // mffs
			GPR[frD] = FPSCR;
			LOG_INFO("CPU", "mffs fr%d = FPSCR (0x%08X)", frD, FPSCR);
			break;
		}
		case 583: { // mtfsf
			uint32_t FM = (instr >> 17) & 0xFF;
			uint32_t frB = (instr >> 11) & 0x1F;
			for (int i = 0; i < 8; ++i) {
				if (FM & (1 << i)) {
					FPSCR = (FPSCR & ~(0xF << (4 * (7 - i)))) |
						(static_cast<uint32_t>(FPR[frB]) & (0xF << (4 * (7 - i))));
				}
			}
			LOG_INFO("CPU", "mtfsf FPSCR = fr%d (FM=0x%02X)", frB, FM);
			break;
		}
		default:
			LOG_WARNING("CPU", "Unknown FP ext_opcode 0x%03X at PC 0x%08X", XO, PC);
			//running = false;
			break;
		}
		break;
	}
	case 62: { // 0x3E: NOP
		LOG_INFO("CPU", "nop at PC=0x%08X", PC);
		break;
	}
	/*Hooks*/
	case 0x90: { // stw
		mmu->Write32(addr, GPR[rs]);
		check_framebuffer_hook(addr, GPR[rs], "STW");
		PC += 4;
		break;
	}
	case 0x94: { // stwu
		addr = GPR[ra] + offset;
		GPR[ra] = addr;
		mmu->Write32(addr, GPR[rs]);
		check_framebuffer_hook(addr, GPR[rs], "STWU");
		PC += 4;
		break;
	}

	case 0xF0: { // std
		mmu->Write64(addr, GPR[rs]);
		check_framebuffer_hook(addr, GPR[rs], "STD");
		PC += 4;
		break;
	}
	case 0xF8: { // std
		uint32_t addr = GPR[ra] + offset;
		check_framebuffer_hook(addr, GPR[rs], "STD");
		mmu->Write64(addr, GPR[rs]);
		PC += 4;
		break;
	}

	case 0x98: { // stb
		mmu->Write8(addr, GPR[rs] & 0xFF);
		check_framebuffer_hook(addr, GPR[rs] & 0xFF, "STB");
		PC += 4;
		break;
	}

	case 0xD0: { // stfs (almacena float simple)
		float fval;
		memcpy(&fval, &FPR[rs], sizeof(float));
		mmu->Write32(addr, *reinterpret_cast<uint32_t*>(&fval));
		check_framebuffer_hook(addr, *reinterpret_cast<uint32_t*>(&fval), "STFS");
		PC += 4;
		break;
	}	
	case 235: { // mullw (sub-opcode 235)
		uint32_t rD = (instr >> 21) & 0x1F;
		uint32_t rA = (instr >> 16) & 0x1F;
		uint32_t rB = (instr >> 11) & 0x1F;
		int64_t result = static_cast<int64_t>(static_cast<int32_t>(GPR[rA]))
			* static_cast<int32_t>(GPR[rB]);
		if (result != (int32_t)result) XER |= 0x40000000; // Set overflow bit
		GPR[rD] = static_cast<uint32_t>(result & 0xFFFFFFFF); // Guarda los 32 bits bajos
		LOG_INFO("CPU", "mullw r%d = r%d * r%d (result=0x%08X)", rD, rA, rB, GPR[rD]);
		break;
	}
	default:		
		LOG_DEBUG("Disasm", "0x%08X: 0x%08X opcode=0x%X", PC, instr, opcode);
		//running = false;
		break;
	}
}
