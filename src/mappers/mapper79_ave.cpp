#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mapper 79 - American Video Entertainment mapper

void Mapper79_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x4000) {
		// very simple register mask
		if ((address & 0xE100) == 0x4100) {
			Mapper79_CONTROL_PRG = (value & 0x8) >> 3;
		}
		Mapper79_CONTROL_CHAR = value & 0x7;
		nesCart.Mapper79_Update();
	}
}

void nes_cart::Mapper79_Update() {
	unsigned int prgBank = (Mapper79_CONTROL_PRG * 4) & (numPRGBanks * 2 - 1);
	MapProgramBanks(0, prgBank, 4);

	unsigned int chrBank = (Mapper79_CONTROL_CHAR & (numCHRBanks - 1)) * 8;
	MapCharacterBanks(0, chrBank, 8);
}

void nes_cart::setupMapper79_AVE() {
	writeSpecial = Mapper79_writeSpecial;

	cachedBankCount = availableROMBanks;

	// only CHR ROM
	DebugAssert(numCHRBanks);
	MapCharacterBanks(0, 0, 8);

	// map first 32 KB of PRG mamory to 80-FF by default
	MapProgramBanks(0, 0, 4);

	// RAM bank is not supported
}
