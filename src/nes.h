
#pragma once

// NES emulator specific header

// specifications about the cart, rom file, mapper, etc
struct nes_cart {
	nes_cart();

	int handle;					// current file handle
	char file[128];				// file name

	int mapper;					// mapper ID used with ROM
	int numPRGBanks;			// num 16 kb Program ROM banks
	int numCHRBanks;			// num 8 kb CHR ROM banks
	int numRAMBanks;			// num 8 kb RAM banks

	int isBatteryBacked;		// 0 if no battery backup
	int isPAL;					// 1 if PAL, 0 if NTSC

	// Loads a ROM from the given file path
	bool loadROM(const char* withFile);

	// Sets up loaded ROM File with the selected mapper (returns false if unsupported)
	bool setupMapper();

	// various mapper setups
	void setupMapper0();
};

// ppu properties and vram
struct nes_ppu {
	enum mirror_type {
		MT_HORIZONTAL,
		MT_VERTICAL,
		MT_4PANE
	};

	nes_ppu();

	mirror_type mirror;
};

// current nes main memory, memory map and settings
struct nes_mem {
	// always around
	unsigned char zeroPage[0x100];
	unsigned char stack[0x100];
	unsigned char RAM[0x600];

	// high byte memory map
	unsigned char* map[0x100]; 

	FORCE_INLINE unsigned char read(unsigned int addr) {
		if (addr >= 0x2000 && addr <= 0x401F)
			return readSpecial(addr);
		else
			return map[addr >> 8][addr & 0xFF];
	}

	FORCE_INLINE void write(unsigned int addr, unsigned char value) {
		if (addr >= 0x2000) {
			writeSpecial(addr, value);
		} else {
			map[addr >> 8][addr & 0xFF] = value;
		}
	}

	unsigned char readSpecial(unsigned int addr);
	void writeSpecial(unsigned int addr, unsigned char value);
};

extern nes_cart nesCart;
extern nes_mem nesMem;
extern nes_ppu nesPPU;