
// Implementation of NES ROM files and mappers (following the popular iNES format)

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_cart nesCart;

nes_cart::nes_cart() : writeSpecial(NULL) {
	handle = 0;
	file[0] = 0;
}

void nes_cart::allocateBanks(unsigned char* withAlloced) {
	for (int i = 0; i < NUM_CACHED_ROM_BANKS; i++) {
		banks[i] = withAlloced + 8192 * i;
	}
}

bool nes_cart::loadROM(const char* withFile) {
	DebugAssert(handle <= 0);

	printf("Loading %s...", withFile);

	unsigned short romName[256];
	Bfile_StrToName_ncpy(romName, withFile, 255);

	// try to load file
	int file = Bfile_OpenFile_OS(romName, READ, 0);
	if (file < 0) {
		printf("Could not open file!");
		return false;
	}

	// ensure there is enough room for header and read it in
	unsigned char header[16];
	if (Bfile_GetFileSize_OS(file) < 16 || Bfile_ReadFile_OS(file, header, 16, 0) != 16) {
		printf("Could not read header in file");
		Bfile_CloseFile_OS(file);
		return false;
	}
	
	int expectedSize = 16;	// starting with header size

	// check 4 bytes for 'N' 'E' 'S' $1A
	const unsigned char type[4] = { 0x4E, 0x45, 0x53, 0x1A };
	if (memcmp(header, type, 4)) {
		printf("Not an iNES ROM file format.");
		Bfile_CloseFile_OS(file);
		return false;
	}

	// byte 4: num PRG ROM banks in 16 KB units
	numPRGBanks = header[4];
	expectedSize += numPRGBanks * 16 * 1024;

	// byte 5: num CHR ROM banks in 8 KB units
	numCHRBanks = header[5];
	expectedSize += numCHRBanks * 8 * 1024;

	printf("%d PRG banks, %d CHR banks", numPRGBanks, numCHRBanks);

	// byte 6: flags (and lower mapper nibble which is used later)
	if (header[6] & (1 << 3)) {
		ppu_setMirrorType(nes_mirror_type::MT_4PANE);
	} else if (header[6] & (1 << 0)) {
		ppu_setMirrorType(nes_mirror_type::MT_VERTICAL);
	} else {
		ppu_setMirrorType(nes_mirror_type::MT_HORIZONTAL);
	}
	isBatteryBacked = header[6] & (1 << 1);

	bool hasTrainer = (header[6] & (1 << 2)) != 0;
	if (hasTrainer) {
		printf("ROM has trainer (not supported).");
		Bfile_CloseFile_OS(file);
		return false;
	}

	bool isNES2 = (header[7] & 0x0C) == 0x08;
	if (isNES2) {
		printf("NES 2.0 ROM (not yet supported).");
		Bfile_CloseFile_OS(file);
		return false;
	}

	// Check for legacy ROM File with 0 filled bytes 12 - 15
	if (header[12] == 0 && header[13] == 0 && header[14] == 0 && header[15] == 0) {
		// byte 7: play choice, vs unisystem (none are supported)
		if (header[7] & (1 << 0)) {
			printf("VS Unisystem ROM (not supported).");
			Bfile_CloseFile_OS(file);
			return false;
		}
		if (header[7] & (1 << 1)) {
			printf("PlayChoice-10 ROM (not supported).");
			Bfile_CloseFile_OS(file);
			return false;
		}

		// byte 8: num RAM banks in 8 KB allotments
		numRAMBanks = header[8];
		if (numRAMBanks == 0)
			numRAMBanks = 1;
		if (numRAMBanks > 1) {
			printf("More than 1 RAM bank (%d) unsupported", numRAMBanks);
			Bfile_CloseFile_OS(file);
			return false;
		}

		// byte 9: 0 bit is PAL / NTSC
		isPAL = header[9] & 1;

		mapper = (header[7] & 0xF0) | (header[6] >> 4);
	} else {
		isPAL = 0;
		numRAMBanks = 1;
		mapper = (header[6] >> 4);
	}

	printf("%s, %d RAM banks", isPAL ? "PAL" : "NTSC", numRAMBanks);

	// expected size?
	if (Bfile_GetFileSize_OS(file) != expectedSize) {
		printf("Not expected file size based on format, will attempt to pad!");
	}

	// default memory mapping first
	mainCPU.mapDefaults();

	// mapper logic
	handle = file;
	if (setupMapper()) {
		printf("Mapper %d : supported", mapper);
		return true;
	} else {
		handle = 0;
		printf("Mapper %d : unsupported", mapper);
		Bfile_CloseFile_OS(file);
		return false;
	}
}

