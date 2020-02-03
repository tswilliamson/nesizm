
// Implementation of NES ROM files and mappers (following the popular iNES format)

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_cart nesCart;

nes_nametable nes_onboardPPUTables[4];

unsigned char openBus[256] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
};

nes_cart::nes_cart() : writeSpecial(NULL) {
	handle = 0;
	file[0] = 0;
}

void nes_cart::allocateBanks(unsigned char* staticAlloced) {
	allocatedROMBanks = STATIC_CACHED_ROM_BANKS;
	for (int i = 0; i < STATIC_CACHED_ROM_BANKS; i++) {
		banks[i] = staticAlloced + 8192 * i;
	}

	for (int i = STATIC_CACHED_ROM_BANKS; i < MAX_CACHED_ROM_BANKS; i++) {
		unsigned char* heapBank = (unsigned char*) malloc(8192);
		if (heapBank) {
			banks[i] = heapBank;
			allocatedROMBanks = i;
		} else {
			break;
		}
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
		nesPPU.setMirrorType(nes_mirror_type::MT_4PANE);
	} else if (header[6] & (1 << 0)) {
		nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
	} else {
		nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
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
		if (numRAMBanks > 1 && isBatteryBacked) {
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

	// load 8 kb .SAV file if available
	availableROMBanks = allocatedROMBanks - numRAMBanks;
	savFile[0] = 0;
	if (numRAMBanks) {
		for (int i = 0; i < numRAMBanks; i++) {
			memset(&banks[availableROMBanks + i][0], 0, 8192);
		}

		if (isBatteryBacked && numRAMBanks == 1) {
			strcpy(savFile, withFile);
			*strrchr(savFile, '.') = 0;
			strcat(savFile, ".sav");
			Bfile_StrToName_ncpy(romName, savFile, 255);

			// try to load file
			int saveFileHandle = Bfile_OpenFile_OS(romName, READ, 0);
			if (saveFileHandle > 0) {
				Bfile_ReadFile_OS(saveFileHandle, &banks[availableROMBanks][0], 8192, -1);
				Bfile_CloseFile_OS(saveFileHandle);
			}

			savHash = GetRAMHash();
		}
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

uint32 nes_cart::GetRAMHash() {
	uint32 hash = 0x13371337;
	if (numRAMBanks) {
		for (int i = 0; i < 8192; i++) {
			hash = hash * 31 + banks[availableROMBanks][i];
		}
	}
	return hash;
}

void nes_cart::OnPause() {
	if (savFile[0]) {
		uint32 curRAMHash = GetRAMHash();

		if (curRAMHash != savHash) {
			savHash = curRAMHash;

			unsigned short fileName[256];
			Bfile_StrToName_ncpy(fileName, savFile, 255);

			size_t size = 8192;
			Bfile_CreateEntry_OS(fileName, CREATEMODE_FILE, &size);

			int savHandle = Bfile_OpenFile_OS(fileName, WRITE, 0);
			if (savHandle >= 0) {
				Bfile_WriteFile_OS(savHandle, &banks[availableROMBanks][0], 8192);
				Bfile_CloseFile_OS(savHandle);
			}
		}
	}
}

void nes_cart::OnContinue() {
	BuildFileBlocks();
}

bool nes_cart::setupMapper() {
	renderLatch = NULL;
	writeSpecial = NULL;

	// most mirror configs just use the on board ppu nametables:
	nesPPU.nameTables = nes_onboardPPUTables;

	clearCacheData();

	memset(registers, 0, sizeof(registers));

	if (!BuildFileBlocks()) {
		printf("Error setting up blocks");
		return false;
	}

	switch (mapper) {
		case 0:
			setupMapper0_NROM();
			return true;
		case 1:
			setupMapper1_MMC1();
			return true;
		case 2:
			setupMapper2_UNROM();
			return true;
		case 3:
			setupMapper3_CNROM();
			return true;
		case 4:
			setupMapper4_MMC3();
			return true;
		case 7:
			setupMapper7_AOROM();
			return true;
		case 9:
			setupMapper9_MMC2();
			return true;
		case 11:
			setupMapper11_ColorDreams();
			return true;
		case 71:
			setupMapper71_Camerica();
			return true;
		default:
			return false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Caching support

bool nes_cart::BuildFileBlocks() {
	int numBlocks = (Bfile_GetFileSize_OS(handle) + 4095) / 4096; 
	for (int i = 0; i < numBlocks; i++) {
		int ret = Bfile_GetBlockAddress(handle, i * 0x1000, &blocks[i]);
		if (ret < 0)
			// error!
			return false;
	}
	return true;
}

void nes_cart::BlockRead(unsigned char* intoMem, int size, int offset) {
	TIME_SCOPE()

	while (size) {
		const int blockNum = offset >> 12;
		const int offsetInBlock = offset & 0xFFF;

		int toRead = 0x1000 - offsetInBlock;
		if (toRead > size) toRead = size;

		memcpy(intoMem, blocks[blockNum] + offsetInBlock, toRead);
		intoMem += toRead;
		size -= toRead;
		offset += toRead;
	}
}

void nes_cart::clearCacheData() {
	requestIndex = 0;
	
	for (int i = 0; i < availableROMBanks; i++) {
		bankIndex[i] = -1;
		bankRequest[i] = -1;
	}

	cachedBankCount = 0;
}

// returns whether the bank given is in use by the memory map
bool nes_cart::isBankUsed(int index) {
	unsigned char* ptr = banks[index];

	if (ptr == nesPPU.chrMap)
		return true;

	for (int i = 0; i < 8; i++) {
		if (mappedCPUPtrs[i] == ptr) {
			return true;
		}
	}

	return false;
}

// finds least recently requested bank that is currently unused
int nes_cart::findOldestUnusedBank() {
	int bestBank = -1;
	for (int i = 0; i < cachedBankCount; i++) {
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
	DebugAssert(index < numPRGBanks * 2);

	requestIndex++;

	// find bank index within range:
	for (int i = 0; i < cachedBankCount; i++) {
		if (bankIndex[i] == index) {
			bankRequest[i] = requestIndex;
			return banks[i];
		}
	}

	// replace smallest request with inactive memory
	int replaceIndex = findOldestUnusedBank();
	bankIndex[replaceIndex] = index;
	bankRequest[replaceIndex] = requestIndex;
	BlockRead(banks[replaceIndex], 8192, 16 + 8192 * index);
	return banks[replaceIndex];
}

// caches an 8 KB CHR bank, returns result bank memory pointer
unsigned char* nes_cart::cacheCHRBank(int index) {
	index &= (numCHRBanks - 1);

	requestIndex++;

	// find bank index within range:
	int cacheIndex = index + 4096;
	for (int i = 0; i < cachedBankCount; i++) {
		if (bankIndex[i] == cacheIndex) {
			bankRequest[i] = requestIndex;
			return banks[i];
		}
	}

	// replace smallest request entirely (no inactive memory for CHR ROM since they are copied to PPU mem directly)
	int replaceIndex = findOldestUnusedBank();
	bankIndex[replaceIndex] = cacheIndex;
	bankRequest[replaceIndex] = requestIndex;
	BlockRead(banks[replaceIndex], 8192, 16 + 16384 * numPRGBanks + 8192 * index);
	return banks[replaceIndex];
}


// inline mapping helpers
inline void mapCPU(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
	DebugAssert((startAddrHigh & ((1<<5)-1)) == 0);
	nesCart.mappedCPUPtrs[startAddrHigh >> 5] = ptr;

	// 256 byte increments
	numKB *= 4;

	for (unsigned int i = 0; i < numKB; i++) {
		mainCPU.map[i + startAddrHigh] = &ptr[i * 0x100];
	}
}

inline void mapPPU(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
	// just copy to CHR ROM
	DebugAssert(startAddrHigh * 0x100 + numKB * 1024 <= 0x2000);	
	memcpy(nesPPU.chrMap + startAddrHigh * 0x100, ptr, numKB * 1024);
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
	cachedBankCount = availableROMBanks;

	writeSpecial = NROM_writeSpecial;

	// read ROM banks (up to 2)
	unsigned char* prgMem[4];
	for (int i = 0; i < 2 * numPRGBanks; i++) {
		prgMem[i] = cachePRGBank(i);
	}

	// read CHR bank (always one ROM) directly into PPU chrMap
	DebugAssert(numCHRBanks == 1);
	nesPPU.chrMap = cacheCHRBank(0);

	// map memory to read-in ROM
	if (numPRGBanks == 1) {
		mapCPU(0x80, 8, prgMem[0]);
		mapCPU(0xA0, 8, prgMem[1]);
		mapCPU(0xC0, 8, prgMem[0]);
		mapCPU(0xE0, 8, prgMem[1]);
	} else {
		DebugAssert(numPRGBanks == 2);

		mapCPU(0x80, 8, prgMem[0]);
		mapCPU(0xA0, 8, prgMem[1]);
		mapCPU(0xC0, 8, prgMem[2]);
		mapCPU(0xE0, 8, prgMem[3]);
	}

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[availableROMBanks]);
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
// register 5 holds the RAM chip enable bit (0 = enabled)
// register 6 holds the bank containing the ram

// register 7 holds the PRG bank in lower 16 KB
// register 8 holds the PRG bank in higher 16 KB 

void MMC1_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			// if enabled and available
			if (nesCart.numRAMBanks && nesCart.registers[5] == 0) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			} 
		} else {
			if (value & 0x80) {
				// clear shift register
				nesCart.registers[1] = 0;
				nesCart.registers[2] = 0;

				// set PRG bank mode
				if (nesCart.registers[3] != 3) {
					nesCart.MMC1_Write(0x8000, nesCart.registers[3] | 0x0C);
				}
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

	// SUROM 256 select
	if (registers[0] == 2 && addr >= 0xA000 && addr < 0xE000) {
		if (addr < 0xC000 || registers[4] == 1) {
			int selectedPRGBlock = (regValue & 0x10) * 2;
			if ((registers[7] & 0x20) != selectedPRGBlock) {
				registers[7] = (registers[7] & 0x1F) | selectedPRGBlock;
				registers[8] = (registers[8] & 0x1F) | selectedPRGBlock;
				unsigned char* prgMem0 = cachePRGBank(registers[7] + 0);
				mapCPU(0x80, 8, prgMem0);
				unsigned char* prgMem1 = cachePRGBank(registers[7] + 1);
				mapCPU(0xA0, 8, prgMem1);
				unsigned char* prgMem2 = cachePRGBank(registers[8] + 0);
				mapCPU(0xC0, 8, prgMem2);
				unsigned char* prgMem3 = cachePRGBank(registers[8] + 1);
				mapCPU(0xE0, 8, prgMem3);
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
		if (registers[3] != newPRGBankMode) {
			registers[3] = newPRGBankMode;

			if (newPRGBankMode == 2) {
				// fix lower
				unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
				mapCPU(0x80, 8, firstBank[0]);
				mapCPU(0xA0, 8, firstBank[1]);
				registers[7] = 0;
			}
			if (newPRGBankMode == 3) {
				// fix upper
				registers[8] = ((numPRGBanks - 1) & 0xF) * 2 + (registers[7] & 0x20);
				unsigned char* lastBank[2] = { cachePRGBank(registers[8]), cachePRGBank(registers[8] + 1) };
				mapCPU(0xC0, 8, lastBank[0]);
				mapCPU(0xE0, 8, lastBank[1]);
			}
		}

		registers[4] = (regValue & 0x10) >> 4;
	} else if (addr < 0xC000) {
		// CHR Bank 0
		if (numCHRBanks) {
			if (registers[4] == 1) {
				// 4 kb mode
				regValue &= numCHRBanks * 2 - 1;
				unsigned char* chrMem = cacheCHRBank(regValue >> 1);
				mapPPU(0x00, 4, chrMem + 4096 * (regValue & 1));
			} else {
				// 8 kb mode
				regValue &= numCHRBanks * 2 - 1;
				unsigned char* chrMem = cacheCHRBank(regValue >> 1);
				mapPPU(0x00, 8, chrMem);
			}
		}
	} else if (addr < 0xE000) {
		// CHR Bank 1
		if (numCHRBanks) {
			if (registers[4] == 1) {
				// 4 kb mode
				regValue &= numCHRBanks * 2 - 1;
				unsigned char* chrMem = cacheCHRBank(regValue >> 1);
				mapPPU(0x10, 4, chrMem + 4096 * (regValue & 1));
			} else {
				// 8 kb mode, ignored
			}
		}
	} else {
		unsigned int openBusMode = regValue & 0x10;
		if (openBusMode != registers[5]) {
			MMC1_SetRAMBank(openBusMode);
		}

		int selectePRGBlock = registers[7] & 0x20;
		regValue = regValue & (numPRGBanks - 1);
		// PRG Bank
		if (registers[3] < 2) {
			// 32 kb mode
			regValue &= 0xFE;
			regValue *= 2;
			regValue |= selectePRGBlock;
			registers[7] = regValue;
			registers[8] = regValue + 2;
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
			regValue |= selectePRGBlock;
			registers[8] = regValue;
			unsigned char* prgMem0 = cachePRGBank(regValue + 0);
			mapCPU(0xC0, 8, prgMem0);
			unsigned char* prgMem1 = cachePRGBank(regValue + 1);
			mapCPU(0xE0, 8, prgMem1);
		} else if (registers[3] == 3) {
			// changes bank at 0x8000
			regValue *= 2;
			regValue |= selectePRGBlock;
			registers[7] = regValue;
			unsigned char* prgMem0 = cachePRGBank(regValue + 0);
			mapCPU(0x80, 8, prgMem0);
			unsigned char* prgMem1 = cachePRGBank(regValue + 1);
			mapCPU(0xA0, 8, prgMem1);
		}
	}
}

void nes_cart::MMC1_SetRAMBank(int value) {
	registers[5] = value;

	if (!value) {
		mapCPU(0x60, 8, banks[registers[6]]);
	} else {
		for (int i = 0x60; i < 0x80; i++) {
			mainCPU.map[i] = openBus;
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

	cachedBankCount = availableROMBanks - 1;

	// chr map uses best bank for chr caching locality:
	nesPPU.chrMap = banks[cachedBankCount];

	// RAM bank (first index if applicable) set up at 0x6000
	if (numRAMBanks) {
		int ramBank0 = cachedBankCount + 1;
		registers[6] = ramBank0;

		// enable by default
		MMC1_SetRAMBank(0);
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNROM

void UNROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			}
		} else {
			// bank select
			value &= nesCart.numPRGBanks - 1;

			unsigned char* selected[2] = { nesCart.cachePRGBank(value*2), nesCart.cachePRGBank(value*2+1) };
			mapCPU(0x80, 8, selected[0]);
			mapCPU(0xA0, 8, selected[1]);
		}
	}
}

void nes_cart::setupMapper2_UNROM() {
	writeSpecial = UNROM_writeSpecial;

	cachedBankCount = availableROMBanks - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one ROM.. or RAM?) directly into PPU chrMap
	nesPPU.chrMap = banks[chrBank];
	if (numCHRBanks == 1) {
		unsigned char* romChrBank = cacheCHRBank(0);
		memcpy(nesPPU.chrMap, romChrBank, 8192);
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
	mapCPU(0x80, 8, firstBank[0]);
	mapCPU(0xA0, 8, firstBank[1]);
	unsigned char* lastBank[2] = { cachePRGBank(numPRGBanks * 2 - 2), cachePRGBank(numPRGBanks * 2 - 1) };
	mapCPU(0xC0, 8, lastBank[0]);
	mapCPU(0xE0, 8, lastBank[1]);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[chrBank + 1]);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNROM (switchable CHR banks)

// registers[0] = current mapped CHR bank
#define CNROM_CUR_CHR_BANK nesCart.registers[0]

void CNROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x8000) {
		// CHR bank select
		value &= nesCart.numCHRBanks - 1;

		if (value != CNROM_CUR_CHR_BANK) {
			nesPPU.chrMap = nesCart.cacheCHRBank(value);
			CNROM_CUR_CHR_BANK = value;
		}
	}
}

void nes_cart::setupMapper3_CNROM() {
	writeSpecial = CNROM_writeSpecial;

	cachedBankCount = availableROMBanks - 1;

	// map first 16 KB of PRG mamory to 80-BF, and last 16 KB to C0-FF
	unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
	mapCPU(0x80, 8, firstBank[0]);
	mapCPU(0xA0, 8, firstBank[1]);
	unsigned char* lastBank[2] = { cachePRGBank(numPRGBanks * 2 - 2), cachePRGBank(numPRGBanks * 2 - 1) };
	mapCPU(0xC0, 8, lastBank[0]);
	mapCPU(0xE0, 8, lastBank[1]);

	// cache first CHR bank by default
	nesPPU.chrMap = nesCart.cacheCHRBank(0);
	CNROM_CUR_CHR_BANK = 0;

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[availableROMBanks]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC3 (most popular mapper with IRQ)

// register 0-7 holds the 8 bank registers
#define MMC3_BANK_REG nesCart.registers

// register 8 is the bank select register (control values)
#define MMC3_BANK_SELECT nesCart.registers[8]

// register 9 is the IRQ counter latch value (the set value on reload)
#define MMC3_IRQ_SET nesCart.registers[9]

// register 10 is the IRQ counter reload flag
#define MMC3_IRQ_RELOAD nesCart.registers[10]

// register 11 is the actual IRQ counter value
#define MMC3_IRQ_COUNTER nesCart.registers[11]

// register 12 is the IRQ interrupt triggle enable/disable flag
#define MMC3_IRQ_ENABLE nesCart.registers[12]

// register 13 is the IRQ latch (when set, IRQ is dispatched with each A12 jump)
#define MMC3_IRQ_LATCH nesCart.registers[13]

// registers 16-19 contain an integer of the last time the IRQ counter was reset, used to fix IRQ timing since we are cheating by performing logic at beginning ot scanline
#define MMC3_IRQ_LASTSET *((unsigned int*) &nesCart.registers[16])

#define MMC3_CHRBANK0 (nesCart.availableROMBanks - 2)
#define MMC3_CHRBANK1 (nesCart.availableROMBanks - 1)

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
							// CHR mode change, fast A12 switch
							nesPPU.chrMap = (MMC3_BANK_SELECT & 0x80) ? nesCart.banks[MMC3_CHRBANK1] : nesCart.banks[MMC3_CHRBANK0];
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

				if (MMC3_IRQ_LASTSET + 90 > mainCPU.clocks) {
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
			unsigned char* chrData = cacheCHRBank(registers[0] >> 3) + (registers[0] & 0x6) * 1024;
			memcpy(banks[MMC3_CHRBANK0] + 0x0000, chrData, 2048);
			memcpy(banks[MMC3_CHRBANK1] + 0x1000, chrData, 2048);
			break;
		}
		case 1:
		{
			// 2 KB bank at $08 and $18
			unsigned char* chrData = cacheCHRBank(registers[1] >> 3) + (registers[1] & 0x6) * 1024;
			memcpy(banks[MMC3_CHRBANK0] + 0x0800, chrData, 2048);
			memcpy(banks[MMC3_CHRBANK1] + 0x1800, chrData, 2048);
			break;
		}
		case 2:
		{
			// 1 KB bank at $10 and $00
			unsigned char* chrData = cacheCHRBank(registers[2] >> 3) + (registers[2] & 0x7) * 1024;
			memcpy(banks[MMC3_CHRBANK0] + 0x1000, chrData, 1024);
			memcpy(banks[MMC3_CHRBANK1] + 0x0000, chrData, 1024);
			break;
		}
		case 3:
		{
			// 1 KB bank at $14 and $04
			unsigned char* chrData = cacheCHRBank(registers[3] >> 3) + (registers[3] & 0x7) * 1024;
			memcpy(banks[MMC3_CHRBANK0] + 0x1400, chrData, 1024);
			memcpy(banks[MMC3_CHRBANK1] + 0x0400, chrData, 1024);
			break;
		}
		case 4:
		{
			// 1 KB bank at $18 and $08
			unsigned char* chrData = cacheCHRBank(registers[4] >> 3) + (registers[4] & 0x7) * 1024;
			memcpy(banks[MMC3_CHRBANK0] + 0x1800, chrData, 1024);
			memcpy(banks[MMC3_CHRBANK1] + 0x0800, chrData, 1024);
			break;
		}
		case 5:
		{
			// 1 KB bank at $1C and $0C
			unsigned char* chrData = cacheCHRBank(registers[5] >> 3) + (registers[5] & 0x7) * 1024;
			memcpy(banks[MMC3_CHRBANK0] + 0x1C00, chrData, 1024);
			memcpy(banks[MMC3_CHRBANK1] + 0x0C00, chrData, 1024);
			break;
		}
		case 6:
		{
			// select bank at 80 (or C0)
			int prgBank = registers[6] & (numPRGBanks * 2 - 1);
			unsigned char* prgData = cachePRGBank(prgBank);
			if (registers[8] & 0x40) {
				mapCPU(0xC0, 8, prgData);
			} else {
				mapCPU(0x80, 8, prgData);
			}
			break;
		}
		case 7:
		{
			// select bank at A0
			int prgBank = registers[7] & (numPRGBanks * 2 - 1);
			unsigned char* prgData = cachePRGBank(prgBank);
			mapCPU(0xA0, 8, prgData);
			break;
		}
	}

	if (regNumber == -1) {
		for (int i = 0; i < 8; i++) {
			MMC3_UpdateMapping(i);
		}

		// possible bank mode change
		unsigned char* penultBank = cachePRGBank(numPRGBanks * 2 - 2);
		if (MMC3_BANK_SELECT & 0x40) {
			// 2nd to last bank set to 80-9F
			mapCPU(0x80, 8, penultBank);
		} else {
			// 2nd to last bank set to C0-DF
			mapCPU(0xC0, 8, penultBank);
		}
	}
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
				mainCPU.irqClocks = mainCPU.ppuClocks - (341 / 3) + flipCycles;
			}
		}
	}
}

void nes_cart::setupMapper4_MMC3() {
	writeSpecial = MMC3_writeSpecial;
	scanlineClock = MMC3_ScanlineClock;

	// 2 banks for chr memory to allow fast A12 CHR inversion
	cachedBankCount = MMC3_CHRBANK0;

	nesPPU.chrMap = banks[MMC3_CHRBANK0];

	// map first 8 KB of CHR memory to ppu chr if not ram
	mapPPU(0x00, 8, cacheCHRBank(0));

	// set fixed page up
	unsigned char* finalBank = cachePRGBank(numPRGBanks * 2 - 1);
	mapCPU(0xE0, 8, finalBank);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[availableROMBanks]);
	}

	// this will set up all non permanent memory
	MMC3_UpdateMapping(-1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AOROM (switches nametables for single screen mirroring)

// registers[0] = current nametable page (1 or 0)
#define AOROM_CUR_NAMETABLE nesCart.registers[0]

#define AOROM_NAMEBANK nesCart.availableROMBanks - 1

inline void AOROM_MapNameBank(int tableNum) {
	nesPPU.nameTables = (nes_nametable*)(nesCart.banks[AOROM_NAMEBANK] + 4096 * tableNum);
}

void AOROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			}
		} else {
			// nametable select
			unsigned int nametable = (value >> 4) & 1;
			if (nametable != AOROM_CUR_NAMETABLE) {
				AOROM_MapNameBank(nametable);
				AOROM_CUR_NAMETABLE = nametable;
			}

			// bank select

			// mask to the number of 32 kb banks we have
			value &= nesCart.numPRGBanks / 2 - 1;

			// 32 kb at a time
			unsigned char* selected[4] = {
				nesCart.cachePRGBank(value*4),
				nesCart.cachePRGBank(value*4+1),
				nesCart.cachePRGBank(value*4+2),
				nesCart.cachePRGBank(value*4+3)
			};
			mapCPU(0x80, 8, selected[0]);
			mapCPU(0xA0, 8, selected[1]);
			mapCPU(0xC0, 8, selected[2]);
			mapCPU(0xE0, 8, selected[3]);
		}
	}
}

