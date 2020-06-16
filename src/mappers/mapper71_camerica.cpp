
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Camerica Mapper 71

void Mapper71_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.writeDirect(address, value);
			}
		} else if (address >= 0xC000) {
			// bank select
			value &= (nesCart.numPRGBanks - 1) & (0xF);

			nesCart.MapProgramBanks(0, value * 2, 2);
		} else if (address == 0x9000) {
			// Firehawk special case
			nesPPU.setMirrorType((value & 0x10) == 0 ? nes_mirror_type::MT_SINGLE : nes_mirror_type::MT_SINGLE_UPPER);
		}
	}
}

void nes_cart::setupMapper71_Camerica() {
	writeSpecial = Mapper71_writeSpecial;

	cachedBankCount = availableROMBanks;

	// read CHR bank (always one RAM) directly into PPU chrMap if not in ROM
	if (numCHRBanks == 1) {
		MapCharacterBanks(0, 0, 8);
	} else {
		DebugAssert(numCHRBanks == 0);
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
}