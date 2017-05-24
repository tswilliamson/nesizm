
// NES memory function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_cpu mainCPU;

unsigned char* nes_cpu::getSpecial(unsigned int addr) {
	if (addr < 0x4000) {
		DebugAssert(addr >= 0x2000);	// assumed to be PPU register then
		return nesPPU.latchReg(addr & 0x07);
	}

	static unsigned char specByte = 0;
	DebugAssert(0);
	return &specByte;
}

void nes_cpu::postSpecialRead(unsigned int addr) {
	if (addr < 0x4000) {
		DebugAssert(addr >= 0x2000);	// assumed to be PPU register then
		return;
	}
	DebugAssert(0);
}

unsigned char nes_cpu::readSpecial(unsigned int addr) {
	unsigned char *spec = getSpecial(addr);
	unsigned char ret = *spec;
	postSpecialRead(addr);
	return ret;
}

void nes_cpu::writeSpecial(unsigned int addr, unsigned char value) {
	if (addr < 0x4000) {
		DebugAssert(addr >= 0x2000);	// assumed to be PPU register then
		nesPPU.writeReg(addr & 0x07, value);
		return;
	}
	DebugAssert(0);
}

void nes_cpu::mapDefaults() {
	memset(map, 0, sizeof(map));

	// 0x0000 - 0x2000 is RAM and its mirrors
	for (int m = 0x00; m < 0x20; m++) {
		map[m] = &RAM[(m & 0x7) * 0x100];
	}

	// remainder is by default unmapped (up to each mapper)
}

// reset the CPU (assumes memory mapping is set up properly for this)
void nes_cpu::reset() {
	// CPU set to match FCEUX for debugging

	// RAM reset
	for (int i = 0; i < 0x800; i += 8) {
		RAM[i + 0] = 0;
		RAM[i + 1] = 0;
		RAM[i + 2] = 0;
		RAM[i + 3] = 0;
		RAM[i + 4] = 255;
		RAM[i + 5] = 255;
		RAM[i + 6] = 255;
		RAM[i + 7] = 255;
	}

	// set to interrupt
	PC = read(0xFFFC) + (read(0xFFFD) << 8);

	// stack pointer technically came from $0000
	RAM[0x01FD] = 0;
	RAM[0x01FC] = 2;
	RAM[0x01FB] = 48;
	SP = 0xFA;

	// rest of the registers are 0
	A = 0;
	X = 0;
	Y = 0;
	P = 0;

	clocks = 7;
}