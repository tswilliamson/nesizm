
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC2 (Punch Out mapper) / MMC4 (Fire Emblem)

inline void MMC2_selectCHRMap() {
	if (MMC2_LOLATCH) {
		nesCart.MapCharacterBanks(0, MMC2_CHR_LOW_FE * 4, 4);
	} else {
		nesCart.MapCharacterBanks(0, MMC2_CHR_LOW_FD * 4, 4);
	}

	if (MMC2_HILATCH) {
		nesCart.MapCharacterBanks(4, MMC2_CHR_HIGH_FE * 4, 4);
	} else {
		nesCart.MapCharacterBanks(4, MMC2_CHR_HIGH_FD * 4, 4);
	}
}

void MMC2_renderLatch(unsigned int address) {
	bool bNeedCommit = false;

	if (address < 0x1000) {
		if (address == 0x0FD8) {
			if (MMC2_LOLATCH == 1) {
				MMC2_LOLATCH = 0;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		} else if (address == 0x0FE8) {
			if (MMC2_LOLATCH == 0) {
				MMC2_LOLATCH = 1;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		}
	} else {
		if (address >= 0x1FD8 && address <= 0x1FDF) {
			if (MMC2_HILATCH == 1) {
				MMC2_HILATCH = 0;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		} else if (address >= 0x1FE8 && address <= 0x1FEF) {
			if (MMC2_HILATCH == 0) {
				MMC2_HILATCH = 1;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		}
	}

	if (bNeedCommit) {
		nesCart.CommitChrBanks();
	}
}

void nes_cart::MMC2_StateLoaded() {
	MMC2_selectCHRMap();
	nesCart.MapProgramBanks(0, MMC2_PRG_SELECT, 1);
}

void MMC2_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000 && nesCart.numRAMBanks) {
			// RAM
			mainCPU.map[address >> 8][address & 0xFF] = value;
		} else if (address >= 0xA000) {
			if (address < 0xB000) {
				// PRG ROM select
				value &= 0xF;
				MMC2_PRG_SELECT = value;
				if (nesCart.mapper == 9) {
					nesCart.MapProgramBanks(0, MMC2_PRG_SELECT, 1);
				} else if (nesCart.mapper == 10) {
					nesCart.MapProgramBanks(0, MMC2_PRG_SELECT * 2, 2);
				}
			}
			else if (address < 0xC000) {
				// low CHR / FD select
				value &= 0x1F;
				if (value != MMC2_CHR_LOW_FD) {
					// map to lower 4k of bank 0 and 2
					MMC2_CHR_LOW_FD = value;
					MMC2_selectCHRMap();
				}
			} else if (address < 0xD000) {
				// low CHR / FE select
				value &= 0x1F;
				if (value != MMC2_CHR_LOW_FE) {
					// map to lower 4k of bank 1 and 3
					MMC2_CHR_LOW_FE = value;
					MMC2_selectCHRMap();
				}
			} else if (address < 0xE000) {
				// high CHR / FD select
				value &= 0x1F;
				if (value != MMC2_CHR_HIGH_FD) {
					// map to higher 4k of bank 0 and 1
					MMC2_CHR_HIGH_FD = value;
					MMC2_selectCHRMap();
				}
			} else if (address < 0xF000) {
				// high CHR / FE select
				value &= 0x1F;
				if (value != MMC2_CHR_HIGH_FE) {
					// map to higher 4k of bank 2 and 3
					MMC2_CHR_HIGH_FE = value;
					MMC2_selectCHRMap();
				}
			}
			else {
				// mirror select
				if (value & 1) {
					nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
				} else {
					nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
				}
			}
		}
	}
}

void nes_cart::setupMapper9_MMC2() {
	writeSpecial = MMC2_writeSpecial;
	renderLatch = MMC2_renderLatch;

	cachedBankCount = availableROMBanks;

	MMC2_LOLATCH = 0;
	MMC2_HILATCH = 0;
	MMC2_PRG_SELECT = -1;
	MMC2_CHR_LOW_FD = 0;
	MMC2_CHR_LOW_FE = 0;
	MMC2_CHR_HIGH_FD = 0;
	MMC2_CHR_HIGH_FE = 0;

	// map memory to read-in ROM
	if (mapper == 9) {
		MapProgramBanks(0, 0, 1);
		MapProgramBanks(1, numPRGBanks * 2 - 3, 3);
	} else if (mapper == 10) {
		MapProgramBanks(0, 0, 2);
		MapProgramBanks(2, numPRGBanks * 2 - 2, 2);
	}

	MMC2_selectCHRMap();

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}