void nes_cart::setupMapper7_AOROM() {
	writeSpecial = AOROM_writeSpecial;

	cachedBankCount = AOROM_NAMEBANK - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one ROM.. or RAM?) directly into PPU chrMap
	nesPPU.chrMap = banks[chrBank];
	if (numCHRBanks == 1) {
		unsigned char* romChrBank = cacheCHRBank(0);
		memcpy(nesPPU.chrMap, romChrBank, 8192);
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
	mapCPU(0x80, 8, firstBank[0]);
	mapCPU(0xA0, 8, firstBank[1]);
	unsigned char* lastBank[2] = { cachePRGBank(numPRGBanks * 2 - 2), cachePRGBank(numPRGBanks * 2 - 1) };
	mapCPU(0xC0, 8, lastBank[0]);
	mapCPU(0xE0, 8, lastBank[1]);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[availableROMBanks]);
	}

	// set up single screen mirroring
	AOROM_MapNameBank(0);
	nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC2 (Punch Out mapper)

// This works by keeping 4 separate copies of the CHR bank around, so they can be instantly switched by
// changing the PPU's chr map pointer. This does mean that we have to save two versions of each latched
// CHR map value on writes to the MMC2 registers, but that's not render critical in the same way

// register 0 is the current low CHR map latch value : 0 = FD, 1 = FE
// register 1 is the current high CHR map latch value : 0 = FD, 1 = FE

