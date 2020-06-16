#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC1

void MMC1_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			// if enabled and available
			if (nesCart.numRAMBanks && MMC1_RAM_DISABLE == 0) {
				// RAM
				mainCPU.writeDirect(address, value);
			} 
		} else {
			if (value & 0x80) {
				// clear shift register
				MMC1_SHIFT_BIT = 0;
				MMC1_SHIFT_VALUE = 0;

				// set PRG bank mode
				if (MMC1_PRG_BANK_MODE != 3) {
					nesCart.MMC1_Write(0x8000, MMC1_PRG_BANK_MODE | 0x0C);
				}
			} else {
				// set shift register bit
				MMC1_SHIFT_VALUE |= ((value & 1) << MMC1_SHIFT_BIT);
				MMC1_SHIFT_BIT++;

				if (MMC1_SHIFT_BIT == 5) {
					// shift register ready to go! send result and clear for another round
					nesCart.MMC1_Write(address, MMC1_SHIFT_VALUE);
					MMC1_SHIFT_BIT = 0;
					MMC1_SHIFT_VALUE = 0;
				}
			}
		}
	}
}

void nes_cart::MMC1_Write(unsigned int addr, int regValue) {

	// SUROM 256 select
	if (MMC1_BOARD_TYPE == MMC1_SUROM && addr >= 0xA000 && addr < 0xE000) {
		if (addr < 0xC000 || MMC1_CHR_BANK_MODE == 1) {
			int selectedPRGBlock = (regValue & 0x10) * 2;
			if ((MMC1_PRG_BANK_1 & 0x20) != selectedPRGBlock) {
				MMC1_PRG_BANK_1 = (MMC1_PRG_BANK_1 & 0x1F) | selectedPRGBlock;
				MMC1_PRG_BANK_2 = (MMC1_PRG_BANK_2 & 0x1F) | selectedPRGBlock;
				MapProgramBanks(0, MMC1_PRG_BANK_1, 2);
				MapProgramBanks(2, MMC1_PRG_BANK_2, 2);
			}
		}
	}

	if (addr < 0xA000) {
		// Control
		// mirroring in bottom 2 bits
		switch (regValue & 3) {
			case 0:
				// one screen lower bank
				nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE);
				break;
			case 1:
				// one screen upper bank
				nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE_UPPER);
				break;
			case 2:
				// vertical
				nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
				break;
			case 3:
				// horizontal
				nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
				break;
		}

		// bank modes
		unsigned int newPRGBankMode = (regValue & 0x0C) >> 2;
		if (MMC1_PRG_BANK_MODE != newPRGBankMode) {
			MMC1_PRG_BANK_MODE = newPRGBankMode;

			if (newPRGBankMode == 2) {
				// fix lower
				MMC1_PRG_BANK_1 = (MMC1_PRG_BANK_1 & 0x20);
				MapProgramBanks(0, MMC1_PRG_BANK_1, 2);
			}
			if (newPRGBankMode == 3) {
				// fix upper
				MMC1_PRG_BANK_2 = ((numPRGBanks - 1) & 0xF) * 2 + (MMC1_PRG_BANK_1 & 0x20);
				MapProgramBanks(2, MMC1_PRG_BANK_2, 2);
			}
		}

		MMC1_CHR_BANK_MODE = (regValue & 0x10) >> 4;
	} else if (addr < 0xC000) {
		// CHR Bank 0
		if (numCHRBanks) {
			if (MMC1_CHR_BANK_MODE == 1) {
				// 4 kb mode
				regValue &= numCHRBanks * 2 - 1;
				MapCharacterBanks(0, regValue * 4, 4);
			} else {
				// 8 kb mode (drop bottom bank bit)
				regValue &= numCHRBanks * 2 - 1;
				MapCharacterBanks(0, (regValue & 0xFE) * 4, 8);
			}
		}
	} else if (addr < 0xE000) {
		// CHR Bank 1
		if (numCHRBanks) {
			if (MMC1_CHR_BANK_MODE == 1) {
				// 4 kb mode
				regValue &= numCHRBanks * 2 - 1;
				MapCharacterBanks(4, regValue * 4, 4);
			} else {
				// 8 kb mode, ignored
			}
		}
	} else {
		unsigned int openBusMode = regValue & 0x10;
		if (openBusMode != MMC1_RAM_DISABLE) {
			MMC1_SetRAMBank(openBusMode);
		}

		int selectedPRGBlock = 0;
		if (numPRGBanks >= 0x10) {
			selectedPRGBlock = ((chrBanks[0] / 4) & 0x10) * 2;
		}
		regValue = regValue & (numPRGBanks - 1);
		// PRG Bank
		if (MMC1_PRG_BANK_MODE < 2) {
			// 32 kb mode
			regValue &= 0xFE;
			regValue *= 2;
			regValue |= selectedPRGBlock;
			MMC1_PRG_BANK_1 = regValue;
			MMC1_PRG_BANK_2 = regValue + 2;

			MapProgramBanks(0, 4, regValue);
		} else if (MMC1_PRG_BANK_MODE == 2) {
			// changes bank at 0xC000
			regValue *= 2;
			regValue |= selectedPRGBlock;
			MMC1_PRG_BANK_2 = regValue;
			MapProgramBanks(2, MMC1_PRG_BANK_2, 2);
		} else if (MMC1_PRG_BANK_MODE == 3) {
			// changes bank at 0x8000
			regValue *= 2;
			regValue |= selectedPRGBlock;
			MMC1_PRG_BANK_1 = regValue;
			MapProgramBanks(0, MMC1_PRG_BANK_1, 2);
		}
	}
}

