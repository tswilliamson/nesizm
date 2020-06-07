
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC3 (most popular mapper with IRQ)

void MMC3_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			} else {
				// open bus
			}
		} else if (address < 0xA000) {
			if (!(address & 1)) {
				// bank select
				if (MMC3_BANK_SELECT != value) {
					// upper two bits changing effects the entire current memory mapping
					int upperBits = (MMC3_BANK_SELECT ^ value) & 0xC0;
					MMC3_BANK_SELECT = value;
					if (upperBits) {
						if (upperBits & 0x40) {
							// PRG bank mode change
							nesCart.MMC3_UpdateMapping(-1);
						}
						if (upperBits & 0x80) {
							// CHR mode change
							if (MMC3_BANK_SELECT & 0x80) {
								nesCart.bSwapChrPages = true;
							} else {
								nesCart.bSwapChrPages = false;
							}

							nesCart.bDirtyChrBanks = true;
						}
					}
				}
			} else {
				// bank data
				int targetReg = MMC3_BANK_SELECT & 7;
				if (MMC3_BANK_REG[targetReg] != value) {
					MMC3_BANK_REG[targetReg] = value;
					nesCart.MMC3_UpdateMapping(targetReg);
				}
			}
		}
		else if (address < 0xC000) {
			if (!(address & 1)) {
				// mirroring
				if (nesPPU.mirror != nes_mirror_type::MT_4PANE) {
					if (value & 1) {
						nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
					} else {
						nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
					}
				}
			} else {
				// RAM protect
				// TODO : support reading open bus
				// DebugAssert(value & 0x80);
			}
		}
		else if (address < 0xE000) {
			if (!(address & 1)) {
				// IRQ counter reload value
				MMC3_IRQ_SET = value;

				if (MMC3_IRQ_LASTSET + (nesCart.isPAL ? 85 : 90) > mainCPU.clocks) {
					MMC3_IRQ_COUNTER = value;
				}
			} else {
				// set IRQ counter reload flag
				MMC3_IRQ_RELOAD = 1;
			}
		}
		else {
			if (!(address & 1)) {
				// disable IRQ interrupt
				MMC3_IRQ_ENABLE = 0;

				// disable IRQ latch
				MMC3_IRQ_LATCH = 0;
			} else {
				// enable IRQ interrupt
				MMC3_IRQ_ENABLE = 1;
			}
		}
	}
}

void nes_cart::MMC3_UpdateMapping(int regNumber) {
	switch (regNumber) {
		case 0:
		{
			// 2 KB bank at $00 and $10
			MapCharacterBanks(0, MMC3_BANK_REG[0] & 0xFFFE, 2);
			break;
		}
		case 1:
		{
			// 2 KB bank at $08 and $18
			MapCharacterBanks(2, MMC3_BANK_REG[1] & 0xFFFE, 2);
			break;
		}
		case 2:
		{
			// 1 KB bank at $10 and $00
			MapCharacterBanks(4, MMC3_BANK_REG[2], 1);
			break;
		}
		case 3:
		{
			// 1 KB bank at $14 and $04
			MapCharacterBanks(5, MMC3_BANK_REG[3], 1);
			break;
		}
		case 4:
		{
			// 1 KB bank at $18 and $08
			MapCharacterBanks(6, MMC3_BANK_REG[4], 1);
			break;
		}
		case 5:
		{
			// 1 KB bank at $1C and $0C
			MapCharacterBanks(7, MMC3_BANK_REG[5], 1);
			break;
		}
		case 6:
		{
			// select bank at 80 (or C0)
			int prgBank = MMC3_BANK_REG[6] & (numPRGBanks * 2 - 1);
			if (MMC3_BANK_SELECT & 0x40) {
				MapProgramBanks(2, prgBank, 1);
			} else {
				MapProgramBanks(0, prgBank, 1);
			}
			break;
		}
		case 7:
		{
			// select bank at A0
			int prgBank = MMC3_BANK_REG[7] & (numPRGBanks * 2 - 1);
			MapProgramBanks(1, prgBank, 1);
			break;
		}
	}

	if (regNumber == -1) {
		for (int i = 0; i < 8; i++) {
			MMC3_UpdateMapping(i);
		}

		// possible bank mode change
		if (MMC3_BANK_SELECT & 0x40) {
			// 2nd to last bank set to 80-9F
			MapProgramBanks(0, numPRGBanks * 2 - 2, 1);
		} else {
			// 2nd to last bank set to C0-DF
			MapProgramBanks(2, numPRGBanks * 2 - 2, 1);
		}
	}
}

