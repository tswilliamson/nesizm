#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mapper 34 : Both BNROM and NINA-1 support

void nes_cart::Mapper34_Sync() {
	int bank = Mapper34_PRG_BANK * 4;
	bank &= nesCart.numPRGBanks * 2 - 1;
	nesCart.MapProgramBanks(0, bank, 4);

	bank = Mapper34_CHR1 * 4;
	bank &= nesCart.numCHRBanks * 8 - 1;
	nesCart.MapCharacterBanks(0, bank, 4);

	bank = Mapper34_CHR2 * 4;
	bank &= nesCart.numCHRBanks * 8 - 1;
	nesCart.MapCharacterBanks(4, bank, 4);
}

void BNROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (nesCart.numCHRBanks > 1) {
			// NINA-1
			if (address == 0x7FFD) {
				// prg select
				Mapper34_PRG_BANK = value;
				nesCart.Mapper34_Sync();
				return;
			}
			if (address == 0x7FFE) {
				// chr1 select
				Mapper34_CHR1 = value & 0xF;
				nesCart.Mapper34_Sync();
				return;
			}
			if (address == 0x7FFF) {
				// chr2 select
				Mapper34_CHR2 = value & 0xF;
				nesCart.Mapper34_Sync();
				return;
			}
		}

		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.writeDirect(address, value);
			}
		} else {
			// bank select
			Mapper34_PRG_BANK = value;
			nesCart.Mapper34_Sync();
		}
	}
}

void nes_cart::setupMapper34_BNROM() {
	writeSpecial = BNROM_writeSpecial;

	cachedBankCount = availableROMBanks;

	// read CHR bank (ROM or RAM) directly into PPU chrMap
	if (numCHRBanks >= 1) {
		MapCharacterBanks(0, 0, 8);
		Mapper34_CHR1 = 0;
		Mapper34_CHR2 = 1;
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
		mainCPU.setMapKB(0x60, 8, cache[availableROMBanks].ptr);
	}
}