void nes_cart::unload() {
	if (handle) {
		Bfile_CloseFile_OS(handle);
		handle = 0;
	}
}

bool nes_cart::setupMapper() {
	clearCacheData();

	switch (mapper) {
		case 0:
			setupMapper0_NROM();
			return true;
		case 1:
			setupMapper1_MMC1();
			return true;
		default:
			return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Caching support

int bankIndex[NUM_CACHED_ROM_BANKS];

int cachedPRGCount;				// number of 8 KB banks to use for PRG
int cachedCHRCount;				// number of 8 KB banks to use for CHR

void nes_cart::clearCacheData() {
	requestIndex = 0;
	
	for (int i = 0; i < NUM_CACHED_ROM_BANKS; i++) {
		bankIndex[i] = -1;
		bankRequest[i] = -1;
	}

	cachedPRGCount = 0;
	cachedCHRCount = 0;
}

// returns whether the bank given is in use by the memory map
bool nes_cart::isBankUsed(int index) {
	unsigned char* startRange = banks[index];
	unsigned char* endRange = startRange + 1024 * 8;

	for (int i = 0; i < 256; i++) {
		if (mainCPU.map[i] >= startRange && mainCPU.map[i] < endRange) {
			return true;
		}
	}

	return false;
}

// finds least recently requested bank that is currently unused
int nes_cart::findOldestUnusedBank(int startIndex, int lastIndexExclusive) {
	int bestBank = -1;
	for (int i = startIndex; i < lastIndexExclusive; i++) {
		if (bestBank == -1 || bankRequest[i] < bankRequest[bestBank]) {
			if (isBankUsed(i) == false) {
				bestBank = i;
			} else {
				// mark as "requested" since it obviously is still in use
				bankRequest[i] = requestIndex;
			}
		}
	}

	// if we have enough caching set up... this shouldn't happen
	DebugAssert(bestBank != -1);
	return bestBank;
}

// caches an 8 KB PRG bank (so index up to 2 * numPRGBanks), returns result bank memory pointer
unsigned char* nes_cart::cachePRGBank(int index) {
	requestIndex++;

	// find bank index within range:
	for (int i = 0; i < cachedPRGCount; i++) {
		if (bankIndex[i] == index) {
			bankRequest[i] = requestIndex;
			return banks[i];
		}
	}

	// replace smallest request with inactive memory
	int replaceIndex = findOldestUnusedBank(0, cachedPRGCount);
	bankIndex[replaceIndex] = index;
	bankRequest[replaceIndex] = requestIndex;
	Bfile_ReadFile_OS(handle, banks[replaceIndex], 8192, 16 + 8192 * index);
	return banks[replaceIndex];
}

// caches an 8 KB CHR bank, returns result bank memory pointer
unsigned char* nes_cart::cacheCHRBank(int index) {
	requestIndex++;

	// find bank index within range:
	int replaceIndex = cachedPRGCount;
	for (int i = 0; i < cachedCHRCount; i++) {
		int chrIndex = i + cachedPRGCount;
		if (bankIndex[chrIndex] == index) {
			bankRequest[chrIndex] = requestIndex;
			return banks[chrIndex];
		} else if (bankRequest[chrIndex] < bankRequest[replaceIndex]) {
			replaceIndex = chrIndex;
		}
	}

	// replace smallest request entirely (no inactive memory for CHR ROM since they are copied to PPU mem directly)
	bankIndex[replaceIndex] = index;
	bankRequest[replaceIndex] = requestIndex;
	Bfile_ReadFile_OS(handle, banks[replaceIndex], 8192, 16 + 16384 * numPRGBanks + 8192 * index);
	return banks[replaceIndex];
}


// inline mapping helpers
inline void mapCPU(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
	// 256 byte increments
	numKB *= 4;

	for (int i = 0; i < numKB; i++) {
		mainCPU.map[i + startAddrHigh] = &ptr[i * 0x100];
	}
}

inline void mapPPU(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
	// just copy to CHR ROM
	DebugAssert(startAddrHigh * 0x100 + numKB * 1024 <= 0x2000);	
	memcpy(ppu_chrMap + startAddrHigh * 0x100, ptr, numKB * 1024);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NROM

void NROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000 && nesCart.numRAMBanks) {
			// RAM
			mainCPU.map[address >> 8][address & 0xFF] = value;
		} else {
			// does nothing!
		}
	}
}

