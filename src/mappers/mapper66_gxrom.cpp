#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mapper 66 GXROM (also Mapper 140)

void GXROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000 && nesCart.mapper != 140) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			}
		} else if (nesCart.mapper == 66 || address < 0x8000) {
			// bank select
			int prg = (((value & 0x30) >> 4) * 4) & (nesCart.numPRGBanks * 2 - 1);
			int chr = (value & 3) * 8;

			nesCart.MapProgramBanks(0, prg, 4);
			nesCart.MapCharacterBanks(0, chr, 8);
		}
	}
}

void nes_cart::setupMapper66_GXROM() {
	writeSpecial = GXROM_writeSpecial;

	cachedBankCount = availableROMBanks;

	// read CHR bank (always one ROM.. or RAM?) directly into PPU chrMap
	if (numCHRBanks == 1) {
		MapCharacterBanks(0, 0, 8);
	} else {
		cachedBankCount--;
		int chrBank = cachedBankCount;
		nesPPU.chrPages[0] = cache[chrBank].ptr;
		nesPPU.chrPages[1] = cache[chrBank].ptr + 0x1000;
	}

	// map first 32 KB of PRG mamory to 80-FF by default
	MapProgramBanks(0, 0, 4);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}