// register 2 is the current bank copied into the PRG select
// register 3 is the current CHR bank selected for low CHR map, latched with FD
// register 4 is the current CHR bank selected for low CHR map, latched with FE
// register 5 is the current CHR bank selected for high CHR map, latched with FD
// register 6 is the current CHR bank selected for high CHR map, latched with FE

#define MMC2_CHRBANK0 (nesCart.availableROMBanks-4)
#define MMC2_CHRBANK1 (nesCart.availableROMBanks-3)
#define MMC2_CHRBANK2 (nesCart.availableROMBanks-2)
#define MMC2_CHRBANK3 (nesCart.availableROMBanks-1)

inline void MMC2_selectCHRMap() {
	nesPPU.chrMap = nesCart.banks[MMC2_CHRBANK0 + nesCart.registers[0] + nesCart.registers[1] * 2];
}

void MMC2_renderLatch(unsigned int address) {
	if (address < 0x1000) {
		if (address == 0x0FD8) {
			if (nesCart.registers[0] == 1) {
				nesCart.registers[0] = 0;
				MMC2_selectCHRMap();
			}
		} else if (address == 0x0FE8) {
			if (nesCart.registers[0] == 0) {
				nesCart.registers[0] = 1;
				MMC2_selectCHRMap();
			}
		}
	} else {
		if (address >= 0x1FD8 && address <= 0x1FDF) {
			if (nesCart.registers[1] == 1) {
				nesCart.registers[1] = 0;
				MMC2_selectCHRMap();
			}
		} else if (address >= 0x1FE8 && address <= 0x1FEF) {
			if (nesCart.registers[1] == 0) {
				nesCart.registers[1] = 1;
				MMC2_selectCHRMap();
			}
		}
	}
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
				if (value != nesCart.registers[2]) {
					mapCPU(0x80, 8, nesCart.cachePRGBank(value));
					nesCart.registers[2] = value;
				}
			} else if (address < 0xC000) {
				// low CHR / FD select
				value &= 0x1F;
				if (value != nesCart.registers[3]) {
					unsigned char* chrMap = nesCart.cacheCHRBank(value >> 1) + ((value & 1) << 12);
					// map to lower 4k of bank 0 and 2
					memcpy(nesCart.banks[MMC2_CHRBANK0], chrMap, 4096);
					memcpy(nesCart.banks[MMC2_CHRBANK2], chrMap, 4096);
					nesCart.registers[3] = value;
				}
			} else if (address < 0xD000) {
				// low CHR / FE select
				value &= 0x1F;
				if (value != nesCart.registers[4]) {
					unsigned char* chrMap = nesCart.cacheCHRBank(value >> 1) + ((value & 1) << 12);
					// map to lower 4k of bank 1 and 3
					memcpy(nesCart.banks[MMC2_CHRBANK1], chrMap, 4096);
					memcpy(nesCart.banks[MMC2_CHRBANK3], chrMap, 4096);
					nesCart.registers[4] = value;
				}
			} else if (address < 0xE000) {
				// high CHR / FD select
				value &= 0x1F;
				if (value != nesCart.registers[5]) {
					unsigned char* chrMap = nesCart.cacheCHRBank(value >> 1) + ((value & 1) << 12);
					// map to higher 4k of bank 0 and 1
					memcpy(nesCart.banks[MMC2_CHRBANK0] + 4096, chrMap, 4096);
					memcpy(nesCart.banks[MMC2_CHRBANK1] + 4096, chrMap, 4096);
					nesCart.registers[5] = value;
				}
			} else if (address < 0xF000) {
				// high CHR / FE select
				value &= 0x1F;
				if (value != nesCart.registers[6]) {
					unsigned char* chrMap = nesCart.cacheCHRBank(value >> 1) + ((value & 1) << 12);
					// map to higher 4k of bank 2 and 3
					memcpy(nesCart.banks[MMC2_CHRBANK2] + 4096, chrMap, 4096);
					memcpy(nesCart.banks[MMC2_CHRBANK3] + 4096, chrMap, 4096);
					nesCart.registers[6] = value;
				}
			} else {
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

	cachedBankCount = MMC2_CHRBANK0;

	registers[0] = 0;
	registers[1] = 0;
	registers[2] = -1;
	registers[3] = -1;
	registers[4] = -1;
	registers[5] = -1;
	registers[6] = -1;

	// map memory to read-in ROM
	mapCPU(0x80, 8, cachePRGBank(0));
	mapCPU(0xA0, 8, cachePRGBank(numPRGBanks * 2 - 3));
	mapCPU(0xC0, 8, cachePRGBank(numPRGBanks * 2 - 2));
	mapCPU(0xE0, 8, cachePRGBank(numPRGBanks * 2 - 1));

	// read first CHR bank (for now) and put it into all 4 CHR banks
	unsigned char* chrBank = cacheCHRBank(0);
	memcpy(banks[MMC2_CHRBANK0], chrBank, 1024 * 8);
	memcpy(banks[MMC2_CHRBANK1], chrBank, 1024 * 8);
	memcpy(banks[MMC2_CHRBANK2], chrBank, 1024 * 8);
	memcpy(banks[MMC2_CHRBANK3], chrBank, 1024 * 8);
	nesPPU.chrMap = banks[availableROMBanks-1];

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[availableROMBanks]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color Dreams Mapper 11

#define Mapper11_PRG_SELECT nesCart.registers[8]
#define Mapper11_CHR_SELECT nesCart.registers[8]

void Mapper11_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x8000) {
		unsigned int prg = (value & 0x03) * 4;
		unsigned int chr = (value & 0xF0) >> 4;

		if (prg != Mapper11_PRG_SELECT) {
			unsigned char* prgBanks[4] = { nesCart.cachePRGBank(prg), nesCart.cachePRGBank(prg+1), nesCart.cachePRGBank(prg+2), nesCart.cachePRGBank(prg+3) };
			mapCPU(0x80, 8, prgBanks[0]);
			mapCPU(0xA0, 8, prgBanks[1]);
			mapCPU(0xC0, 8, prgBanks[2]);
			mapCPU(0xE0, 8, prgBanks[3]);

			Mapper11_PRG_SELECT = prg;
		}

		if (chr != Mapper11_CHR_SELECT) {
			nesPPU.chrMap = nesCart.cacheCHRBank(chr);

			Mapper11_CHR_SELECT = chr;
		}
	}
}