void nes_cart::setupMapper0_NROM() {
	writeSpecial = NROM_writeSpecial;

	// read ROM banks (up to 2)
	for (int i = 0; i < 2 * numPRGBanks; i++) {
		Bfile_ReadFile_OS(handle, banks[i], 8192, -1);
	}

	// read CHR bank (always one ROM) directly into PPU chrMap
	DebugAssert(numCHRBanks == 1);
	int chrBank = 2 * numPRGBanks;
	Bfile_ReadFile_OS(handle, banks[chrBank], 8192, -1);
	ppu_chrMap = banks[chrBank];

	// map memory to read-in ROM
	if (numPRGBanks == 1) {
		mapCPU(0x80, 8, banks[0]);
		mapCPU(0xA0, 8, banks[1]);
		mapCPU(0xC0, 8, banks[0]);
		mapCPU(0xE0, 8, banks[1]);
	} else {
		DebugAssert(numPRGBanks == 2);

		mapCPU(0x80, 8, banks[0]);
		mapCPU(0xA0, 8, banks[1]);
		mapCPU(0xC0, 8, banks[2]);
		mapCPU(0xE0, 8, banks[3]);
	}

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[4]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC1

// register 0 holds board disambiguation
// 0 : S*ROM (most common formats with same interface. This includes SNROM whose RAM enable bit won't be implemented)
// 1 : SOROM (extra RAM bank, bit 3 of chr bank select selects RAM bank)
// 2 : SUROM (512 KB PRG ROM, high chr bank bit selects bank)
// 3 : SXROM (32 KB RAM, bits 3-4 of chr bank select selects RAM bank high bits)
#define S_ROM 0
#define SOROM 1
#define SUROM 2
#define SXROM 3

// register 1 holds the shift register bit
// register 2 holds the shift register value

// register 3 holds PRG bank mode
// register 4 holds CHR bank mode

void MMC1_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000 && nesCart.numRAMBanks) {
			// RAM
			mainCPU.map[address >> 8][address & 0xFF] = value;
		} else {
			if (value & 0x80) {
				// clear shift register
				nesCart.registers[1] = 0;
				nesCart.registers[2] = 0;
			} else {
				// set shift register bit
				nesCart.registers[2] |= ((value & 1) << nesCart.registers[1]);
				nesCart.registers[1]++;

				if (nesCart.registers[1] == 5) {
					// shift register ready to go! send result and clear for another round
					nesCart.MMC1_Write(address, nesCart.registers[2]);
					nesCart.registers[1] = 0;
					nesCart.registers[2] = 0;
				}

			}
		}
	}
}

