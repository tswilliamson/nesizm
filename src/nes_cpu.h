#pragma once
// inlined header file for cpu_6502 sub-struct so we can include NES specific functionality

struct nes_cpu : public cpu_6502 {
	// each 8 KB page access is stored to determine if we need to effect hardware from a read
	unsigned int accessTable[8];

	// clocks for next PPU update
	unsigned int ppuClocks;

	// clocks for next APU update
	unsigned int apuClocks;

	// indicates that an NMI should occur on completion of next cpu instruction
	bool ppuNMI;

	void latchedSpecial(unsigned int addr);

	// Main RAM (zero page at 0x000, stack at 0x100, mirrored every 2 kb to 0x2000)
	unsigned char RAM[0x800] ALIGN(256);

	// high byte memory map (with wraparound)
	unsigned char* map[0x101] ALIGN(256);

	// memory map for special address (mirrored from 0x4000 to 0x6000)
	unsigned char specialMemory[256];

	FORCE_INLINE unsigned char read(unsigned int addr) {
		accessTable[addr >> 13] = addr;
		return map[addr >> 8][addr & 0xFF];
	}

	FORCE_INLINE unsigned char readNonIO(unsigned int addr) {
		DebugAssert(addr < 0x2000 || addr >= 0x5000); // shouldn't occur in special memory
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