
#include "mappers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sunsoft 4 Mapper 68

void nes_cart::Mapper68_Update(bool bMapNametables) {
	// eack 2 kb chr bank
	MapCharacterBanks(0, Mapper68_CHR0 * 2, 2);
	MapCharacterBanks(2, Mapper68_CHR1 * 2, 2);
	MapCharacterBanks(4, Mapper68_CHR2 * 2, 2);
	MapCharacterBanks(6, Mapper68_CHR3 * 2, 2);
	bDirtyChrBanks = true;

	int32 PRGMask = (numPRGBanks - 1) & 0xF;
	MapProgramBanks(0, (Mapper68_PRG & PRGMask) * 2, 2);

	if (bMapNametables && (Mapper68_NTM & 0x10)) {
		// map a cached bank for nametable ROM into nametable memory
		unsigned char* table0 = cacheSingleCHRBank(Mapper68_NT0 / 8) + 1024 * (Mapper68_NT0 & 7);
		unsigned char* table1 = cacheSingleCHRBank(Mapper68_NT1 / 8) + 1024 * (Mapper68_NT1 & 7);
		memcpy_fast32(nesPPU.nameTables, table0, 1024);
		memcpy_fast32(nesPPU.nameTables+1, table1, 1024);
	}
}

void nes_cart::Mapper68_StateLoaded() {
	Mapper68_Update(true);
}

void Mapper68_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if ((Mapper68_PRG & 0x10) && nesCart.numRAMBanks) {
				// RAM
				mainCPU.writeDirect(address, value);
			}
		} else {
			address = address & 0xF000;
			bool bMapNametables = false;

			if (address == 0x8000) {
				Mapper68_CHR0 = value;
			} else if (address == 0x9000) {
				Mapper68_CHR1 = value;
			} else if (address == 0xA000) {
				Mapper68_CHR2 = value;
			} else if (address == 0xB000) {
				Mapper68_CHR3 = value;
			} else if (address == 0xC000) {
				Mapper68_NT0 = value | 0x80;
				if (Mapper68_NTM & 0x10)
					bMapNametables = true;
			} else if (address == 0xD000) {
				Mapper68_NT1 = value | 0x80;
				if (Mapper68_NTM & 0x10)
					bMapNametables = true;
			} else if (address == 0xE000) {
				unsigned int oldValue = Mapper68_NTM;
				Mapper68_NTM = value & 0x13;
				if ((oldValue & 0x10) != (value & 0x10)) {
					unsigned char* cacheArea = nesCart.cache[nesCart.cachedBankCount].ptr;
					if (value & 0x10) {
						// using ROM for nametables, copy existing ones into our cached area
						memcpy_fast32(cacheArea, nesPPU.nameTables, 2048);
						bMapNametables = true;
					} else {
						// returning nametables to RAM, uncache from our area
						memcpy_fast32(nesPPU.nameTables, cacheArea, 2048);
					}
				}
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
			} else if (address == 0xF000) {
				unsigned int oldValue = Mapper68_PRG;
				Mapper68_PRG = value & 0x1F;
				if ((oldValue & 0x10) != (value & 0x10)) {
					// changed WRAM mode
					if ((value & 0x10) && nesCart.numRAMBanks) {
						mainCPU.setMapKB(0x60, 8, nesCart.cache[nesCart.availableROMBanks].ptr);
					} else {
						mainCPU.setMapOpenBusKB(0x60, 8);
					}
				}
			}

			nesCart.Mapper68_Update(bMapNametables);
		}
	}
}

void nes_cart::setupMapper68_Sunsoft4() {
	writeSpecial = Mapper68_writeSpecial;

	// will use cache[cachedBankCount] as nametable RAM swap
	cachedBankCount = availableROMBanks - 1;

	DebugAssert(numCHRBanks > 0);
	MapCharacterBanks(0, 0, 8);
	Mapper68_CHR1 = 1;
	Mapper68_CHR2 = 2;
	Mapper68_CHR3 = 3;

	// map firat 16 kb to start by default, last 16 kb is not bank switchable
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mainCPU.setMapKB(0x60, 8, cache[availableROMBanks].ptr);
	}
}
