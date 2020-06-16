#pragma once
// inlined header file for cpu_6502 sub-struct so we can include NES specific functionality

extern unsigned char openBus[256];

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

	// high byte memory map (with wraparound).
	// This uses Blargg's concept of page biasing to avoid needing an address mask
	// See http://blargg.8bitalley.com/nes-emu/6502.html
	unsigned char* _map[0x101] ALIGN(256);

	// memory map for special address (mirrored from 0x4000 to 0x6000)
	unsigned char specialMemory[256];

	FORCE_INLINE unsigned char read(unsigned int addr) {
		accessTable[addr >> 13] = addr;
		return _map[addr >> 8][addr];
	}

	FORCE_INLINE unsigned char readNonIO(unsigned int addr) {
		DebugAssert(addr < 0x2000 || addr >= 0x5000); // shouldn't occur in special memory
		return _map[addr >> 8][addr];
	}

	FORCE_INLINE unsigned char* getNonIOMem(unsigned int addr) {
		return &_map[addr >> 8][addr];
	}

	FORCE_INLINE void write(unsigned int addr, unsigned char value) {
		if (addr >= 0x2000) {
			writeSpecial(addr, value);
		} else {
			_map[addr >> 8][addr] = value;
		}
	}

	FORCE_INLINE void writeDirect(unsigned int addr, unsigned char value) {
		_map[addr >> 8][addr] = value;
	}

	// inline mapping helpers
	FORCE_INLINE void setMap(unsigned int startAddrHigh, unsigned int numBlocks, unsigned char* ptr) {
		// 256 byte increments
		for (unsigned int i = 0; i < numBlocks; i++) {
			_map[startAddrHigh] = &ptr[i * 0x100] - (startAddrHigh << 8);
			startAddrHigh++;
		}
	}

	FORCE_INLINE void setMapOpenBus(unsigned int startAddrHigh, unsigned int numBlocks) {
		for (unsigned int i = 0; i < numBlocks; i++) {
			_map[startAddrHigh] = openBus - (startAddrHigh << 8);
			startAddrHigh++;
		}
	}

	FORCE_INLINE void setMapKB(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
		setMap(startAddrHigh, numKB * 4, ptr);
	}

	FORCE_INLINE void setMapOpenBusKB(unsigned int startAddrHigh, unsigned int numKB) {
		setMapOpenBus(startAddrHigh, numKB * 4);
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

	// synchronizes the various clock counters down to avoid 32 bit wraparound
	void syncClocks();
};