void nes_cart::setupMapper11_ColorDreams() {
	writeSpecial = Mapper11_writeSpecial;

	cachedBankCount = availableROMBanks;

	// map first 32 KB of PRG mamory to 80-FF by default
	unsigned char* prgBanks[4] = { cachePRGBank(0), cachePRGBank(1), cachePRGBank(2), cachePRGBank(3) };
	mapCPU(0x80, 8, prgBanks[0]);
	mapCPU(0xA0, 8, prgBanks[1]);
	mapCPU(0xC0, 8, prgBanks[2]);
	mapCPU(0xE0, 8, prgBanks[3]);

	nesPPU.chrMap = cacheCHRBank(0);

	Mapper11_PRG_SELECT = 0;
	Mapper11_CHR_SELECT = 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Camerica Mapper 71

void Mapper71_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			if (nesCart.numRAMBanks) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
			}
		}
		else if (address >= 0xC000) {
			// bank select
			value &= (nesCart.numPRGBanks - 1) & (0xF);

			unsigned char* selected[2] = { nesCart.cachePRGBank(value * 2), nesCart.cachePRGBank(value * 2 + 1) };
			mapCPU(0x80, 8, selected[0]);
			mapCPU(0xA0, 8, selected[1]);
		}
	}
}

void nes_cart::setupMapper71_Camerica() {
	writeSpecial = Mapper71_writeSpecial;

	cachedBankCount = availableROMBanks - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one RAM) directly into PPU chrMap
	nesPPU.chrMap = banks[chrBank];
	if (numCHRBanks == 1) {
		unsigned char* romChrBank = cacheCHRBank(0);
		memcpy(nesPPU.chrMap, romChrBank, 8192);
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	unsigned char* firstBank[2] = { cachePRGBank(0), cachePRGBank(1) };
	mapCPU(0x80, 8, firstBank[0]);
	mapCPU(0xA0, 8, firstBank[1]);
	unsigned char* lastBank[2] = { cachePRGBank(numPRGBanks * 2 - 2), cachePRGBank(numPRGBanks * 2 - 1) };
	mapCPU(0xC0, 8, lastBank[0]);
	mapCPU(0xE0, 8, lastBank[1]);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, banks[availableROMBanks]);
	}
}