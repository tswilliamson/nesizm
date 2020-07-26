
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sunsoft 3 Mapper 67

void nes_cart::Mapper67_Update() {
	// eack 2 kb chr bank
	MapCharacterBanks(0, Mapper67_CHR0 * 2, 2);
	MapCharacterBanks(2, Mapper67_CHR1 * 2, 2);
	MapCharacterBanks(4, Mapper67_CHR2 * 2, 2);
	MapCharacterBanks(6, Mapper67_CHR3 * 2, 2);
	bDirtyChrBanks = true;

	int32 PRGMask = numPRGBanks - 1;
	MapProgramBanks(0, (Mapper67_PRG & PRGMask) * 2, 2);
	
	// set irq to latest if enabled
	if (Mapper67_IRQ_Enable) {
		mainCPU.setIRQ(0, Mapper67_IRQ_LastSet + Mapper67_IRQ_Counter);
	} else if (mainCPU.clocks < mainCPU.irqClock[0]) {
		// disable future IRQ if applicable
		mainCPU.ackIRQ(0);
	}
}

void nes_cart::Mapper67_StateLoaded() {
	Mapper67_Update();
}

void Mapper67_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.writeDirect(address, value);
			}
		} else {
			if ((address & 0x8800) == 0x8000) {
				mainCPU.ackIRQ(0);
			}

			address = address & 0xF800;

			if (address == 0x8800) {
				Mapper67_CHR0 = value & 0x3F;
			} else if (address == 0x9800) {
				Mapper67_CHR1 = value & 0x3F;
			} else if (address == 0xA800) {
				Mapper67_CHR2 = value & 0x3F;
			} else if (address == 0xB800) {
				Mapper67_CHR3 = value & 0x3F;
			} else if (address == 0xC800) {
				// write to IRQ counter

				// update counter by offset if applicable
				if (Mapper67_IRQ_Enable) {
					unsigned int clocksPassed = mainCPU.clocks - Mapper67_IRQ_LastSet;
					if (clocksPassed < Mapper67_IRQ_Counter) {
						Mapper67_IRQ_Counter -= clocksPassed;
					} else {
						Mapper67_IRQ_Counter = 0;
					}
				}
				Mapper67_IRQ_LastSet = mainCPU.clocks;

				if (Mapper67_IRQ_WriteToggle) {
					Mapper67_IRQ_Counter = (Mapper67_IRQ_Counter & 0xFF00) | value;
				} else {
					Mapper67_IRQ_Counter = (value << 8) | (Mapper67_IRQ_Counter & 0xFF);
				}
				Mapper67_IRQ_WriteToggle ^= 1;
			} else if (address == 0xD800) {
				// enable IRQ
				unsigned int newEnable = (value & 0x10);
				if (newEnable != Mapper67_IRQ_Enable) {
					if (newEnable) {
						Mapper67_IRQ_LastSet = mainCPU.clocks;
					}
					Mapper67_IRQ_Enable = newEnable;
				}

				Mapper67_IRQ_WriteToggle = 0;
			} else if (address == 0xE800) {
				switch (value & 3) {
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
			} else if (address == 0xF800) {
				Mapper67_PRG = value & 0xF;
			}

			nesCart.Mapper67_Update();
		}
	}
}

void nes_cart::setupMapper67_Sunsoft3() {
	writeSpecial = Mapper67_writeSpecial;

	cachedBankCount = availableROMBanks;

	DebugAssert(numCHRBanks > 0);
	MapCharacterBanks(0, 0, 8);
	Mapper67_CHR1 = 1;
	Mapper67_CHR2 = 2;
	Mapper67_CHR3 = 3;

	// map firat 16 kb to start by default, last 16 kb is not bank switchable
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mainCPU.setMapKB(0x60, 8, cache[availableROMBanks].ptr);
	}
}
