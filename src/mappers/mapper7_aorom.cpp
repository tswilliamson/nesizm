
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AOROM (switches nametables for single screen mirroring)

inline void AOROM_MapNameBank(int tableNum) {
	nesPPU.setMirrorType(tableNum == 0 ? nes_mirror_type::MT_SINGLE : nes_mirror_type::MT_SINGLE_UPPER);
}

void AOROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.writeDirect(address, value);
			}
		} else {
			// nametable select
			unsigned int nametable = (value >> 4) & 1;
			if (nametable != AOROM_CUR_NAMETABLE) {
				AOROM_MapNameBank(nametable);
				AOROM_CUR_NAMETABLE = nametable;
			}

			// bank select

			// mask to the number of 32 kb banks we have
			value &= nesCart.numPRGBanks / 2 - 1;

			// 32 kb at a time
			nesCart.MapProgramBanks(0, value * 4, 4);
		}
	}
}

void nes_cart::AOROM_StateLoaded() {
	AOROM_MapNameBank(AOROM_CUR_NAMETABLE);
}

void nes_cart::setupMapper7_AOROM() {
	writeSpecial = AOROM_writeSpecial;

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

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mainCPU.setMapKB(0x60, 8, cache[availableROMBanks].ptr);
	}

	// set up single screen mirroring
	AOROM_MapNameBank(0);
	nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE);
}