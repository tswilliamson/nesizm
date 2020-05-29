
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Nanjing Mapper 163
//
// Much due credit to FCEUX implementation for this mapper. See fceux/164.cpp

void Mapper163_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000 && nesCart.numRAMBanks) {
			// RAM
			mainCPU.map[address >> 8][address & 0xFF] = value;
		} else {
			// does nothing!
		}
	} else {
		if (address == 0x5101) {
			if (Mapper163_STROBE && value == 0) {
				Mapper163_TRIGGER ^= 1;
			}
			Mapper163_STROBE = value;
		} else if (address == 0x5100 && value == 0x06) {
			Mapper163_REG[4] = 1;
		} else {
			switch (address & 0x7300) {
			case 0x5000:
				Mapper163_REG[1] = value;
				Mapper163_REG[4] = 0;
				break;
			case 0x5200:
				Mapper163_REG[0] = value;
				Mapper163_REG[4] = 0;
				break;
			case 0x5300:
				Mapper163_REG[2] = value;
				break;
			case 0x5100:
				Mapper163_REG[3] = value;
				Mapper163_REG[4] = 0;
				break;
			}
		}

		nesCart.Mapper163_Update();
	}
}

void nes_cart::Mapper163_ScanlineClock() {
	// if high bit of reg 1 is set, then first 4 KB of CHR RAM are mirrored for first half of screen and then this is swapped
	if (Mapper163_REG[1] & 0x80) {
		const int chrPage = nesCart.cachedBankCount + 1;
		if (nesPPU.scanline == 240) {
			nesPPU.chrPages[0] = nesCart.cache[chrPage].ptr;
			nesPPU.chrPages[1] = nesCart.cache[chrPage].ptr;
		} else if (nesPPU.scanline == 128) {
			nesPPU.chrPages[0] = nesCart.cache[chrPage].ptr + 0x1000;
			nesPPU.chrPages[1] = nesCart.cache[chrPage].ptr + 0x1000;
		}
	}
}

void nes_cart::Mapper163_Update() {
	// map selected program bank
	int prgBank;
	if (Mapper163_REG[4]) {
		// use copy protection page
		prgBank = 3;
	} else {
		// use normally register mapped page
		prgBank = (Mapper163_REG[0] << 4) | (Mapper163_REG[1] & 0x0F);
	}
	MapProgramBanks(0, (prgBank * 4) & (numPRGBanks * 2 - 1), 4);

	// not using the chr flip mode
	if ((Mapper163_REG[1] & 0x80) == 0) {
		const int chrPage = cachedBankCount + 1;
		nesPPU.chrPages[0] = cache[chrPage].ptr;
		nesPPU.chrPages[1] = cache[chrPage].ptr + 0x1000;
	}

	// update protect page values
	unsigned char val5100 = (Mapper163_REG[2] | Mapper163_REG[0] | Mapper163_REG[1] | Mapper163_REG[3]) ^ 0xff;
	unsigned char val5500 = Mapper163_TRIGGER ? Mapper163_REG[2] | Mapper163_REG[1] : 0;
	const int protectPage = cachedBankCount;
	cache[protectPage].ptr[0x100] = val5100;
	cache[protectPage].ptr[0x500] = val5500;
}

void nes_cart::setupMapper163_Nanjing() {
	// available banks minus 1 for WRAM, 1 for CHR RAM, 1 for protection cpu mapping
	cachedBankCount = availableROMBanks - 2;

	const int protectPage = cachedBankCount;
	const int chrPage = cachedBankCount + 1;
	const int ramPage = cachedBankCount + 2;

	// these carts only use CHR RAM
	DebugAssert(!numCHRBanks);
	// chr map uses best bank for chr caching locality:
	nesPPU.chrPages[0] = cache[chrPage].ptr;
	nesPPU.chrPages[1] = cache[chrPage].ptr + 0x1000;

	scanlineClock = nes_cart::Mapper163_ScanlineClock;
	writeSpecial = Mapper163_writeSpecial;

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[ramPage].ptr);
	}

	// map repeat 2 KB to 0x5000 - 0x6000 
	mapCPU(0x50, 2, cache[protectPage].ptr);
	mapCPU(0x58, 2, cache[protectPage].ptr);

	// fill protect page with 4, apparently
	for (int32 i = 0; i < 2048; i++) {
		cache[protectPage].ptr[i] = 4;
	}

	// strobe starts high
	Mapper163_STROBE = 1;

	Mapper163_Update();
}