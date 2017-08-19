
// main 6502 interpreter and emulation
#include "platform.h"
#include "debug.h"

#define NES 1
#include "6502.h"

#if TARGET_WINSIM
#define TRACE_DEBUG 1
#else
#define TRACE_DEBUG 0
#endif

#if TRACE_DEBUG
static unsigned int cpuBreakpoint = 0x0000;
static unsigned int memWriteBreakpoint = 0xfffff;

#define NUM_TRACED 300
static cpu_instr_history traceHistory[NUM_TRACED] = { 0 };
static unsigned int traceNum;
void HitBreakpoint();
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
}

void resolveToP() {
	mainCPU.P = (mainCPU.P & (~ST_ZRO & ~ST_NEG & ~ST_CRY)) |
		((mainCPU.zeroResult == 0) ? ST_ZRO : 0) |
		(mainCPU.carryResult ? ST_CRY : 0) |
		(mainCPU.negativeResult & ST_NEG);
}

void resolveFromP() {
	mainCPU.zeroResult = (~mainCPU.P & ST_ZRO);
	mainCPU.negativeResult = (mainCPU.P & ST_NEG);
	mainCPU.carryResult = (mainCPU.P & ST_CRY);
}

inline void writeAddr(unsigned int addr, unsigned int result) {
	mainCPU.write(addr, result);

#if TRACE_DEBUG
	if (addr == memWriteBreakpoint) {
		HitBreakpoint();
	}
#endif
}

inline void latchWriteAddr(unsigned int addr, unsigned int result) {
	mainCPU.read(addr);
	writeAddr(addr, result);
}

