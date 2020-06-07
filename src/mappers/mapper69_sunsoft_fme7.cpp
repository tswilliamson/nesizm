
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sunsoft FME-7 Mapper 69

void nes_cart::Mapper69_RunCommand(bool bIsForceUpdate) {
	if (!bIsForceUpdate) {
		nesCart.registers[Mapper69_COMMAND] = Mapper69_PARAM;
	}

	if (Mapper69_COMMAND < 8 || bIsForceUpdate) {
		MapCharacterBanks(0, Mapper69_CHAR0, 1);
		MapCharacterBanks(1, Mapper69_CHAR1, 1);
		MapCharacterBanks(2, Mapper69_CHAR2, 1);
		MapCharacterBanks(3, Mapper69_CHAR3, 1);
		MapCharacterBanks(4, Mapper69_CHAR4, 1);
		MapCharacterBanks(5, Mapper69_CHAR5, 1);
		MapCharacterBanks(6, Mapper69_CHAR6, 1);
		MapCharacterBanks(7, Mapper69_CHAR7, 1);

		bDirtyChrBanks = true;
		if (!bIsForceUpdate) return;
	} 
	
	if (Mapper69_COMMAND < 0xC || bIsForceUpdate) {
		int32 PRGMask = ((numPRGBanks * 2) - 1) & 0x3F;
		MapProgramBanks(0, Mapper69_PRG1 & PRGMask, 1);
		MapProgramBanks(1, Mapper69_PRG2 & PRGMask, 1);
		MapProgramBanks(2, Mapper69_PRG3 & PRGMask, 1);

		if (Mapper69_COMMAND == 0x8 || bIsForceUpdate) {
			if (Mapper69_PRG0 & 0x40) {
				isLowPRGROM = 0;
				// map RAM to 0x6000 if setup
				if (numRAMBanks == 1) {
					mapCPU(0x60, 8, cache[availableROMBanks].ptr);
				}
			} else {
				// map program bank to 0x6000
				isLowPRGROM = 1;
				MapProgramBanks(4, Mapper69_PRG0 & PRGMask, 1);
			}
		}

		if (!bIsForceUpdate) return;
	}

	if (Mapper69_COMMAND == 0xC || bIsForceUpdate) {
		switch (Mapper69_NT & 0x3) {
			case 0:
				nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
				break;
			case 1:
				nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
				break;
			case 2:
				nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE);
				break;
			case 3:
				nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE_UPPER);
				break;
		}

		if (!bIsForceUpdate) return;
	}

	if (Mapper69_COMMAND == 0xD || bIsForceUpdate) {
		Mapper69_IRQCONTROL = Mapper69_IRQCONTROL & 0x81;

		if (Mapper69_IRQ > 0 && Mapper69_IRQ < mainCPU.clocks) {
			Mapper69_IRQ = 0;
		}

		unsigned int countClks = Mapper69_LASTCOUNTERCLK + Mapper69_LASTCOUNTER;
		if (Mapper69_IRQCONTROL & 0x80) {
			// keep the counter up to date
			while (countClks < mainCPU.clocks) {
				countClks += 0x10000;
				Mapper69_LASTCOUNTERCLK += 0x10000;
			}
		}

		if (Mapper69_IRQCONTROL == 0x81) {
			// IRQ is enabled
			Mapper69_IRQ = countClks;
			mainCPU.setIRQ(0, countClks);

		} else {
			Mapper69_IRQ = 0;
		}
	}

	if (Mapper69_COMMAND > 0xD) {
		unsigned int actualCounter;
		if (Mapper69_IRQCONTROL & 0x80) {
			actualCounter = (Mapper69_LASTCOUNTERCLK + Mapper69_LASTCOUNTER - mainCPU.clocks) & 0xFFFF;
		} else {
			actualCounter = Mapper69_LASTCOUNTER;
		}

		if (!bIsForceUpdate) {
			if (Mapper69_COMMAND == 0x0F) {
				// high byte
				actualCounter = (actualCounter & 0xFF) | (Mapper69_PARAM << 8);
			} else {
				// low byte
				actualCounter = (actualCounter & 0xFF00) | (Mapper69_PARAM);
			}
		} 

		Mapper69_LASTCOUNTER = actualCounter;
		Mapper69_LASTCOUNTERCLK = mainCPU.clocks;
	}
}


void Mapper69_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			// RAM write only in RAM mode if RAM enabled
			if ((Mapper69_PRG0 & 0xC0) == 0xC0) {
				if (nesCart.numRAMBanks) {
					// RAM
					mainCPU.map[address >> 8][address & 0xFF] = value;
				}
			}
		} else if (address < 0xA000) {
			Mapper69_COMMAND = value & 0xF;
		} else if (address < 0xC000) {
			Mapper69_PARAM = value;
			nesCart.Mapper69_RunCommand(false);
		} 
	}
}

void nes_cart::setupMapper69_Sunsoft() {
	writeSpecial = Mapper69_writeSpecial;

	cachedBankCount = availableROMBanks;

	DebugAssert(numCHRBanks > 0);
	MapCharacterBanks(0, 0, 8);

	// map first 24 KB of PRG mamory to 80-DF by default, and last 8 KB to E0-FF
	MapProgramBanks(0, 0, 3);
	MapProgramBanks(3, numPRGBanks * 2 - 1, 1);

	Mapper69_CHAR0 = 0;
	Mapper69_CHAR1 = 1;
	Mapper69_CHAR2 = 2;
	Mapper69_CHAR3 = 3;
	Mapper69_CHAR4 = 4;
	Mapper69_CHAR5 = 5;
	Mapper69_CHAR6 = 6;
	Mapper69_CHAR7 = 7;
	Mapper69_PRG0 = 0x40;
	Mapper69_PRG1 = 0;
	Mapper69_PRG2 = 1;
	Mapper69_PRG3 = 2;

	Mapper69_RunCommand(true);
}
