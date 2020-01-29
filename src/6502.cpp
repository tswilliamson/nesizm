
// main 6502 interpreter and emulation
#include "platform.h"
#include "debug.h"
#include "scope_timer/scope_timer.h"

#define NES 1
#include "6502.h"

#include "6502_instr_timing.inl"

#if TRACE_DEBUG
static unsigned int cpuBreakpoint = 0x10000;
static unsigned int memWriteBreakpoint = 0x10000;
static unsigned int instructionCountBreakpoint = 0;
static bool bHitMemBreakpoint = false;
static bool bHitPPUBreakpoint = false;

#define NUM_TRACED 500
static cpu_instr_history traceHistory[NUM_TRACED] = { 0 };
static unsigned int traceNum;
static unsigned int cpuInstructionCount = 0;
void HitBreakpoint();
void IllegalInstruction();
void Do_PPUBreakpoint();
static int traceLineRemaining = 0;
#endif

// the low 5 bits of the opcode determines the addressing mode with only 5 instruction exceptions (noted)
const static int modeTableSmall[32] = {
	AM_None,		// 00
	AM_IndirectX,	// 01
	AM_None,		// 02 (mostly unused)
	AM_None,		// 03 (unused)
	AM_Zero,		// 04
	AM_Zero,		// 05
	AM_Zero,		// 06
	AM_None,		// 07 (unused)

	AM_None,		// 08 (all implied)
	AM_None,		// 09 (all immediate)
	AM_None,		// 0A (A register or implied)
	AM_None,		// 0B (unused)
	AM_Absolute,	// 0C (exceptions at 4C and 6C with JMPs)
	AM_Absolute,	// 0D 
	AM_Absolute,	// 0E
	AM_None,		// 0F (unused)

	AM_None,		// 10 (all relative JMPs and branches)
	AM_IndirectY,	// 11
	AM_None,		// 12 (unused)
	AM_None,		// 13 (unused)
	AM_ZeroX,		// 14 (only 94 and B4)
	AM_ZeroX,		// 15
	AM_ZeroX,		// 16 (exceptions at 96 and B6 which are ZeroY)
	AM_None,		// 17 (unused)

	AM_None,		// 18 (all implied)
	AM_AbsoluteY,	// 19
	AM_None,		// 1A (only used for 9A and BA)
	AM_None,		// 1B (unused)
	AM_AbsoluteX,	// 1C (only used for BC)
	AM_AbsoluteX,	// 1D
	AM_AbsoluteX,	// 1E (exception at BE which is AbsoluteY)
	AM_None,		// 1F (unused)
};

static int modeTable[256];

void cpu6502_Init() {
	for (int i = 0; i < 256; i++) {
		modeTable[i] = modeTableSmall[i & 0x1F];
	}
	// exceptions
	modeTable[0x96] = AM_ZeroY;
	modeTable[0xB6] = AM_ZeroY;
	modeTable[0xBE] = AM_AbsoluteY;

	RegisterInstructionTimers();
}

void resolveToP() {
	mainCPU.P = (mainCPU.P & (~ST_ZRO & ~ST_NEG & ~ST_CRY)) |
		((mainCPU.zeroResult == 0) ? ST_ZRO : 0) |
		(mainCPU.carryResult) |
		(mainCPU.negativeResult & ST_NEG);
}

void resolveFromP() {
	mainCPU.zeroResult = (~mainCPU.P & ST_ZRO);
	mainCPU.negativeResult = (mainCPU.P & ST_NEG);
	mainCPU.carryResult = (mainCPU.P & ST_CRY);
}

FORCE_INLINE void writeAddr(unsigned int addr, unsigned int result) {
	mainCPU.write(addr, result);

#if TRACE_DEBUG
	if (addr == memWriteBreakpoint) {
		bHitMemBreakpoint = true;
	}
#endif
}

FORCE_INLINE void latchWriteAddr(unsigned int addr, unsigned int result) {
	mainCPU.accessTable[addr >> 13] = addr;
	writeAddr(addr, result);
}