inline void writeZero(unsigned int addr, unsigned int result) {
	mainCPU.RAM[addr] = result;

#if TRACE_DEBUG
	if (addr == memWriteBreakpoint) {
		HitBreakpoint();
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STORE / LOAD

// LDA #$NN (load A immediate)
inline void LDA(unsigned int data) {
	mainCPU.A = data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void LDA_MEM(unsigned int address) {
	LDA(mainCPU.read(address));
}

inline void LDA_ZERO(unsigned int address) {
	LDA(mainCPU.RAM[address]);
}

inline void LDX(unsigned int data) {
	mainCPU.X = data;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

inline void LDX_MEM(unsigned int address) {
	LDX(mainCPU.read(address));
}

inline void LDX_ZERO(unsigned int address) {
	LDX(mainCPU.RAM[address]);
}

inline void LDY(unsigned int data) {
	mainCPU.Y = data;
	mainCPU.zeroResult = mainCPU.Y;
	mainCPU.negativeResult = mainCPU.Y;
}

inline void LDY_MEM(unsigned int address) {
	LDY(mainCPU.read(address));
}

inline void LDY_ZERO(unsigned int address) {
	LDY(mainCPU.RAM[address]);
}

inline void STA_MEM(unsigned int address) {
	latchWriteAddr(address, mainCPU.A);
}

inline void STA_ZERO(unsigned int address) {
	writeZero(address, mainCPU.A);
}

inline void STX_MEM(unsigned int address) {
	latchWriteAddr(address, mainCPU.X);
}

inline void STX_ZERO(unsigned int address) {
	writeZero(address, mainCPU.X);
}

inline void STY_MEM(unsigned int address) {
	latchWriteAddr(address, mainCPU.Y);
}

inline void STY_ZERO(unsigned int address) {
	writeZero(address, mainCPU.Y);
}

inline void TAX() {
	mainCPU.X = mainCPU.A;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void TXA() {
	mainCPU.A = mainCPU.X;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void TAY() {
	mainCPU.Y = mainCPU.A;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void TYA() {
	mainCPU.A = mainCPU.Y;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void TSX() {
	mainCPU.X = mainCPU.SP;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

inline void TXS() {
	mainCPU.SP = mainCPU.X;
}

inline void PHA() {
	mainCPU.push(mainCPU.A);
}

inline void PLA() {
	mainCPU.A = mainCPU.pop();
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void PHP() {
	resolveToP();
	mainCPU.push(mainCPU.P | ST_UNUSED | ST_BRK);
}

// PLP (pop processor status ignoring bit 4)
inline void PLP() {
	mainCPU.P = mainCPU.pop() & ~(ST_BRK);
	resolveFromP();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BRANCH / JUMP

inline void takeBranch(unsigned int data) {
	unsigned int oldPC = mainCPU.PC;
	mainCPU.PC += (char) (data);
	mainCPU.clocks += 1 + ((oldPC & 0xFF00) != (mainCPU.PC & 0xFF00) ? 1 : 0);
}

inline void BPL(unsigned int data) {
	if ((~mainCPU.negativeResult) & ST_NEG) {
		takeBranch(data);
	}
}

inline void BMI(unsigned int data) {
	if (mainCPU.negativeResult & ST_NEG) {
		takeBranch(data);
	}
}

inline void BVC(unsigned int data) {
	if ((~mainCPU.P) & ST_OVR) {
		takeBranch(data);
	}
}

inline void BVS(unsigned int data) {
	if (mainCPU.P & ST_OVR) {
		takeBranch(data);
	}
}

inline void BCC(unsigned int data) {
	if (mainCPU.carryResult == 0) {
		takeBranch(data);
	}
}

inline void BCS(unsigned int data) {
	if (mainCPU.carryResult) {
		takeBranch(data);
	}
}

inline void BNE(unsigned int data) {
	if (mainCPU.zeroResult) {
		takeBranch(data);
	}
}

inline void BEQ(unsigned int data) {
	if (!mainCPU.zeroResult) {
		takeBranch(data);
	}
}

inline void JMP_MEM(unsigned int addr) {
	// common infinite loop
	if (mainCPU.PC == addr + 3) {
		mainCPU.clocks = mainCPU.ppuClocks;
	}

	mainCPU.PC = addr;
}

inline void JSR_MEM(unsigned int addr) {
	// JSR (subroutine)
	mainCPU.push(mainCPU.PC >> 8);
	mainCPU.push(mainCPU.PC & 0xFF);

	mainCPU.PC = addr;
}

inline void RTI() {
	// RTI (return from interrupt)
	// TODO : Non- delayed IRQ response behavior?
	mainCPU.P |= mainCPU.pop() & ~(ST_BRK);
	mainCPU.PC = mainCPU.pop() | (mainCPU.pop() << 8);
	resolveFromP();
}
			
inline void RTS() {
	// RTS
	mainCPU.PC = mainCPU.pop() | (mainCPU.pop() << 8);
	mainCPU.PC++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ALU

inline void ADC(unsigned int data) {
	unsigned int result = mainCPU.A + data + (mainCPU.carryResult ? 1 : 0);
	mainCPU.P =
		(mainCPU.P & (ST_INT | ST_BRK | ST_BCD | ST_UNUSED)) |					// keep flags
		(((mainCPU.A^result)&(data^result) & 0x80) >> (7 - ST_OVR_BIT));		// overflow (http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html)
	mainCPU.A = result & 0xFF;
	mainCPU.carryResult = result & 0x100;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void ADC_MEM(unsigned int address) {
	ADC(mainCPU.read(address));
}

inline void ADC_ZERO(unsigned int address) {
	ADC(mainCPU.RAM[address]);
}

inline void SBC(unsigned int data) {
	// TODO : possibly move overflow calculation to flag resolve?
	unsigned int result = mainCPU.A - data - (mainCPU.carryResult ? 0 : 1);
	mainCPU.P =
		(mainCPU.P & (ST_INT | ST_BRK | ST_BCD | ST_UNUSED)) |					// keep flags
		(((mainCPU.A^result)&((~(data)) ^ result) & 0x80) >> (7 - ST_OVR_BIT));	// overflow (http://www.righto.com/2012/12/the-6502-overflow-flag-explained.html)
	mainCPU.A = result & 0xFF;
	mainCPU.carryResult = ~result & 0x100;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void SBC_MEM(unsigned int address) {
	SBC(mainCPU.read(address));
}

inline void SBC_ZERO(unsigned int address) {
	SBC(mainCPU.RAM[address]);
}

inline void DEC_MEM(unsigned int address) {
	unsigned int result = (mainCPU.read(address) - 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeAddr(address, result);
}

inline void DEC_ZERO(unsigned int address) {
	unsigned int result = (mainCPU.RAM[address] - 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeZero(address, result);
}

inline void INC_MEM(unsigned int address) {
	unsigned int result = (mainCPU.read(address) + 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeAddr(address, result);
}

inline void INC_ZERO(unsigned int address) {
	unsigned int result = (mainCPU.RAM[address] + 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;
	writeZero(address, result);
}

inline void DEX() {
	// DEX (decrement X)
	mainCPU.X = (mainCPU.X - 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

inline void INX() {
	// INX (increment X)
	mainCPU.X = (mainCPU.X + 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.X;
	mainCPU.negativeResult = mainCPU.X;
}

inline void DEY() {
	// DEY (decrement Y)
	mainCPU.Y = (mainCPU.Y - 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.Y;
	mainCPU.negativeResult = mainCPU.Y;
}

inline void INY() {
	// INY (increment Y)
	mainCPU.Y = (mainCPU.Y + 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.Y;
	mainCPU.negativeResult = mainCPU.Y;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COMPARE / TEST

inline void CMP(unsigned int data) {
	mainCPU.carryResult = (data <= mainCPU.A);
	mainCPU.zeroResult = (mainCPU.A - data);
	mainCPU.negativeResult = mainCPU.zeroResult;
}

inline void CMP_MEM(unsigned int addr) {
	CMP(mainCPU.read(addr));
}

inline void CMP_ZERO(unsigned int addr) {
	CMP(mainCPU.RAM[addr]);
}

inline void CPX(unsigned int data) {
	mainCPU.carryResult = (data <= mainCPU.X);
	mainCPU.zeroResult = (mainCPU.X - data);
	mainCPU.negativeResult = mainCPU.zeroResult;
}

inline void CPX_MEM(unsigned int addr) {
	CPX(mainCPU.read(addr));
}

inline void CPX_ZERO(unsigned int addr) {
	CPX(mainCPU.RAM[addr]);
}

inline void CPY(unsigned int data) {
	mainCPU.carryResult = (data <= mainCPU.Y);
	mainCPU.zeroResult = (mainCPU.Y - data);
	mainCPU.negativeResult = mainCPU.zeroResult;
}

inline void CPY_MEM(unsigned int addr) {
	CPY(mainCPU.read(addr));
}

inline void CPY_ZERO(unsigned int addr) {
	CPY(mainCPU.RAM[addr]);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MISC

inline void BRK() {
	// BRK moves forward an instruction
	cpu6502_SoftwareInterrupt(0xFFFE);
}

inline void NOP() {
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BITWISE

inline void ORA(unsigned int data) {
	mainCPU.A = mainCPU.A | data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void ORA_MEM(unsigned int address) {
	ORA(mainCPU.read(address));
}

inline void ORA_ZERO(unsigned int address) {
	ORA(mainCPU.RAM[address]);
}

inline void AND(unsigned int data) {
	mainCPU.A = mainCPU.A & data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void AND_MEM(unsigned int address) {
	AND(mainCPU.read(address));
}

inline void AND_ZERO(unsigned int address) {
	AND(mainCPU.RAM[address]);
}

inline void EOR(unsigned int data) {
	mainCPU.A = mainCPU.A ^ data;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void EOR_MEM(unsigned int address) {
	EOR(mainCPU.read(address));
}

inline void EOR_ZERO(unsigned int address) {
	EOR(mainCPU.RAM[address]);
}

inline void ASL() {
	mainCPU.carryResult = (mainCPU.A & 0x80);
	mainCPU.A = (mainCPU.A << 1) & 0xFF;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void ASL_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	mainCPU.carryResult = (data & 0x80);
	data = (data << 1) & 0xFF;
	mainCPU.zeroResult = data;
	mainCPU.negativeResult = data;

	writeAddr(address, data);
}

inline void ASL_ZERO(unsigned int address) {
	unsigned int data = mainCPU.RAM[address];

	mainCPU.carryResult = (data & 0x80);
	int result = (data << 1) & 0xFF;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;

	writeZero(address, result);
}

inline void LSR() {
	mainCPU.carryResult = (mainCPU.A & 0x01);
	mainCPU.A >>= 1;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = 0;
}

inline void LSR_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	mainCPU.carryResult = (data & 0x01);
	data >>= 1;
	mainCPU.zeroResult = data;
	mainCPU.negativeResult = 0;

	writeAddr(address, data);
}

inline void LSR_ZERO(unsigned int address) {
	unsigned int data = mainCPU.RAM[address];

	mainCPU.carryResult = (data & 0x01);
	data >>= 1;
	mainCPU.zeroResult = data;
	mainCPU.negativeResult = 0;

	writeZero(address, data);
}

inline void ROL() {
	mainCPU.A = (mainCPU.A << 1) | (mainCPU.carryResult ? 0x01 : 0);
	mainCPU.carryResult = mainCPU.A & 0x100;
	mainCPU.zeroResult = mainCPU.A & 0xFF;
	mainCPU.A = mainCPU.zeroResult;
	mainCPU.negativeResult = mainCPU.A;
}

inline void ROL_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	data = (data << 1) | (mainCPU.carryResult ? 0x01 : 0);
	mainCPU.carryResult = data & 0x100;
	mainCPU.zeroResult = data & 0xFF;
	mainCPU.negativeResult = data;

	writeAddr(address, data);
}

inline void ROL_ZERO(unsigned int address) {
	unsigned int data = mainCPU.RAM[address];

	data = (data << 1) | (mainCPU.carryResult ? 0x01 : 0);
	mainCPU.carryResult = data & 0x100;
	mainCPU.zeroResult = data & 0xFF;
	mainCPU.negativeResult = data;

	writeZero(address, data);
}


inline void ROR() {
	unsigned int result = (mainCPU.A >> 1) | (mainCPU.carryResult ? 0x80 : 0);
	mainCPU.carryResult = mainCPU.A & 0x01;

	mainCPU.A = result;
	mainCPU.zeroResult = mainCPU.A;
	mainCPU.negativeResult = mainCPU.A;
}

inline void ROR_MEM(unsigned int address) {
	unsigned int data = mainCPU.read(address);

	unsigned int result = (data >> 1) | (mainCPU.carryResult ? 0x80 : 0);
	mainCPU.carryResult = data & 0x01;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;

	writeAddr(address, result);
}

inline void ROR_ZERO(unsigned int address) {
	int data = mainCPU.RAM[address];

	unsigned int result = (data >> 1) | (mainCPU.carryResult ? 0x80 : 0);
	mainCPU.carryResult = data & 0x01;
	mainCPU.zeroResult = result;
	mainCPU.negativeResult = result;

	writeZero(address, result);
}

inline void BIT(unsigned int data) {
	mainCPU.P =
		(mainCPU.P & (ST_INT | ST_BCD | ST_BRK | ST_CRY | ST_UNUSED)) |		// keep flags
		(data & (ST_OVR));													// these are copied in (bits 6-7)
	mainCPU.zeroResult = (data & mainCPU.A);
	mainCPU.negativeResult = data;
}

inline void BIT_MEM(unsigned int address) {
	BIT(mainCPU.read(address));
}

inline void BIT_ZERO(unsigned int address) {
	BIT(mainCPU.RAM[address]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STATUS FLAGS

inline void CLC() {
	// (clear carry)
	mainCPU.carryResult = 0;
}

inline void SEC() {
	// (set carry)
	mainCPU.carryResult = 1;
}

inline void CLI() {
	// (clear interrupt)
	// TODO : Delayed IRQ response behavior?
	mainCPU.P &= ~ST_INT;
}

inline void SEI() {
	// (set interrupt)
	mainCPU.P |= ST_INT;
}

inline void CLV() {
	// (clear overflow)
	mainCPU.P &= ~ST_OVR;
}

inline void CLD() {
	// (clear decimal)
	mainCPU.P &= ~ST_BCD;
}

inline void SED() {
	// (set decimal)
	mainCPU.P |= ST_BCD;
}

inline void cpu6502_PerformInstruction() {
#if TRACE_DEBUG
	cpu_instr_history hist;
	resolveToP();
	memcpy(&hist.regs, &mainCPU, sizeof(cpu_6502));
	int effAddr = -1;
#define EFF_ADDR
#else
#define EFF_ADDR int effAddr; 
#endif

	// TODO : cache into single read (with instruction overlap in bank pages)
	unsigned int instr, data1, data2;
	if (mainCPU.PC & 0xFF < 0xFD) {
		unsigned char* ptr = mainCPU.getNonIOMem(mainCPU.PC);
		instr = ptr[0];
		data1 = ptr[1];
		data2 = ptr[2];
	} else {
		instr = mainCPU.readNonIO(mainCPU.PC);
		data1 = mainCPU.readNonIO(mainCPU.PC + 1);
		data2 = mainCPU.readNonIO(mainCPU.PC + 2);
	}

	switch (instr) {
#define OPCODE_START(op,clk,sz) \
	case op: \
		{ EFF_ADDR \
		  mainCPU.clocks += clk; \
		  mainCPU.PC += sz;

#define OPCODE_END(spc) \
		  spc \
		  break; }

#define OPCODE_NON(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) name(); OPCODE_END(spc) 
#define OPCODE_IMM(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) name(data1); OPCODE_END(spc) 
#define OPCODE_REL(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) name(data1); OPCODE_END(spc) 
#define OPCODE_ABS(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = data1 + (data2 << 8);					name##_MEM(effAddr); OPCODE_END(spc) 
#define OPCODE_ABX(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = data1 + (data2 << 8) + (mainCPU.X); if (page && ((data1 + mainCPU.X) & 0x100)) mainCPU.clocks++;	name##_MEM(effAddr); OPCODE_END(spc) 
#define OPCODE_ABY(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = data1 + (data2 << 8) + (mainCPU.Y); if (page && ((data1 + mainCPU.Y) & 0x100)) mainCPU.clocks++;	name##_MEM(effAddr); OPCODE_END(spc) 
#define OPCODE_IND(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) int target = (data2 << 8); effAddr = mainCPU.read(data1 + target) + (mainCPU.read(((data1 + 1) & 0xFF) + target) << 8);			name##_MEM(effAddr); OPCODE_END(spc)
#define OPCODE_INX(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) int target = (data1 + mainCPU.X) & 0xFF; effAddr = mainCPU.RAM[target] + (mainCPU.RAM[(target + 1) & 0xFF] << 8);				name##_MEM(effAddr); OPCODE_END(spc)
#define OPCODE_INY(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = mainCPU.RAM[data1] + (mainCPU.RAM[(data1 + 1) & 0xFF] << 8) + mainCPU.Y; if (page && ((mainCPU.RAM[data1] + mainCPU.Y) & 0x100)) mainCPU.clocks++;	name##_MEM(effAddr); OPCODE_END(spc) 
#define OPCODE_ZRO(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = data1;							name##_ZERO(data1); OPCODE_END(spc) 
#define OPCODE_ZRX(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = (data1 + mainCPU.X) & 0xFF;	name##_ZERO(effAddr); OPCODE_END(spc) 
#define OPCODE_ZRY(op,str,clk,sz,page,name,spc) OPCODE_START(op,clk,sz) effAddr = (data1 + mainCPU.Y) & 0xFF;	name##_ZERO(effAddr); OPCODE_END(spc) 

		#include "6502_opcodes.inl"
		default: DebugAssert(false); 
	};

#if TRACE_DEBUG
	hist.instr = instr;
	hist.data1 = data1;
	hist.data2 = data2;
	hist.effAddr = effAddr;
	if (effAddr != -1 && (effAddr < 0x2000 || effAddr >= 0x6000)) {
		hist.effByte = mainCPU.readNonIO(effAddr);
	}

	traceHistory[traceNum++] = hist;
	if (traceNum == NUM_TRACED) traceNum = 0;

	if (traceLineRemaining) {
		hist.output();
		traceLineRemaining--;
	}

	if (hist.regs.PC == cpuBreakpoint) {
		HitBreakpoint();
	}
#endif
}

void cpu6502_Step() {
	TIME_SCOPE();

	// stop at next PPU or IRQ
	int nextClocks = mainCPU.ppuClocks;
	if (mainCPU.irqClocks && mainCPU.irqClocks < mainCPU.ppuClocks) {
		nextClocks = mainCPU.irqClocks;
	}

	for (; mainCPU.clocks < nextClocks;) {
		cpu6502_PerformInstruction();
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

#if DEBUG
	static bool showClocks = false;
	if (showClocks) {
		OutputLog("c%-11d ", regs.clocks);
	}
	static bool showRegs = true;
	if (showRegs) {
		OutputLog("A:%02X X:%02X Y:%02X S:%02X P:%s%s%s%s%s%s%s%s  ",
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

#define OPCODE_0W(op,str,clk,sz,page,name,spc) if (instr == op) OutputLog("$%04X:%02X        " str, regs.PC, instr);
#define OPCODE_1W(op,str,clk,sz,page,name,spc) if (instr == op) OutputLog("$%04X:%02X %02X     " str, regs.PC, instr, data1, data1);
#define OPCODE_2W(op,str,clk,sz,page,name,spc) if (instr == op) OutputLog("$%04X:%02X %02X %02X  " str, regs.PC, instr, data1, data2, data1 + (data2 << 8));
#define OPCODE_REL(op,str,clk,sz,page,name,spc) if (instr == op) OutputLog("$%04X:%02X %02X     " str, regs.PC, instr, data1, regs.PC+2+((signed char)data1));
#include "6502_opcodes.inl"
#endif

	unsigned int addressMode = modeTable[instr & 0x1F];
	if (addressMode == AM_IndirectX || addressMode == AM_IndirectY) {
		OutputLog(" @ $%04X", effAddr);
	}
	if (addressMode != AM_None && instr != 0x4C) {
		OutputLog(" = #$%02X", effByte);
	}
	OutputLog("\n");
}

#if TRACE_DEBUG
void HitBreakpoint() {
	OutputLog("CPU Instruction Trace:\n");
	for (int i = NUM_TRACED - 1; i > 0; i--) {
		traceHistory[(traceNum - i + NUM_TRACED) % NUM_TRACED].output();
	}
	OutputLog("Hit breakpoint at %04x!\n", cpuBreakpoint);

	DebugBreak();
}

void PPUBreakpoint() {
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

