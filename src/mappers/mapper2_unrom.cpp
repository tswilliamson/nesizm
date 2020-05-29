#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNROM

void UNROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			}
		} else {
			// bank select
			value &= nesCart.numPRGBanks - 1;

			nesCart.MapProgramBanks(0, value * 2, 2);
		}
	}
}

void nes_cart::setupMapper2_UNROM() {
	writeSpecial = UNROM_writeSpecial;

	cachedBankCount = availableROMBanks - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one ROM.. or RAM?) directly into PPU chrMap
	if (numCHRBanks == 1) {
		MapCharacterBanks(0, 0, 8);
	} else {
		nesPPU.chrPages[0] = cache[chrBank].ptr;
		nesPPU.chrPages[1] = cache[chrBank].ptr + 0x1000;
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[chrBank + 1].ptr);
	}
}
