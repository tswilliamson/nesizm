#pragma once

// 6502 chip emulation specific header (with NES specialization in sub-struct to allow for add'l extension later if we want it)

// address modes internally related only to *data* outside of the instruction, so AM_None encompasses any instruction with no outside instruction access
enum ADDR_MODE {
	AM_None,		// Implied, immediate, relative jump, or absolute jump/branch
	AM_Absolute,	// Absolute address $addr
	AM_AbsoluteX,	// Absolute address + X + carry $addr+X+C
	AM_IndirectX,	// Indirect zeropage address + X: ($00## + X) with no page translation
	AM_IndirectY,	// Indirect zeropage address offset by Y: ($00##) + Y with no page translation
	AM_Zero,		// zero page
	AM_ZeroX,		// zero page + X with no page translation

	// there are not actually mapped to the mode table and are exceptions for a few instructions, but are here for completeness
	AM_Indirect,	// Indirect address ($addr)
	AM_AbsoluteY,	// Absolute address + Y + carry $addr+Y+C 
	AM_ZeroY,		// zero page + Y with no page translation 
};

struct cpu_6502 {
	// main registers (stored as ints due to improved 32-bit speed, but only use the relevant bits)
	unsigned int PC;	// program counter, 16-bit
	unsigned int SP;	// stack pointer (offset from 0x100), 8-bit
	unsigned int A;		// accumulator and index registers, 8-bit
	unsigned int X;
	unsigned int Y;
	unsigned int P;		// processor status flags, 8-bit

	// Zero and negative flags are stored as their results only until they need to be resolved
	unsigned int carryResult;			// C if 1 or 0 (same as ST_CRY)
	unsigned int zeroResult;			// Z if 0
	unsigned int negativeResult;		// N if ST_NEG is set

	// clock cycle counter
	unsigned int clocks;

	// next time instructions check for interrupts, PPU step, etc
	unsigned int nextClocks;

	// represents a low IRQ latch if any are non 0 and our CPU clocks are past a given counter (for speed), up to 4 supported
	unsigned int irqMask;
	unsigned int irqClock[4];

	// set the irq for the given irq number (clocks = 0 to immediately trigger)
	void setIRQ(int irqNum, unsigned int clocks) {
		// don't set clocks forward if already acknowledged
		if ((irqMask & (1 << irqNum)) && clocks > irqClock[irqNum])
			clocks = irqClock[irqNum];

		irqMask |= (1 << irqNum);
		irqClock[irqNum] = clocks;
	}

	// acknowledge the given irq number
	void ackIRQ(int irqNum) {
		irqMask &= ~(1 << irqNum);
	}

	// resolve the cached results to P
	void resolveToP();

	// resolve from P to the cached results
	void resolveFromP();
};

struct cpu_instr_history {
	cpu_6502 regs;			// copy of CPU pre op and address
	unsigned char instr;	// instruction byte
	unsigned char data1;
	unsigned char data2;	
	unsigned short effAddr;	// not used by all instructions
	unsigned char effByte;  // effective address byte
	
	void output();
	bool isEmpty() const {
		return instr == 0 && data1 == 0 && regs.A == 0 && regs.X == 0 && regs.Y == 0 && regs.SP == 0;
	}
};

#define ST_CRY_BIT (0)
#define ST_ZRO_BIT (1)
#define ST_INT_BIT (2)
#define ST_BCD_BIT (3)
#define ST_BRK_BIT (4)
#define ST_OVR_BIT (6)
#define ST_NEG_BIT (7)

#define ST_CRY (1 << ST_CRY_BIT)
#define ST_ZRO (1 << ST_ZRO_BIT)
#define ST_INT (1 << ST_INT_BIT)
#define ST_BCD (1 << ST_BCD_BIT)
#define ST_BRK (1 << ST_BRK_BIT)
#define ST_OVR (1 << ST_OVR_BIT)
#define ST_NEG (1 << ST_NEG_BIT)
#define ST_UNUSED (1 << 5)

// initalizes vars needed for 6502 simulation
void cpu6502_Init();

// this is done statically to obtain static addressing optimizations
void cpu6502_Step();

// performs IRQ interrupt
void cpu6502_IRQ(int irqBit);

// runs device interrupt routine at the given vector address (if interrupt disable flag is 0)
void cpu6502_DeviceInterrupt(unsigned int vectorAddress, bool masked);

// runs software interrupt routine at the given vector address (if interrupt disable flag is 0)
void cpu6502_SoftwareInterrupt(unsigned int vectorAddress);

#if NES
#include "nes.h"
#include "nes_cpu.h"
extern nes_cpu mainCPU ALIGN(256);
#endif

#define CPU_RAM(X) mainCPU.RAM[X]

#if TARGET_WINSIM
#define TRACE_DEBUG 1
#else
#define TRACE_DEBUG 0
#endif