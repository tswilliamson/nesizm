#pragma once

// 6502 chip emulation specific header (with NES specialization in sub-struct to allow for add'l extension later if we want it)

struct cpu_6502 {
	// main registers (stored as ints due to improved 32-bit speed, but only use the relevant bits)
	unsigned int PC;	// program counter, 16-bit
	unsigned int SP;	// stack pointer (offset from 0x100), 8-bit
	unsigned int A;		// accumulator and index registers, 8-bit
	unsigned int X;
	unsigned int Y;
	unsigned int P;		// status flags, 8-bit
};

#if NES
#include "nes_cpu.h"
extern nes_cpu mainCPU;
#endif

// this is done statically to obtain static addressing optimizations
void cpu6502_Step();