void nes_cart::MMC1_SetRAMBank(int value) {
	MMC1_RAM_DISABLE = value;

	if (!value) {
		mainCPU.setMapKB(0x60, 8, cache[MMC1_RAM_BANK].ptr);
	} else {
		mainCPU.setMapOpenBusKB(0x60, 8);
	}
}

void nes_cart::MMC1_StateLoaded() {
	nesCart.MapProgramBanks(0, MMC1_PRG_BANK_1, 2);
	nesCart.MapProgramBanks(2, MMC1_PRG_BANK_2, 2);
}

void nes_cart::setupMapper1_MMC1() {
	writeSpecial = MMC1_writeSpecial;

	// disambiguate board type
	if (numRAMBanks == 2) {
		// SOROM
		MMC1_BOARD_TYPE = MMC1_SOROM;
	} else if (numRAMBanks == 4) {
		MMC1_BOARD_TYPE = MMC1_SXROM;
	} else if (numPRGBanks == 512 / 16) {
		MMC1_BOARD_TYPE = MMC1_SUROM;
	} else {
		MMC1_BOARD_TYPE = MMC1_S_ROM;
	}

	MMC1_SHIFT_BIT = 0;
	MMC1_SHIFT_VALUE = 0;
	MMC1_PRG_BANK_MODE = 0;
	MMC1_CHR_BANK_MODE = 0;

	cachedBankCount = availableROMBanks;

	// map first 8 KB of CHR memory to ppu chr if not ram
	if (numCHRBanks) {
		MapCharacterBanks(0, 0, 8);
	} else {
		// CHR RAM in use
		cachedBankCount--;

		// chr map uses best bank for chr caching locality:
		nesPPU.chrPages[0] = cache[cachedBankCount].ptr;
		nesPPU.chrPages[1] = cache[cachedBankCount].ptr + 0x1000;
	}

	// RAM bank (first index if applicable) set up at 0x6000
	if (numRAMBanks) {
		int ramBank0 = availableROMBanks;
		MMC1_RAM_BANK = ramBank0;

		// enable by default
		MMC1_SetRAMBank(0);
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);
}
