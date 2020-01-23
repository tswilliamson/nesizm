#pragma once
// inlined header file for cpu_6502 sub-struct so we can include NES specific functionality

struct nes_cpu : public cpu_6502_with_RAM {
	// clocks for next PPU update
	unsigned int ppuClocks;

	// indicates that an NMI should occur on completion of next cpu instruction
	bool ppuNMI;

	unsigned char readSpecial(unsigned int addr);

	FORCE_INLINE unsigned char read(unsigned int addr) {
		// TODO: play with this math and determine fastest instructional approach
		if (addr < 0x2000 || addr >= 0x6000) {
			return map[addr >> 8][addr & 0xFF];
		} else if (addr < 0x4000) {
			return ppu_registers.latchReg(addr);
		} else {
			return readSpecial(addr);
		}
	}

	FORCE_INLINE unsigned char readNonIO(unsigned int addr) {
		return map[addr >> 8][addr & 0xFF];
	}

	FORCE_INLINE unsigned char* getNonIOMem(unsigned int addr) {
		return &map[addr >> 8][addr & 0xFF];
	}

	FORCE_INLINE void write(unsigned int addr, unsigned char value) {
		if (addr >= 0x2000) {
			writeSpecial(addr, value);
		} else {
			map[addr >> 8][addr & 0xFF] = value;
		}
	}

	FORCE_INLINE void push(unsigned int byte) {
		RAM[0x100 | (SP & 0xFF)] = byte;
		SP--;
	}

	FORCE_INLINE unsigned int pop() {
		SP++;
		return RAM[0x100 | (SP & 0xFF)];
	}

	// performs write operation on byte (when >= 0x2000)
	void writeSpecial(unsigned int addr, unsigned char value);

	// triggers CPU NMI interrupt
	FORCE_INLINE void NMI() {
		cpu6502_DeviceInterrupt(0xFFFA, false);
	}

	// map default memory for CPU (zero page, stack, RAM, mirrors, etc)
	void mapDefaults();

	// reset the CPU (assumes memory mapping is set up properly for this)
	void reset();
};