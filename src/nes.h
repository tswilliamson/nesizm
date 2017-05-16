
#pragma once

// NES emulator specific header

#define NES 1
#include "6502.h"

#define NUM_CACHED_ROM_BANKS 20

// specifications about the cart, rom file, mapper, etc
struct nes_cart {
	nes_cart();

	int handle;						// current file handle
	char file[128];					// file name

	int mapper;						// mapper ID used with ROM
	int numPRGBanks;				// num 16 kb Program ROM banks
	int numCHRBanks;				// num 8 kb CHR ROM banks
	int numRAMBanks;				// num 8 kb RAM banks

	int isBatteryBacked;			// 0 if no battery backup
	int isPAL;						// 1 if PAL, 0 if NTSC

	// 8kb ROM banks for various usages based on mapper (allocated on stack due to Prizm deficiencies
	unsigned char* romBanks[NUM_CACHED_ROM_BANKS];	

	// sets up ROM bank pointers with the given allocated data
	void allocateROMBanks(unsigned char* withAlloced);

	// Loads a ROM from the given file path
	bool loadROM(const char* withFile);

	// Sets up loaded ROM File with the selected mapper (returns false if unsupported)
	bool setupMapper();

	// various mapper setups
	void setupMapper0_NROM();
};

// ppu properties and vram
struct nes_ppu {
	enum mirror_type {
		MT_HORIZONTAL,
		MT_VERTICAL,
		MT_4PANE
	};

	// all 8 kb mapped for character memory at once
	unsigned char* chrMap;

	nes_ppu();

	mirror_type mirror;
};

extern nes_cart nesCart;
extern nes_ppu nesPPU;