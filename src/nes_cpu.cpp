
// NES memory function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_cpu mainCPU;

unsigned char* nes_cpu::getSpecial(unsigned int addr) {
	if (addr < 0x4000) {
		DebugAssert(addr >= 0x2000);	// assumed to be PPU register then
		return ppu_registers.latchReg(addr & 0x07);
	} 

	static unsigned char specByte = 0;

	if (addr < 0x4020) {
		// APU and IO registers
		switch (addr - 0x4000) {
			case 0x16:
				specByte = input_readController1();
				break;
			case 0x17:
				specByte = input_readController2();
				break;
			default:
				// UNMAPPED! NO DATA!
				specByte = 0;
		}
	} 

	return &specByte;
}

unsigned char nes_cpu::readSpecial(unsigned int addr) {
	unsigned char *spec = getSpecial(addr);
	unsigned char ret = *spec;
	return ret;
}

void nes_cpu::writeSpecial(unsigned int addr, unsigned char value) {
	if (addr < 0x4000) {
		DebugAssert(addr >= 0x2000);	// assumed to be PPU register then
		ppu_registers.writeReg(addr & 0x07, value);
		return;
	} else if (addr < 0x4020) {
		// APU and IO registers
		switch (addr - 0x4000) {
			case 0x14:
				// perform OAM dma
				ppu_oamDMA(((int) value) << 8);
				break;
			case 0x16:
				input_writeStrobe(value);
				break;
			default:
				// UNMAPPED! WILL DO NOTHING
				break;
		}
	} else if (addr < 0x10000) {
		nesCart.writeSpecial(addr, value);
	} else {
		write(addr & 0xFFFF, value);
	}
}

void nes_cpu::mapDefaults() {
	memset(map, 0, sizeof(map));

	// 0x0000 - 0x2000 is RAM and its mirrors
	for (int m = 0x00; m < 0x20; m++) {
		map[m] = &RAM[(m & 0x7) * 0x100];
	}

	// wrap around
	map[0x100] = map[0];

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

	// rest of the registers are 0
	A = 0;
	X = 0;
	Y = 0;
	P = 0;

	PC = 0x0002;
	SP = 0xFD;

	clocks = 0;

	// comparing to FCEUX we appear to be just slightly ahead on clocks
	ppuClocks = 4;

	// trigger reset interrupt
	cpu6502_SoftwareInterrupt(0xFFFC);
}

void cpu6502_IRQ() {
	// disable IRQ
	mainCPU.irqClocks = 0;

	// perform interrupt if they are enabled
	if ((mainCPU.P & ST_INT) == 0) {
		cpu6502_SoftwareInterrupt(0xFFFE);
	}
}