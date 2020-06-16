
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNROM (switchable CHR banks)

void CNROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x8000) {
		// CHR bank select
		value &= nesCart.numCHRBanks - 1;

		if (value != CNROM_CUR_CHR_BANK) {
			nesCart.MapCharacterBanks(0, value * 8, 8);
			CNROM_CUR_CHR_BANK = value;
		}
	}
}

void nes_cart::setupMapper3_CNROM() {
	writeSpecial = CNROM_writeSpecial;

	cachedBankCount = availableROMBanks - 1;

	// map first 16 KB of PRG mamory to 80-BF, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// cache first CHR bank by default
	MapCharacterBanks(0, 0, 8);
	CNROM_CUR_CHR_BANK = 0;

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mainCPU.setMapKB(0x60, 8, cache[availableROMBanks].ptr);
	}
}