
// NES memory function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_cpu mainCPU ALIGN(256);

void nes_cpu::latchedSpecial(unsigned int addr) {
	if (addr == 0x4015) {
		nesAPU.clearFrameIRQ();
	} else if (addr == 0x4016) {
		input_readController1();
	} else if (addr == 0x4017) {
		input_readController2();
	} else {
		return;
	}
}

void nes_cpu::writeSpecial(unsigned int addr, unsigned char value) {
	if (addr < 0x4000) {
		DebugAssert(addr >= 0x2000);	// assumed to be PPU register then
		nesPPU.writeReg(addr & 0x07, value);
		return;
	} else if (addr < 0x4020) {
		// APU and IO registers
		switch (addr - 0x4000) {
		case 0x14:
			// perform OAM dma
			nesPPU.oamDMA(((int)value) << 8);
			break;
		case 0x16:
			input_writeStrobe(value);
			break;
		default:
			// other wise this is an APU register
			nesAPU.writeReg(addr - 0x4000, value);
			break;
		}
	} else if (addr < 0x10000) {
		nesCart.writeSpecial(addr, value);
	} else {
		write(addr & 0xFFFF, value);
	}
}

void nes_cpu::mapDefaults() {
	memset(_map, 0, sizeof(_map));

	// 0x0000 - 0x2000 is RAM and its mirrors
	for (int m = 0x00; m < 0x20; m++) {
		setMap(m + 0x00, 1, &RAM[(m & 0x7) * 0x100]);
		setMap(m + 0x20, 1, nesPPU.memoryMap);
		setMap(m + 0x40, 1, specialMemory);
	}

	// wrap around
	_map[0x100] = _map[0];

	// remainder is by default unmapped (up to each mapper)
}

// reset the CPU (assumes memory mapping is set up properly for this)
void nes_cpu::reset() {
	// CPU set to match FCEUX for debugging
	ppuNMI = false;

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
	ppuClocks = 2510;

	// start with apuClocks in one frame
	apuClocks = 7458;
	
	// all irqs high
	irqMask = 0;
	irqClock[0] = 0;
	irqClock[1] = 0;
	irqClock[2] = 0;
	irqClock[3] = 0;

	// trigger reset interrupt
	cpu6502_SoftwareInterrupt(0xFFFC);
}

void cpu6502_IRQ(int irqBit) {
	// 0 is cart IRQ, 1-2 APU IRQ
	if (irqBit == 0 && nesCart.IRQReached() == false)
		return;
	else if ((irqBit == 1 || irqBit == 2) && nesAPU.IRQReached(irqBit) == false) {
		return;
	}

	// perform interrupt if they are enabled
	if ((mainCPU.P & ST_INT) == 0) {
		cpu6502_SoftwareInterrupt(0xFFFE);
	}
}

void nes_cpu::syncClocks() {
	// at a billion cycles, reduve by 500m cycles
	const unsigned int highThresh =  1000000000;
	const unsigned int reduction = 500000000;
	if (clocks > highThresh) {
		clocks -= reduction;
		nextClocks -= reduction;
		ppuClocks -= reduction;
		apuClocks -= reduction;
		if (irqClock[0]) irqClock[0] -= reduction;
		if (irqClock[1]) irqClock[0] -= reduction;
		if (irqClock[2]) irqClock[0] -= reduction;

		nesCart.rollbackClocks(reduction);
	}
}