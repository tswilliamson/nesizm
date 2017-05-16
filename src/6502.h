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
	unsigned int P;		// status flags, 8-bit
};

#define CARRY_BIT (1 << 0)

#if NES
#include "nes_cpu.h"
extern nes_cpu mainCPU;
#endif

// this is done statically to obtain static addressing optimizations
void cpu6502_Step();