FORCE_INLINE void writeZero(unsigned int addr, unsigned int result) {
	CPU_RAM(addr) = result;

#if TRACE_DEBUG
	if (addr == memWriteBreakpoint) {
		bHitMemBreakpoint = true;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STORE / LOAD

// LDA #$NN (load A immediate)
FORCE_INLINE void LDA(unsigned int data) {
	mainCPU.A = data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void LDA_MEM(unsigned int address) {
	LDA(mainCPU.read(address));
}

FORCE_INLINE void LDA_ZERO(unsigned int address) {
	LDA(CPU_RAM(address));
}

FORCE_INLINE void LDX(unsigned int data) {
	mainCPU.X = data;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

FORCE_INLINE void LDX_MEM(unsigned int address) {
	LDX(mainCPU.read(address));
}

FORCE_INLINE void LDX_ZERO(unsigned int address) {
	LDX(CPU_RAM(address));
}

FORCE_INLINE void LDY(unsigned int data) {
	mainCPU.Y = data;
	mainCPU.zeroResult = mainCPU.Y;
	mainCPU.negativeResult = mainCPU.Y;
}

FORCE_INLINE void LDY_MEM(unsigned int address) {
	LDY(mainCPU.read(address));
}

FORCE_INLINE void LDY_ZERO(unsigned int address) {
	LDY(CPU_RAM(address));
}

FORCE_INLINE void STA_MEM(unsigned int address) {
	latchWriteAddr(address, mainCPU.A);
}

FORCE_INLINE void STA_ZERO(unsigned int address) {
	writeZero(address, mainCPU.A);
}

FORCE_INLINE void STX_MEM(unsigned int address) {
	latchWriteAddr(address, mainCPU.X);
}

FORCE_INLINE void STX_ZERO(unsigned int address) {
	writeZero(address, mainCPU.X);
}

FORCE_INLINE void STY_MEM(unsigned int address) {
	latchWriteAddr(address, mainCPU.Y);
}

FORCE_INLINE void STY_ZERO(unsigned int address) {
	writeZero(address, mainCPU.Y);
}

FORCE_INLINE void TAX() {
	mainCPU.X = mainCPU.A;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void TXA() {
	mainCPU.A = mainCPU.X;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void TAY() {
	mainCPU.Y = mainCPU.A;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void TYA() {
	mainCPU.A = mainCPU.Y;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void TSX() {
	mainCPU.X = mainCPU.SP;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

FORCE_INLINE void TXS() {
	mainCPU.SP = mainCPU.X;
}

FORCE_INLINE void PHA() {
	mainCPU.push(mainCPU.A);
}

FORCE_INLINE void PLA() {
	mainCPU.A = mainCPU.pop();
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void PHP() {
	resolveToP();
	mainCPU.push(mainCPU.P | ST_UNUSED | ST_BRK);
}

// PLP (pop processor status ignoring bit 4)
FORCE_INLINE void PLP() {
	mainCPU.P = mainCPU.pop() & ~(ST_BRK);
	resolveFromP();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BRANCH / JUMP

FORCE_INLINE void takeBranch(unsigned int data) {
	unsigned int oldPC = mainCPU.PC;
	mainCPU.PC += (char) (data);
	mainCPU.clocks++;
	if ((oldPC ^ mainCPU.PC) & 0x100) mainCPU.clocks++;
}

FORCE_INLINE void BPL(unsigned int data) {
	if (!(mainCPU.negativeResult & ST_NEG)) {
		takeBranch(data);
	}
}

FORCE_INLINE void BMI(unsigned int data) {
	if (mainCPU.negativeResult & ST_NEG) {
		takeBranch(data);
	}
}

FORCE_INLINE void BVC(unsigned int data) {
	if (!(mainCPU.P & ST_OVR)) {
		takeBranch(data);
	}
}

FORCE_INLINE void BVS(unsigned int data) {
	if (mainCPU.P & ST_OVR) {
		takeBranch(data);
	}
}

FORCE_INLINE void BCC(unsigned int data) {
	if (mainCPU.carryResult == 0) {
		takeBranch(data);
	}
}

FORCE_INLINE void BCS(unsigned int data) {
	if (mainCPU.carryResult) {
		takeBranch(data);
	}
}

FORCE_INLINE void BNE(unsigned int data) {
	if (mainCPU.zeroResult) {
		takeBranch(data);
	}
}

FORCE_INLINE void BEQ(unsigned int data) {
	if (!mainCPU.zeroResult) {
		takeBranch(data);
	}
}

FORCE_INLINE void JMP_MEM(unsigned int addr) {
	// common infinite loop
	if (mainCPU.PC == addr + 3) {
		// skip ahead until next interrupt
		for (; mainCPU.clocks < mainCPU.nextClocks;) {
			mainCPU.clocks += 3;
		}
	}

	mainCPU.PC = addr;
}

FORCE_INLINE void JSR_MEM(unsigned int addr) {
	// JSR (subroutine)
	mainCPU.push(mainCPU.PC >> 8);
	mainCPU.push(mainCPU.PC & 0xFF);

	mainCPU.PC = addr;
}

FORCE_INLINE void RTI() {
	// RTI (return from interrupt)
	// TODO : Non- delayed IRQ response behavior?
	mainCPU.P = mainCPU.pop() & ~(ST_BRK);
	mainCPU.PC = mainCPU.pop() | (mainCPU.pop() << 8);
	resolveFromP();
}
			
FORCE_INLINE void RTS() {
	// RTS
	mainCPU.PC = mainCPU.pop() | (mainCPU.pop() << 8);
	mainCPU.PC++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ALU

FORCE_INLINE void ADC(unsigned int data) {
	unsigned int result = mainCPU.A + data + mainCPU.carryResult;
	mainCPU.P =
		(mainCPU.P & (ST_INT | ST_BRK | ST_BCD | ST_UNUSED)) |					// keep flags
		(((mainCPU.A^result)&(data^result) & 0x80) >> (7 - ST_OVR_BIT));		// overflow (http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html)
	mainCPU.A = result & 0xFF;
	mainCPU.carryResult = result >> 8;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void ADC_MEM(unsigned int address) {
	ADC(mainCPU.read(address));
}

FORCE_INLINE void ADC_ZERO(unsigned int address) {
	ADC(CPU_RAM(address));
}

FORCE_INLINE void SBC(unsigned int data) {
	// TODO : possibly move overflow calculation to flag resolve?
	unsigned int result = mainCPU.A - data - 1 + mainCPU.carryResult;
	mainCPU.P =
		(mainCPU.P & (ST_INT | ST_BRK | ST_BCD | ST_UNUSED)) |					// keep flags
		(((mainCPU.A^result)&((~(data)) ^ result) & 0x80) >> (7 - ST_OVR_BIT));	// overflow (http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html)
	mainCPU.A = result & 0xFF;
	mainCPU.carryResult = (~result & 0x100) >> 8;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void SBC_MEM(unsigned int address) {
	SBC(mainCPU.read(address));
}

FORCE_INLINE void SBC_ZERO(unsigned int address) {
	SBC(CPU_RAM(address));
}

FORCE_INLINE void DEC_MEM(unsigned int address) {
	unsigned int result = (mainCPU.read(address) - 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeAddr(address, result);
}

FORCE_INLINE void DEC_ZERO(unsigned int address) {
	unsigned int result = (CPU_RAM(address) - 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeZero(address, result);
}

FORCE_INLINE void INC_MEM(unsigned int address) {
	unsigned int result = (mainCPU.read(address) + 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeAddr(address, result);
}

FORCE_INLINE void INC_ZERO(unsigned int address) {
	unsigned int result = (CPU_RAM(address) + 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeZero(address, result);
}

FORCE_INLINE void DEX() {
	// DEX (decrement X)
	mainCPU.X = (mainCPU.X - 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

FORCE_INLINE void INX() {
	// INX (increment X)
	mainCPU.X = (mainCPU.X + 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

FORCE_INLINE void DEY() {
	// DEY (decrement Y)
	mainCPU.Y = (mainCPU.Y - 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.Y;
	mainCPU.negativeResult = mainCPU.Y;
}

FORCE_INLINE void INY() {
	// INY (increment Y)
	mainCPU.Y = (mainCPU.Y + 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.Y;
	mainCPU.negativeResult = mainCPU.Y;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COMPARE / TEST

FORCE_INLINE void CMP(unsigned int data) {
	mainCPU.carryResult = (data <= mainCPU.A) ? 1 : 0;
	mainCPU.zeroResult = (mainCPU.A - data);
	mainCPU.negativeResult = mainCPU.zeroResult;
}

FORCE_INLINE void CMP_MEM(unsigned int addr) {
	CMP(mainCPU.read(addr));
}

FORCE_INLINE void CMP_ZERO(unsigned int addr) {
	CMP(CPU_RAM(addr));
}

FORCE_INLINE void CPX(unsigned int data) {
	mainCPU.carryResult = (data <= mainCPU.X) ? 1 : 0;
	mainCPU.zeroResult = (mainCPU.X - data);
	mainCPU.negativeResult = mainCPU.zeroResult;
}

FORCE_INLINE void CPX_MEM(unsigned int addr) {
	CPX(mainCPU.read(addr));
}

FORCE_INLINE void CPX_ZERO(unsigned int addr) {
	CPX(CPU_RAM(addr));
}

FORCE_INLINE void CPY(unsigned int data) {
	mainCPU.carryResult = (data <= mainCPU.Y) ? 1 : 0;
	mainCPU.zeroResult = (mainCPU.Y - data);
	mainCPU.negativeResult = mainCPU.zeroResult;
}

FORCE_INLINE void CPY_MEM(unsigned int addr) {
	CPY(mainCPU.read(addr));
}

FORCE_INLINE void CPY_ZERO(unsigned int addr) {
	CPY(CPU_RAM(addr));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MISC

FORCE_INLINE void BRK() {
	// BRK moves forward an instruction
	cpu6502_SoftwareInterrupt(0xFFFE);
}

FORCE_INLINE void NOP() {
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BITWISE

FORCE_INLINE void ORA(unsigned int data) {
	mainCPU.A = mainCPU.A | data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void ORA_MEM(unsigned int address) {
	ORA(mainCPU.read(address));
}

FORCE_INLINE void ORA_ZERO(unsigned int address) {
	ORA(CPU_RAM(address));
}

FORCE_INLINE void AND(unsigned int data) {
	mainCPU.A = mainCPU.A & data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void AND_MEM(unsigned int address) {
	AND(mainCPU.read(address));
}

FORCE_INLINE void AND_ZERO(unsigned int address) {
	AND(CPU_RAM(address));
}

FORCE_INLINE void EOR(unsigned int data) {
	mainCPU.A = mainCPU.A ^ data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void EOR_MEM(unsigned int address) {
	EOR(mainCPU.read(address));
}

FORCE_INLINE void EOR_ZERO(unsigned int address) {
	EOR(CPU_RAM(address));
}

FORCE_INLINE void ASL() {
	mainCPU.carryResult = mainCPU.A >> 7;
	mainCPU.A = (mainCPU.A << 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void ASL_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	mainCPU.carryResult = data >> 7;
	data = (data << 1) & 0xFF;
	mainCPU.zeroResult = data;
	mainCPU.negativeResult = data;

	writeAddr(address, data);
}

FORCE_INLINE void ASL_ZERO(unsigned int address) {
	unsigned int data = CPU_RAM(address);

	mainCPU.carryResult = data >> 7;
	int result = (data << 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;

	writeZero(address, result);
}

FORCE_INLINE void LSR() {
	mainCPU.carryResult = (mainCPU.A & 0x01);
	mainCPU.A >>= 1;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = 0;
}

FORCE_INLINE void LSR_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	mainCPU.carryResult = (data & 0x01);
	data >>= 1;
	mainCPU.zeroResult = data;
	mainCPU.negativeResult = 0;

	writeAddr(address, data);
}

FORCE_INLINE void LSR_ZERO(unsigned int address) {
	unsigned int data = CPU_RAM(address);

	mainCPU.carryResult = (data & 0x01);
	data >>= 1;
	mainCPU.zeroResult = data;
	mainCPU.negativeResult = 0;

	writeZero(address, data);
}

FORCE_INLINE void ROL() {
	mainCPU.A = (mainCPU.A << 1) | mainCPU.carryResult;
	mainCPU.carryResult = mainCPU.A >> 8;
	mainCPU.zeroResult = mainCPU.A & 0xFF;
	mainCPU.A = mainCPU.zeroResult;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void ROL_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	data = (data << 1) | mainCPU.carryResult;
	mainCPU.carryResult = data >> 8;
	mainCPU.zeroResult = data & 0xFF;
	mainCPU.negativeResult = data;

	writeAddr(address, data);
}

FORCE_INLINE void ROL_ZERO(unsigned int address) {
	unsigned int data = CPU_RAM(address);

	data = (data << 1) | mainCPU.carryResult;
	mainCPU.carryResult = data >> 8;
	mainCPU.zeroResult = data & 0xFF;
	mainCPU.negativeResult = data;

	writeZero(address, data);
}


FORCE_INLINE void ROR() {
	unsigned int result = (mainCPU.A >> 1) | (mainCPU.carryResult << 7);
	mainCPU.carryResult = mainCPU.A & 0x01;

	mainCPU.A = result;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

FORCE_INLINE void ROR_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	unsigned int result = (data >> 1) | (mainCPU.carryResult << 7);
	mainCPU.carryResult = data & 0x01;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;

	writeAddr(address, result);
}

FORCE_INLINE void ROR_ZERO(unsigned int address) {
	int data = CPU_RAM(address);

	unsigned int result = (data >> 1) | (mainCPU.carryResult << 7);
	mainCPU.carryResult = data & 0x01;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;

	writeZero(address, result);
}

FORCE_INLINE void BIT(unsigned int data) {
	mainCPU.P =
		(mainCPU.P & (ST_INT | ST_BCD | ST_BRK | ST_CRY | ST_UNUSED)) |		// keep flags
		(data & (ST_OVR | ST_NEG));											// these are copied in (bits 6-7)
	mainCPU.zeroResult = (data & mainCPU.A);
	mainCPU.negativeResult = data;
}

FORCE_INLINE void BIT_MEM(unsigned int address) {
	BIT(mainCPU.read(address));
}

FORCE_INLINE void BIT_ZERO(unsigned int address) {
	BIT(CPU_RAM(address));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STATUS FLAGS

FORCE_INLINE void CLC() {
	// (clear carry)
	mainCPU.carryResult = 0;
}

FORCE_INLINE void SEC() {
	// (set carry)
	mainCPU.carryResult = 1;
}

FORCE_INLINE void CLI() {
	// (clear interrupt)
	// TODO : Delayed IRQ response behavior?
	mainCPU.P &= ~ST_INT;
}

FORCE_INLINE void SEI() {
	// (set interrupt)
	mainCPU.P |= ST_INT;
}

FORCE_INLINE void CLV() {
	// (clear overflow)
	mainCPU.P &= ~ST_OVR;
}

FORCE_INLINE void CLD() {
	// (clear decimal)
	mainCPU.P &= ~ST_BCD;
}

FORCE_INLINE void SED() {
	// (set decimal)
	mainCPU.P |= ST_BCD;
}

#if TRACE_DEBUG
static unsigned int effAddr = -1;
static unsigned int effByte = 0;
unsigned int eff_address(unsigned int addr) { effAddr = addr; if (effAddr < 0x2000 || effAddr >= 0x6000) effByte = mainCPU.readNonIO(effAddr); return effAddr; }
#else
#define eff_address(X) (X)
#endif

FORCE_INLINE void cpu6502_PerformInstruction() {
#if TRACE_DEBUG
	cpu_instr_history hist;
	resolveToP();
	memcpy(&hist.regs, &mainCPU, sizeof(cpu_6502));
#else
#endif

	// TODO : cache into single read (with instruction overlap in bank pages)
	unsigned int instr, data1, data2;
	if ((mainCPU.PC & 0xFF) < 0xFD) {
		unsigned char* ptr = mainCPU.getNonIOMem(mainCPU.PC);
		instr = ptr[0];
		data1 = ptr[1];
		data2 = ptr[2];
	} else {
		instr = mainCPU.readNonIO(mainCPU.PC);
		data1 = mainCPU.readNonIO(mainCPU.PC + 1);
		data2 = mainCPU.readNonIO(mainCPU.PC + 2);
	}

	// all instructions at least 2 clks
	mainCPU.clocks += 2;

	// most are at 2 bytes (this will be incremented/decremented on the rest)
	mainCPU.PC += 2;

#define SKIP_LATCHING() goto SkipLatching;
#define OPCODE_START(op,clk,sz) case op: { INSTR_TIMING(op); mainCPU.clocks += (clk-2); mainCPU.PC += (sz-2);
#define OPCODE_END(spc) spc break; }

#define OPCODE_NON(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name(); \
		SKIP_LATCHING(); \
	OPCODE_END(spc) 

#define OPCODE_IMM(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name(data1); \
		SKIP_LATCHING(); \
	OPCODE_END(spc) 

#define OPCODE_REL(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name(data1); \
		SKIP_LATCHING(); \
	OPCODE_END(spc) 

#define OPCODE_ABS(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name##_MEM(eff_address(data1 + (data2 << 8))); \
	OPCODE_END(spc) 

#define OPCODE_ABX(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		if (page && ((data1 + mainCPU.X) & 0x100)) mainCPU.clocks++; \
		name##_MEM((data1 + (data2 << 8) + (mainCPU.X))); \
	OPCODE_END(spc) 

#define OPCODE_ABY(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		if (page && ((data1 + mainCPU.Y) & 0x100)) mainCPU.clocks++;	\
		name##_MEM((data1 + (data2 << 8) + (mainCPU.Y))); \
	OPCODE_END(spc) 

#define OPCODE_IND(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		unsigned int target = (data2 << 8); \
		name##_MEM(eff_address(mainCPU.read(data1 + target) + (mainCPU.read(((data1 + 1) & 0xFF) + target) << 8))); \
	OPCODE_END(spc)

#define OPCODE_INX(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		int target = (data1 + mainCPU.X) & 0xFF; \
		name##_MEM(eff_address(CPU_RAM(target) + (CPU_RAM((target + 1) & 0xFF) << 8))); \
	OPCODE_END(spc)

#define OPCODE_INY(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		if (page && ((CPU_RAM(data1) + mainCPU.Y) & 0x100)) mainCPU.clocks++; \
		name##_MEM(eff_address(CPU_RAM(data1) + (CPU_RAM((data1 + 1) & 0xFF) << 8) + mainCPU.Y)); \
	OPCODE_END(spc) 

#define OPCODE_ZRO(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name##_ZERO(eff_address(data1)); \
		SKIP_LATCHING(); \
	OPCODE_END(spc)

#define OPCODE_ZRX(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name##_ZERO(eff_address((data1 + mainCPU.X) & 0xFF)); \
		SKIP_LATCHING(); \
	OPCODE_END(spc) 

#define OPCODE_ZRY(op,str,clk,sz,page,name,spc) \
	OPCODE_START(op,clk,sz) \
		name##_ZERO(eff_address((data1 + mainCPU.Y) & 0xFF)); \
		SKIP_LATCHING(); \
	OPCODE_END(spc)

	switch (instr) {
		#include "6502_opcodes.inl"
		default:
		{
#if TRACE_DEBUG
			IllegalInstruction();
#else
			DebugAssert(false);
#endif
		}
	};

	if (mainCPU.accessTable[0x2000 >> 13]) {
		ppu_registers.latchedReg(mainCPU.accessTable[0x2000 >> 13]);
		mainCPU.accessTable[0x2000 >> 13] = 0;
	} else if (mainCPU.accessTable[0x4000 >> 13]) {
		mainCPU.latchedSpecial(mainCPU.accessTable[0x4000 >> 13]);
		mainCPU.accessTable[0x4000 >> 13] = 0;
	}

SkipLatching:

	// sanity checks
	DebugAssert(mainCPU.carryResult == 0 || mainCPU.carryResult == 1);
#if TRACE_DEBUG
	if (instr == 0x60) {
		// RTS special case:
		effAddr = (mainCPU.readNonIO(mainCPU.PC - 1) << 8) + mainCPU.readNonIO(mainCPU.PC - 2);
	}

	hist.instr = instr;
	hist.data1 = data1;
	hist.data2 = data2;
	hist.effAddr = effAddr;
	hist.effByte = effByte;

	traceHistory[traceNum++] = hist;
	if (traceNum == NUM_TRACED) traceNum = 0;

	if (traceLineRemaining) {
		hist.output();
		traceLineRemaining--;
	}

	if (hist.regs.PC == cpuBreakpoint) {
		HitBreakpoint();
	}

	if (bHitMemBreakpoint) {
		HitBreakpoint();
		bHitMemBreakpoint = false;
	}

	if (bHitPPUBreakpoint) {
		Do_PPUBreakpoint();
		bHitPPUBreakpoint = false;
	}


	cpuInstructionCount++;
	if (instructionCountBreakpoint && cpuInstructionCount == instructionCountBreakpoint) {
		HitBreakpoint();
	}
#endif
}

void cpu6502_Step() {
	TIME_SCOPE();

	// stop at next PPU or IRQ
	mainCPU.nextClocks = mainCPU.ppuClocks;
	if (mainCPU.irqClocks && mainCPU.irqClocks < mainCPU.ppuClocks) {
		mainCPU.nextClocks = mainCPU.irqClocks;
	}
	if (mainCPU.ppuNMI) {
		mainCPU.nextClocks = mainCPU.clocks + 7;
	}

	for (; mainCPU.clocks < mainCPU.nextClocks;) {
		cpu6502_PerformInstruction();
	}

	if (mainCPU.ppuNMI) {
		mainCPU.NMI();
		mainCPU.ppuNMI = false;
	}
}

void cpu6502_DeviceInterrupt(unsigned int vectorAddress, bool masked) {
	if (!masked || (mainCPU.P & ST_INT) == 0) {
		// interrupts are enabled

		// push PC and P (without BRK) onto stack
		resolveToP();
		mainCPU.push(mainCPU.PC >> 8);
		mainCPU.push(mainCPU.PC & 0xFF);
		mainCPU.push((mainCPU.P & ~ST_BRK) | ST_UNUSED);

		// switch PC to vector address and disable interrupts 
		mainCPU.PC = mainCPU.read(vectorAddress) | (mainCPU.read(vectorAddress+1) << 8);
		mainCPU.P |= ST_INT;

		// whole process takes 7 clocks
		mainCPU.clocks += 7;
	}
}

void cpu6502_SoftwareInterrupt(unsigned int vectorAddress) {
	// push PC and P (with BRK) onto stack
	resolveToP();
	mainCPU.push(mainCPU.PC >> 8);
	mainCPU.push(mainCPU.PC & 0xFF);
	mainCPU.push(mainCPU.P | ST_BRK | ST_UNUSED);

	// switch PC to vector address and disable interrupts 
	mainCPU.PC = mainCPU.read(vectorAddress) | (mainCPU.read(vectorAddress+1) << 8);
	mainCPU.P |= ST_INT;

	// whole process takes 7 clocks
	mainCPU.clocks += 7;
}

void cpu_instr_history::output() {
	// output is set up to match fceux for easy comparison

#if TARGET_WINSIM && DEBUG
	char output[2048];
	output[0] = 0;
	#define ADD_LOG(...) { char buffer[512]; sprintf_s(buffer, 512, __VA_ARGS__); strcat(output, buffer); }

	static bool showClocks = false;
	if (showClocks) {
		ADD_LOG("c%-11d ", regs.clocks);
	}
	static bool showRegs = true;
	if (showRegs) {
		ADD_LOG("A:%02X X:%02X Y:%02X S:%02X P:%s%s%s%s%s%s%s%s ",
			regs.A,
			regs.X,
			regs.Y,
			regs.SP,
			regs.P & ST_NEG ? "N" : "n",
			regs.P & ST_OVR ? "V" : "v",
			regs.P & ST_UNUSED ? "U" : "u",
			regs.P & ST_BRK ? "B" : "b",
			regs.P & ST_BCD ? "D" : "d",
			regs.P & ST_INT ? "I" : "i",
			regs.P & ST_ZRO ? "Z" : "z",
			regs.P & ST_CRY ? "C" : "c");
	}

	static bool showStackDepth = false;
	if (showStackDepth) {
		for (int i = 0xFF; i > regs.SP; i--) {
			ADD_LOG(" ");
		}
	}

#define OPCODE_0W(op,str,clk,sz,page,name,spc) if (instr == op) ADD_LOG("$%04X:%02X        " str, regs.PC, instr);
#define OPCODE_1W(op,str,clk,sz,page,name,spc) if (instr == op) ADD_LOG("$%04X:%02X %02X     " str, regs.PC, instr, data1, data1);
#define OPCODE_2W(op,str,clk,sz,page,name,spc) if (instr == op) ADD_LOG("$%04X:%02X %02X %02X  " str, regs.PC, instr, data1, data2, data1 + (data2 << 8));
#define OPCODE_REL(op,str,clk,sz,page,name,spc) if (instr == op) ADD_LOG("$%04X:%02X %02X     " str, regs.PC, instr, data1, regs.PC+2+((signed char)data1));
#include "6502_opcodes.inl"

	unsigned int addressMode = modeTable[instr & 0x1F];
	if (addressMode == AM_IndirectX || addressMode == AM_IndirectY || addressMode == AM_AbsoluteX || addressMode == AM_AbsoluteY || addressMode == AM_ZeroX) {
		ADD_LOG(" @ $%04X", effAddr);
	}
	if (addressMode != AM_None && instr != 0x4C) {
		ADD_LOG(" = #$%02X", effByte);
	}
	if (instr == 0x60) {
		// RTS special case
		ADD_LOG(" (from $%04X) ---------------------------", effAddr)
	}
	ADD_LOG("\n");
	OutputLog(output);
#endif
}

#if TRACE_DEBUG
void HitBreakpoint() {
	OutputLog("CPU Instruction Trace:\n");
	for (int i = NUM_TRACED - 1; i > 0; i--) {
		int curLine = (traceNum - i + NUM_TRACED) % NUM_TRACED;
		if (!traceHistory[curLine].isEmpty()) {
			traceHistory[curLine].output();
		}
	}
	OutputLog("Hit breakpoint at %04x!\n", cpuBreakpoint);

	DebugBreak();
}

void IllegalInstruction() {
	OutputLog("CPU Instruction Trace:\n");
	for (int i = NUM_TRACED - 1; i > 0; i--) {
		traceHistory[(traceNum - i + NUM_TRACED) % NUM_TRACED].output();
	}
	OutputLog("Encountered illegal instruction at %04x (0x%02x)!\n", mainCPU.PC, mainCPU.read(mainCPU.PC));

	DebugBreak();
}

void PPUBreakpoint() {
	bHitPPUBreakpoint = true;
}

void Do_PPUBreakpoint() {
	OutputLog("CPU Instruction Trace:\n");
	for (int i = NUM_TRACED - 1; i > 0; i--) {
		traceHistory[(traceNum - i + NUM_TRACED) % NUM_TRACED].output();
	}
	OutputLog("PPU Breakpoint!");

	DebugBreak();
}
#else
void PPUBreakpoint() {

} 
#endif