void nes_cart::MMC1_Write(unsigned int addr, int regValue) {
	if (addr < 0xA000) {
		// Control
		// mirroring in bottom 2 bits
		switch (regValue & 3) {
			case 0:
				// one screen lower bank
				DebugAssert(false);	// unimplemented
				break;
			case 1:
				// one screen upper bank
				DebugAssert(false);	// unimplemented
				break;
			case 2:
				// vertical
				ppu_setMirrorType(nes_mirror_type::MT_VERTICAL);
				break;
			case 3:
				// horizontal
				ppu_setMirrorType(nes_mirror_type::MT_HORIZONTAL);
				break;
		}

		// bank modes
		int newPRGBankMode = (regValue & 0x0C) >> 2;
		if (registers[3] != newPRGBankMode) {
			registers[3] = newPRGBankMode;

			if (newPRGBankMode == 2) {
				// fix lower
				unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
				mapCPU(0x80, 8, firstBank[0]);
				mapCPU(0xA0, 8, firstBank[1]);
			}
			if (newPRGBankMode == 3) {
				// fix upper
				unsigned char* lastBank[2] = { cachePRGBank(numPRGBanks * 2 - 2), cachePRGBank(numPRGBanks * 2 - 1) };
				mapCPU(0xC0, 8, lastBank[0]);
				mapCPU(0xE0, 8, lastBank[1]);
			}
		}

		registers[4] = (regValue & 0x10) >> 4;
	} else if (addr < 0xC000) {
		// CHR Bank 0
		if (registers[4] == 1) {
			// 4 kb mode
			regValue &= numCHRBanks * 2 - 1;
			unsigned char* chrMem = cacheCHRBank(regValue >> 1);
			mapPPU(0x00, 4, chrMem + 4096 * (regValue & 1));
		} else {
			// 8 kb mode
			regValue &= numCHRBanks - 1;
			unsigned char* chrMem = cacheCHRBank(regValue);
			mapPPU(0x00, 8, chrMem);
		}
	} else if (addr < 0xE000) {
		// CHR Bank 1
		if (registers[4] == 1) {
			// 4 kb mode
			regValue &= numCHRBanks * 2 - 1;
			unsigned char* chrMem = cacheCHRBank(regValue >> 1);
			mapPPU(0x10, 4, chrMem + 4096 * (regValue & 1));
		} else {
			// 8 kb mode, ignored
		}
	} else {
		// PRG Bank
		if (registers[3] < 2) {
			// 32 kb mode
			regValue &= 0xFE;
			regValue *= 2;
			unsigned char* prgMem0 = cachePRGBank(regValue + 0);
			mapCPU(0x80, 8, prgMem0);
			unsigned char* prgMem1 = cachePRGBank(regValue + 1);
			mapCPU(0xA0, 8, prgMem1);
			unsigned char* prgMem2 = cachePRGBank(regValue + 2);
			mapCPU(0xC0, 8, prgMem2);
			unsigned char* prgMem3 = cachePRGBank(regValue + 3);
			mapCPU(0xE0, 8, prgMem3);
		} else if (registers[3] == 2) {
			// changes bank at 0xC000
			regValue *= 2;
			unsigned char* prgMem0 = cachePRGBank(regValue + 0);
			mapCPU(0xC0, 8, prgMem0);
			unsigned char* prgMem1 = cachePRGBank(regValue + 1);
			mapCPU(0xE0, 8, prgMem1);
		} else if (registers[3] == 3) {
			// changes bank at 0x8000
			regValue *= 2;
			unsigned char* prgMem0 = cachePRGBank(regValue + 0);
			mapCPU(0x80, 8, prgMem0);
			unsigned char* prgMem1 = cachePRGBank(regValue + 1);
			mapCPU(0xA0, 8, prgMem1);
		}
	}
}

void nes_cart::setupMapper1_MMC1() {
	writeSpecial = MMC1_writeSpecial;

	// disambiguate board type
	if (numRAMBanks == 2) {
		// SOROM
		registers[0] = SOROM;
	} else if (numRAMBanks == 4) {
		registers[0] = SXROM;
	} else if (numPRGBanks == 512 / 16) {
		registers[0] = SUROM;
	} else {
		registers[0] = S_ROM;
	}

	registers[1] = 0;
	registers[2] = 0;
	registers[3] = 0;
	registers[4] = 0;

	// determine bank caching allocations proportional to PRG and CHR ROM usage
	// applies minimums of a third the amount of space or the total actual ROM amount, whichever is smaller
	int cachedCount = NUM_CACHED_ROM_BANKS - numRAMBanks - 1;
	int minPRGCount = cachedCount / 3;
	if (numPRGBanks * 2 < minPRGCount) minPRGCount = numPRGBanks * 2;
	int minCHRCount = cachedCount / 3;
	if (numCHRBanks < minCHRCount) minCHRCount = numCHRBanks;

	cachedPRGCount = cachedCount * (numPRGBanks * 2) / (numPRGBanks * 2 + numCHRBanks);
	if (cachedPRGCount < minPRGCount)
		cachedPRGCount = minPRGCount;

	cachedCHRCount = cachedCount - cachedPRGCount;
	if (cachedCHRCount < minCHRCount) {
		int diff = minCHRCount - cachedCHRCount;
		cachedPRGCount -= diff;
		cachedCHRCount += diff;
	}

	// chr map uses best bank for chr caching locality:
	ppu_chrMap = banks[cachedCount];

	// RAM bank (first index if applicable) set up at 0x6000
	if (numRAMBanks) {
		int ramBank0 = cachedCount + 1;
		mapCPU(0x60, 8, banks[ramBank0]);
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
	mapCPU(0x80, 8, firstBank[0]);
	mapCPU(0xA0, 8, firstBank[1]);
	unsigned char* lastBank[2] = { cachePRGBank(numPRGBanks * 2 - 2), cachePRGBank(numPRGBanks * 2 - 1) };
	mapCPU(0xC0, 8, lastBank[0]);
	mapCPU(0xE0, 8, lastBank[1]);

	// map first 8 KB of CHR memory to ppu chr if not ram
	if (numCHRBanks) {
		mapPPU(0x00, 8, cacheCHRBank(0));
	}
}