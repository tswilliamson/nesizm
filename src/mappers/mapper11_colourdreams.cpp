
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color Dreams Mapper 11

void Mapper11_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x8000) {
		unsigned int prg = (value & 0x03) * 4;
		unsigned int chr = (value & 0xF0) >> 4;
		prg &= nesCart.numPRGBanks * 2 - 1;

		if (prg != Mapper11_PRG_SELECT) {
			nesCart.MapProgramBanks(0, prg, 4);
			Mapper11_PRG_SELECT = prg;
		}

		if (chr != Mapper11_CHR_SELECT) {
			nesCart.MapCharacterBanks(0, chr * 8, 8);
			Mapper11_CHR_SELECT = chr;
		}
	}
}

void nes_cart::setupMapper11_ColorDreams() {
	writeSpecial = Mapper11_writeSpecial;

	cachedBankCount = availableROMBanks;

	// map first 32 KB of PRG mamory to 80-FF by default
	MapProgramBanks(0, 0, 4);

	MapCharacterBanks(0, 0, 8);

	Mapper11_PRG_SELECT = 0;
	Mapper11_CHR_SELECT = 0;
}
