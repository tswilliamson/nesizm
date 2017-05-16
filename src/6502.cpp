
// main 6502 interpreter and emulation
#include "platform.h"
#include "debug.h"

#define NES 1
#include "6502.h"

// the low 5 bits of the opcode determines the addressing mode with only 5 instruction exceptions (noted)
static int modeTable[32] = {
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

void cpu6502_Step() {
	// TODO : cache into single read (with instruction overlap in bank pages)
	unsigned int instr = mainCPU.read(mainCPU.PC++);
	unsigned int data1 = mainCPU.read(mainCPU.PC);
	unsigned int data2 = mainCPU.read(mainCPU.PC + 1);

	unsigned char* operand;
	unsigned int isSpecial = 0;
	unsigned int isWrite = 0;

	switch (modeTable[instr & 0x1F]) {
		case AM_Absolute:
			operand = mainCPU.getByte(data1 + (data2 << 8), isSpecial);
			break;
		case AM_AbsoluteX:
			operand = mainCPU.getByte(data1 + (data2 << 8) + (mainCPU.P & CARRY_BIT), isSpecial);
			break;
		case AM_IndirectX:
		{
			int target = (data1 + mainCPU.X) & 0xFF;
			operand = mainCPU.getByte(mainCPU.RAM[target] + (mainCPU.RAM[(target + 1) & 0xFF] << 8), isSpecial);
			break;
		}
		case AM_IndirectY:
		{
			operand = mainCPU.getByte(mainCPU.RAM[data1] + (mainCPU.RAM[(data1 + 1) & 0xFF] << 8) + mainCPU.Y, isSpecial);
			break;
		}
		case AM_Zero:
			operand = &mainCPU.RAM[data1];
			break;
		case AM_ZeroX:
			operand = &mainCPU.RAM[(data1 + mainCPU.X) & 0xFF];
			break;
		default:
			DebugAssert(modeTable[instr & 0x1F] == AM_None);
			break;
	}

	if (isSpecial) {
		if (isWrite) {
			mainCPU.postSpecialWrite(operand);
		} else {
			mainCPU.postSpecialRead(operand);
		}
	}
}