void nes_cart::MMC3_StateLoaded() {
	MMC3_UpdateMapping(-1);

	if (MMC3_BANK_SELECT & 0x80) {
		nesCart.bSwapChrPages = true;
	} else {
		nesCart.bSwapChrPages = false;
	}

	nesCart.bDirtyChrBanks = true;
}

void nes_cart::MMC3_ScanlineClock() {
	TIME_SCOPE();

	int flipCycles = -1;
	int irqDec = 1;
	
	const int OAM_LOOKUP_CYCLE = 82; // 82 is derived from: PPU clock 260 / 3 - half the largest instruction size, appears to get us compatible

	if (nesPPU.PPUCTRL & PPUCTRL_SPRSIZE) {
		// we need to build the fetch mask potentially
		if (nesPPU.dirtyOAM) {
			nesPPU.resolveOAMExternal();
		}

		// 8x16 sprites are a special case
		flipCycles = OAM_LOOKUP_CYCLE;

		// check which sprites are on this scanline to determine irq decrement amount
		irqDec = 0;
		unsigned char* curObj = &nesPPU.oam[252];
		int lastPatternTable = 0;
		int numSprites = 0;
		int scanlineOffset = nesPPU.scanline - 2;

		extern uint8 fetchMask[272];
		for (int b = fetchMask[nesPPU.scanline]; b; b = b >> 1) {
			if (!(b & 1)) {
				curObj -= 32;
				continue;
			}

			for (int i = 0; i < 8; i++, curObj -= 4) {
				unsigned int yCoord = scanlineOffset - curObj[0];
				if (yCoord < 16) {
					numSprites++;
					int patternTable = (curObj[1] & 1);
					if (patternTable > lastPatternTable) {
						irqDec++;
					}
					lastPatternTable = patternTable;

					if (numSprites == 8) {
						break;
					}
				}
			}
		}
		if (numSprites < 8 && lastPatternTable == 0) {
			irqDec++;
		}
	} else if (nesPPU.PPUCTRL & PPUCTRL_OAMTABLE) {
		if (!(nesPPU.PPUCTRL & PPUCTRL_BGDTABLE)) {
			// BG uses 0x0000, OAM uses 0x1000
			flipCycles = OAM_LOOKUP_CYCLE;
		}
	} else {
		if (nesPPU.PPUCTRL & PPUCTRL_BGDTABLE) {
			// BG uses 0x1000, OAM uses 0x0000
			flipCycles = 1;
		} 
	}

	if (flipCycles >= 0 && (nesPPU.PPUMASK & (PPUMASK_SHOWBG | PPUMASK_SHOWOBJ))) {
		// perform counter
		while (irqDec--) {
			if (MMC3_IRQ_RELOAD || MMC3_IRQ_COUNTER == 0) {
				// reload counter value
				MMC3_IRQ_COUNTER = MMC3_IRQ_SET;

				// if not reloading but set with 0, then latch at the end of this scanline
				if (MMC3_IRQ_RELOAD == 0 && MMC3_IRQ_COUNTER == 0 && MMC3_IRQ_ENABLE) {
					MMC3_IRQ_LATCH = 1;
				}

				MMC3_IRQ_RELOAD = 0;
				MMC3_IRQ_LASTSET = mainCPU.ppuClocks - (341 / 3); // beginning of the scanline
			} else {
				MMC3_IRQ_COUNTER--;

				if (MMC3_IRQ_COUNTER == 0 && MMC3_IRQ_ENABLE) {
					MMC3_IRQ_LATCH = 1;
				}
			}

			if (MMC3_IRQ_LATCH) {
				// trigger an IRQ
				mainCPU.setIRQ(0, mainCPU.ppuClocks - (341 / 3) + flipCycles);
			}
		}
	}
}

void nes_cart::setupMapper4_MMC3() {
	writeSpecial = MMC3_writeSpecial;
	scanlineClock = MMC3_ScanlineClock;

	cachedBankCount = availableROMBanks;

	// map first 8 KB of CHR memory to ppu chr if not ram
	MapCharacterBanks(0, 0, 8);

	// set fixed page up
	MapProgramBanks(3, numPRGBanks * 2 - 1, 1);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}

	// this will set up all non permanent memory
	MMC3_UpdateMapping(-1);
}
