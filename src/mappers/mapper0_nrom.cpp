
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NROM

void NROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000 && nesCart.numRAMBanks) {
			// RAM
			mainCPU.writeDirect(address, value);
		} else {
			// does nothing!
		}
	}
}

void nes_cart::setupMapper0_NROM() {
	cachedBankCount = availableROMBanks;

	writeSpecial = NROM_writeSpecial;

	// read CHR bank (always one ROM) directly into PPU chrMap
	DebugAssert(numCHRBanks == 1);
	MapCharacterBanks(0, 0, 8);

	// map memory to read-in ROM
	if (numPRGBanks == 1) {
		MapProgramBanks(0, 0, 2);
		MapProgramBanks(2, 0, 2);
	} else {
		DebugAssert(numPRGBanks == 2);
		MapProgramBanks(0, 0, 4);
	}

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mainCPU.setMapKB(0x60, 8, cache[availableROMBanks].ptr);
	}
}