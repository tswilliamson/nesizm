
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rambo-1 Mapper 64

void nes_cart::Mapper64_Update() {
	// select program and chr banks based on status of bank registers
	if (Mapper64_BANK_SELECT & 0x20) {
		MapCharacterBanks(0, Mapper64_R0, 1);
		MapCharacterBanks(1, Mapper64_R8, 1);
		MapCharacterBanks(2, Mapper64_R1, 1);
		MapCharacterBanks(3, Mapper64_R9, 1);
	} else {
		MapCharacterBanks(0, Mapper64_R0 & 0xFE, 2);
		MapCharacterBanks(2, Mapper64_R1 & 0xFE, 2);
	}
	MapCharacterBanks(4, Mapper64_R2, 1);
	MapCharacterBanks(5, Mapper64_R3, 1);
	MapCharacterBanks(6, Mapper64_R4, 1);
	MapCharacterBanks(7, Mapper64_R5, 1);

	int32 PRGMask = (numPRGBanks * 2) - 1;
	if (Mapper64_BANK_SELECT & 0x40) {
		MapProgramBanks(0, Mapper64_RF & PRGMask, 1);
		MapProgramBanks(1, Mapper64_R7 & PRGMask, 1);
		MapProgramBanks(2, Mapper64_R6 & PRGMask, 1);
	} else {
		MapProgramBanks(0, Mapper64_R6 & PRGMask, 1);
		MapProgramBanks(1, Mapper64_R7 & PRGMask, 1);
		MapProgramBanks(2, Mapper64_RF & PRGMask, 1);
	}

	// chr page swap (A12 select)
	bSwapChrPages = (Mapper64_BANK_SELECT & 0x80) != 0;
	bDirtyChrBanks = true;
}

void nes_cart::Mapper64_StateLoaded() {
	Mapper64_Update();

	if (Mapper64_IRQ_MODE == 1) {
		// set next IRQ breakpoint
		Mapper64_IRQ_CLOCKS = mainCPU.clocks + Mapper64_IRQ_COUNT * 4;
		if (Mapper64_IRQ_ENABLE) {
			mainCPU.irqClocks = Mapper64_IRQ_CLOCKS;
		}
		nesCart.scanlineClock = nullptr;
	} else {
		Mapper64_IRQ_COUNT = 0;
		Mapper64_IRQ_CLOCKS = 0;
		nesCart.scanlineClock = nes_cart::Mapper64_ScanlineClock;
	}
}

void Mapper64_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			}
		} else if (address < 0xA000) {
			if (address & 1) {
				// set bank register
				switch (Mapper64_BANK_SELECT & 0xF) {
					case 0: Mapper64_R0 = value; break;
					case 1: Mapper64_R1 = value; break;
					case 2: Mapper64_R2 = value; break;
					case 3: Mapper64_R3 = value; break;
					case 4: Mapper64_R4 = value; break;
					case 5: Mapper64_R5 = value; break;
					case 6: Mapper64_R6 = value; break;
					case 7: Mapper64_R7 = value; break;
					case 8: Mapper64_R8 = value; break;
					case 9: Mapper64_R9 = value; break;
					case 15: Mapper64_RF = value; break;
				}
			} else {
				// set bank select
				Mapper64_BANK_SELECT = value;
			}

			nesCart.Mapper64_Update();
		} else if (address < 0xC000) {
			if (address & 1) {
				// unused
			} else {
				nesPPU.setMirrorType((value & 1) ? nes_mirror_type::MT_HORIZONTAL : nes_mirror_type::MT_VERTICAL);
			}
		} else if (address < 0xE000) {
			if (address & 1) {
				// IRQ reload
				Mapper64_IRQ_MODE = value & 1;
				if (Mapper64_IRQ_MODE == 1) {
					// set next IRQ breakpoint
					Mapper64_IRQ_CLOCKS = mainCPU.clocks + Mapper64_IRQ_COUNT * 4;
					if (Mapper64_IRQ_ENABLE) {
						mainCPU.irqClocks = Mapper64_IRQ_CLOCKS;
					}
					nesCart.scanlineClock = nullptr;
				} else {
					Mapper64_IRQ_COUNT = 0;
					Mapper64_IRQ_CLOCKS = 0;
					nesCart.scanlineClock = nes_cart::Mapper64_ScanlineClock;
				}
			} else {
				// IRQ latch
				Mapper64_IRQ_LATCH = value;
			}
		} else {
			if (address & 1) {
				// irq enable
				Mapper64_IRQ_ENABLE = 1;
				if (mainCPU.clocks < Mapper64_IRQ_CLOCKS) {
					mainCPU.irqClocks = Mapper64_IRQ_CLOCKS;
				}
			} else {
				// irq disable
				if (mainCPU.clocks > Mapper64_IRQ_CLOCKS && Mapper64_IRQ_CLOCKS != 0) {
					Mapper64_IRQ_ENABLE = 1;
					cpu6502_IRQ();
					mainCPU.irqClocks = 0;
				}
				Mapper64_IRQ_ENABLE = 0;
			}
		}
	}
}

void nes_cart::setupMapper64_Rambo1() {
	writeSpecial = Mapper64_writeSpecial;

	cachedBankCount = availableROMBanks;

	DebugAssert(numCHRBanks > 0);
	MapCharacterBanks(0, 0, 8);

	// map first 24 KB of PRG mamory to 80-DF by default, and last 8 KB to E0-FF
	MapProgramBanks(0, 0, 3);
	MapProgramBanks(3, numPRGBanks * 2 - 1, 1);

	Mapper64_R6 = 0;
	Mapper64_R7 = 1;
	Mapper64_RF = 2;

	Mapper64_R0 = 0;
	Mapper64_R1 = 2;
	Mapper64_R2 = 4;
	Mapper64_R3 = 5;
	Mapper64_R4 = 6;
	Mapper64_R5 = 7;

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}

void nes_cart::Mapper64_ScanlineClock() {
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
			if (Mapper64_IRQ_COUNT == 0) {
				// reload counter value
				Mapper64_IRQ_COUNT = Mapper64_IRQ_LATCH;
			} else {
				Mapper64_IRQ_COUNT--;

				if (Mapper64_IRQ_COUNT == 0) {
					uint32 TargetClocks = mainCPU.ppuClocks - (341 / 3) + flipCycles;
					if (Mapper64_IRQ_ENABLE) {
						// trigger an IRQ
						mainCPU.irqClocks = TargetClocks;
						Mapper64_IRQ_CLOCKS = 0;
					} else {
						Mapper64_IRQ_CLOCKS = TargetClocks;
					}
				}
			}
		}
	}
}
