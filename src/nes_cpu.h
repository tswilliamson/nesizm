#pragma once
// inlined header file for cpu_6502 sub-struct so we can include NES specific functionality

struct nes_cpu : public cpu_6502 {
	// Main RAM (zero page at 0x000, stack at 0x100, mirrored every 2 kb to 0x2000)
	unsigned char RAM[0x800];

	// high byte memory map (with wraparound)
	unsigned char* map[0x101];

	// clocks for next PPU update
	unsigned int ppuClocks;

	// indicates that an NMI should occur on completion of next cpu instruction
	bool ppuNMI;

	FORCE_INLINE unsigned char* getByte(unsigned int addr) {
		if (addr >= 0x2000 && addr <= 0x401F) {
			return getSpecial(addr);
		} else {
			return map[addr >> 8] + (addr & 0xFF);
		}
	}

	FORCE_INLINE unsigned char read(unsigned int addr) {
		// TODO: play with this math and determine fastest instructional approach
		if (addr >= 0x2000 && addr < 0x6000)
			return readSpecial(addr);
		else
			return map[addr >> 8][addr & 0xFF];
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

	// returns pointer to data for special address but does not perform a "read" operation if it effects it
	unsigned char* getSpecial(unsigned int addr);

	// combined special read (for off cpu ops) 
	unsigned char readSpecial(unsigned int addr);

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