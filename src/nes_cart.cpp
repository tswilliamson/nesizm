
// Implementation of NES ROM files and mappers (following the popular iNES format)

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "mappers.h"

nes_cart nesCart;

nes_nametable nes_onboardPPUTables[4];

FORCE_INLINE void memcpy_fast32(void* dest, const void* src, unsigned int size) {
#if TARGET_WINSIM
	DebugAssert((((uint32)dest) & 3) == 0);
	DebugAssert((((uint32)src) & 3) == 0);
	DebugAssert((((uint32)size) & 31) == 0);
	memcpy(dest, src, size);
#elif TARGET_PRIZM
	asm(
		// line 1
		"mov %[dest],r1							\n\t"
		"add %[size],r1							\n\t"  // r1 = size + dest ( loop point end)
		"mov.l @%[src]+,r0						\n\t"
		"1:\n"	// local label
		"mov.l r0,@(0,%[dest])					\n\t" // copy 32 bytes accting for branch delay
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(4,%[dest])					\n\t" 
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(8,%[dest])					\n\t" 
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(12,%[dest])					\n\t" 
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(16,%[dest])					\n\t" 
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(20,%[dest])					\n\t" 
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(24,%[dest])					\n\t" 
		"mov.l @%[src]+,r0						\n\t"
		"mov.l r0,@(28,%[dest])					\n\t" 
		"add #32,%[dest]						\n\t"			
		"cmp/eq %[dest],r1						\n\t" // if dest == dest + size (end point), T = 1
		"bf/s 1b								\n\t" // if T == 0 go back to last local label 1: after delay instruction
		"mov.l @%[src]+,r0						\n\t" // delayed branch instruction
		// outputs
		: 
		// inputs
		: [dest] "r" (dest), [src] "r" (src), [size] "r" (size)
		// clobbers
		: "r0", "r1"
	);
#else
#pragma Unknown Target!
#endif
}

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
	romFile[0] = 0;
}

void nes_cart::allocateBanks(unsigned char* staticAlloced) {
	allocatedROMBanks = STATIC_CACHED_ROM_BANKS;
	for (int i = 0; i < STATIC_CACHED_ROM_BANKS; i++) {
		cache[i].ptr = staticAlloced + 8192 * i;
	}

	for (int i = STATIC_CACHED_ROM_BANKS; i < MAX_CACHED_ROM_BANKS; i++) {
		unsigned char* heapBank = (unsigned char*) malloc(8192);
		if (heapBank) {
			cache[i].ptr = heapBank;
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
			memset(cache[availableROMBanks + i].ptr, 0, 8192);
		}

		if (isBatteryBacked && numRAMBanks == 1) {
			strcpy(savFile, withFile);
			*strrchr(savFile, '.') = 0;
			strcat(savFile, ".sav");
			Bfile_StrToName_ncpy(romName, savFile, 255);

			// try to load file
			int saveFileHandle = Bfile_OpenFile_OS(romName, READ, 0);
			if (saveFileHandle > 0) {
				Bfile_ReadFile_OS(saveFileHandle, cache[availableROMBanks].ptr, 8192, -1);
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
		strcpy(romFile, withFile);
		printf("Mapper %d : supported", mapper);
		return true;
	} else {
		handle = 0;
		printf("Mapper %d : unsupported", mapper);
		Bfile_CloseFile_OS(file);
		return false;
	}
}

void nes_cart::readState_WRAM(uint8* data) {
	if (numRAMBanks >= 1) {
		memcpy_fast32(cache[availableROMBanks].ptr, data, 0x2000);
	}
}

uint8* nes_cart::GetWRAM() {
	if (numRAMBanks >= 1) {
		return (uint8*) cache[availableROMBanks].ptr;
	} else {
		return nullptr;
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
			hash = hash * 31 + cache[availableROMBanks].ptr[i];
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
				Bfile_WriteFile_OS(savHandle, cache[availableROMBanks].ptr, 8192);
				Bfile_CloseFile_OS(savHandle);
			}
		}
	}

	// close rom handle for now (will re open on continue)
	Bfile_CloseFile_OS(handle);
	handle = 0;
}

void nes_cart::OnContinue() {
	DebugAssert(handle == 0 && romFile[0]);
	unsigned short filename[128];
	Bfile_StrToName_ncpy(filename, romFile, 127);
	handle = Bfile_OpenFile_OS(filename, READ, 0);
	BuildFileBlocks();
}

bool nes_cart::setupMapper() {
	renderLatch = NULL;
	writeSpecial = NULL;
	bSwapChrPages = false;

	// most mirror configs just use the on board ppu nametables:
	nesPPU.nameTables = nes_onboardPPUTables;

	clearCacheData();

	memset(registers, 0, sizeof(registers));
	memset(programBanks, 0xFF, sizeof(programBanks));
	memset(chrBanks, 0xFF, sizeof(chrBanks));

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
		cache[i].clear();
	}

	cachedBankCount = 0;
}

// returns whether the bank given is in use by the memory map
bool nes_cart::isBankUsed(int index) {
	unsigned char* ptr = cache[index].ptr;

	if (ptr == nesPPU.chrPages[0] || ptr == nesPPU.chrPages[1] || ptr + 0x1000 == nesPPU.chrPages[0] || ptr + 0x1000 == nesPPU.chrPages[1])
		return true;

	for (int i = 0; i < 4; i++) {
		if (programBanks[i] == cache[index].prgIndex) {
			return true;
		}
	}

	return false;
}

// finds least recently requested bank that is currently unused
int nes_cart::findOldestUnusedBank() {
	int bestBank = -1;
	for (int i = 0; i < cachedBankCount; i++) {
		if (bestBank == -1 || cache[i].request < cache[bestBank].request) {
			if (isBankUsed(i) == false) {
				bestBank = i;
			} else {
				// mark as "requested" since it obviously is still in use
				cache[i].request = requestIndex;
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
		if (cache[i].prgIndex == index) {
			cache[i].request = requestIndex;
			return cache[i].ptr;
		}
	}

	// replace smallest request with inactive memory
	int replaceIndex = findOldestUnusedBank();
	cache[replaceIndex].prgIndex = index;
	cache[replaceIndex].request = requestIndex;
	BlockRead(cache[replaceIndex].ptr, 8192, 16 + 8192 * index);
	return cache[replaceIndex].ptr;
}

// caches an 8 KB CHR bank, returns result bank memory pointer
unsigned char* nes_cart::cacheCHRBank(int16* indices) {
	int chrBankMask = (numCHRBanks << 3) - 1;
	indices[0] &= chrBankMask;
	indices[1] &= chrBankMask;
	indices[2] &= chrBankMask;
	indices[3] &= chrBankMask;
	indices[4] &= chrBankMask;
	indices[5] &= chrBankMask;
	indices[6] &= chrBankMask;
	indices[7] &= chrBankMask;

	requestIndex++;

	// find bank index within range:
	const int cacheIndex = 4096;
	for (int i = 0; i < cachedBankCount; i++) {
		if (cache[i].prgIndex == cacheIndex) {
			nes_cached_bank& bank = cache[i];
			if (bank.chrIndex[0] == indices[0] && bank.chrIndex[1] == indices[1] &&
				bank.chrIndex[2] == indices[2] && bank.chrIndex[3] == indices[3] &&
				bank.chrIndex[4] == indices[4] && bank.chrIndex[5] == indices[5] &&
				bank.chrIndex[6] == indices[6] && bank.chrIndex[7] == indices[7])
			{
				bank.request = requestIndex;
				return bank.ptr;
			}
		}
	}

	// replace smallest request entirely (no inactive memory for CHR ROM since they are copied to PPU mem directly)
	int replaceIndex = findOldestUnusedBank();
	nes_cached_bank& bank = cache[replaceIndex];
	bank.prgIndex = cacheIndex;
	bank.request = requestIndex;
	for (int32 i = 0; i < 8; i++) {
		bank.chrIndex[i] = indices[i];
		BlockRead(bank.ptr + 1024 * i, 1024, 16 + 16384 * numPRGBanks + 1024 * indices[i]);
	}
	return bank.ptr;
}

// inline mapping helpers
inline void mapCPU(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
	DebugAssert((startAddrHigh & ((1<<5)-1)) == 0);

	// 256 byte increments
	numKB *= 4;

	for (unsigned int i = 0; i < numKB; i++) {
		mainCPU.map[i + startAddrHigh] = &ptr[i * 0x100];
	}
}

inline void mapPPU(unsigned int startAddrHigh, unsigned int numKB, unsigned char* ptr) {
	// just copy to CHR ROM
	DebugAssert(startAddrHigh * 0x100 + numKB * 1024 <= 0x2000);
	DebugAssert(numKB <= 4);

	int page = (startAddrHigh & 0x10) >> 4;
	startAddrHigh &= 0x0F;
	memcpy_fast32(nesPPU.chrPages[page] + startAddrHigh * 0x100, ptr, numKB * 1024);
}


void nes_cart::MapProgramBanks(int32 toBank, int32 cartBank, int32 numBanks) {
	DebugAssert(toBank + numBanks <= 4);

	for (int32 i = 0; i < numBanks; i++) {
		const int32 destBank = i + toBank;
		if (programBanks[destBank] != cartBank + i) {
			programBanks[destBank] = cartBank + i;
			mapCPU(0x80 + 0x20 * (destBank), 8, cachePRGBank(cartBank + i));
		}
	}
}

void nes_cart::CommitChrBanks() {
	uint8* bankData = cacheCHRBank(chrBanks);

	if (bSwapChrPages) {
		nesPPU.chrPages[0] = bankData + 0x1000;
		nesPPU.chrPages[1] = bankData;
	} else {
		nesPPU.chrPages[0] = bankData;
		nesPPU.chrPages[1] = bankData + 0x1000;
	}
	
	bDirtyChrBanks = false;
}

void nes_cart::MapCharacterBanks(int32 toBank, int32 cartBank, int32 numBanks) {
	DebugAssert(toBank + numBanks <= 8);

	for (int32 i = 0; i < numBanks; i++) {
		const int32 destBank = i + toBank;
		if (chrBanks[destBank] != cartBank + i) {
			chrBanks[destBank] = cartBank + i;
			bDirtyChrBanks = true;
		}
	}
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

	// read CHR bank (always one ROM) directly into PPU chrMap
	DebugAssert(numCHRBanks == 1);
	MapCharacterBanks(0, 0, 8);

	// map memory to read-in ROM
	if (numPRGBanks == 1) {
		MapProgramBanks(0, 0, 2);
		MapProgramBanks(2, 0, 2);
	} else {
		DebugAssert(numPRGBanks == 2);
		MapProgramBanks(0, 0, 4);
	}

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC1

void MMC1_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x6000) {
		if (address < 0x8000) {
			// if enabled and available
			if (nesCart.numRAMBanks && MMC1_RAM_DISABLE == 0) {
				// RAM
				mainCPU.map[address >> 8][address & 0xFF] = value;
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
		mapCPU(0x60, 8, cache[MMC1_RAM_BANK].ptr);
	} else {
		for (int i = 0x60; i < 0x80; i++) {
			mainCPU.map[i] = openBus;
		}
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

			nesCart.MapProgramBanks(0, value * 2, 2);
		}
	}
}

void nes_cart::setupMapper2_UNROM() {
	writeSpecial = UNROM_writeSpecial;

	cachedBankCount = availableROMBanks - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one ROM.. or RAM?) directly into PPU chrMap
	if (numCHRBanks == 1) {
		MapCharacterBanks(0, 0, 8);
	} else {
		nesPPU.chrPages[0] = cache[chrBank].ptr;
		nesPPU.chrPages[1] = cache[chrBank].ptr + 0x1000;
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[chrBank + 1].ptr);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNROM (switchable CHR banks)

void CNROM_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x8000) {
		// CHR bank select
		value &= nesCart.numCHRBanks - 1;

		if (value != CNROM_CUR_CHR_BANK) {
			nesCart.MapCharacterBanks(0, value * 8, 8);
			CNROM_CUR_CHR_BANK = value;
		}
	}
}

void nes_cart::setupMapper3_CNROM() {
	writeSpecial = CNROM_writeSpecial;

	cachedBankCount = availableROMBanks - 1;

	// map first 16 KB of PRG mamory to 80-BF, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// cache first CHR bank by default
	MapCharacterBanks(0, 0, 8);
	CNROM_CUR_CHR_BANK = 0;

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AOROM (switches nametables for single screen mirroring)

inline void AOROM_MapNameBank(int tableNum) {
	nesPPU.nameTables = (nes_nametable*)(nesCart.cache[AOROM_NAMEBANK].ptr + 4096 * tableNum);
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
			nesCart.MapProgramBanks(0, value * 4, 4);
		}
	}
}

void nes_cart::AOROM_StateLoaded() {
	AOROM_MapNameBank(AOROM_CUR_NAMETABLE);
}

void nes_cart::setupMapper7_AOROM() {
	writeSpecial = AOROM_writeSpecial;

	cachedBankCount = AOROM_NAMEBANK - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one ROM.. or RAM?) directly into PPU chrMap
	if (numCHRBanks == 1) {
		MapCharacterBanks(0, 0, 8);
	} else {
		nesPPU.chrPages[0] = cache[chrBank].ptr;
		nesPPU.chrPages[1] = cache[chrBank].ptr + 0x1000;
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}

	// set up single screen mirroring
	AOROM_MapNameBank(0);
	nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC2 (Punch Out mapper)

inline void MMC2_selectCHRMap() {
	if (nesCart.registers[0]) {
		nesCart.MapCharacterBanks(0, nesCart.registers[4] * 4, 4);
	} else {
		nesCart.MapCharacterBanks(0, nesCart.registers[3] * 4, 4);
	}

	if (nesCart.registers[1]) {
		nesCart.MapCharacterBanks(4, nesCart.registers[6] * 4, 4);
	} else {
		nesCart.MapCharacterBanks(4, nesCart.registers[5] * 4, 4);
	}
}

void MMC2_renderLatch(unsigned int address) {
	bool bNeedCommit = false;

	if (address < 0x1000) {
		if (address == 0x0FD8) {
			if (nesCart.registers[0] == 1) {
				nesCart.registers[0] = 0;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		} else if (address == 0x0FE8) {
			if (nesCart.registers[0] == 0) {
				nesCart.registers[0] = 1;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		}
	} else {
		if (address >= 0x1FD8 && address <= 0x1FDF) {
			if (nesCart.registers[1] == 1) {
				nesCart.registers[1] = 0;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		} else if (address >= 0x1FE8 && address <= 0x1FEF) {
			if (nesCart.registers[1] == 0) {
				nesCart.registers[1] = 1;
				MMC2_selectCHRMap();
				bNeedCommit = true;
			}
		}
	}

	if (bNeedCommit) {
		nesCart.CommitChrBanks();
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
				nesCart.MapProgramBanks(0, value, 1);
			}
			else if (address < 0xC000) {
				// low CHR / FD select
				value &= 0x1F;
				if (value != nesCart.registers[3]) {
					// map to lower 4k of bank 0 and 2
					nesCart.registers[3] = value;
					MMC2_selectCHRMap();
				}
			} else if (address < 0xD000) {
				// low CHR / FE select
				value &= 0x1F;
				if (value != nesCart.registers[4]) {
					// map to lower 4k of bank 1 and 3
					nesCart.registers[4] = value;
					MMC2_selectCHRMap();
				}
			} else if (address < 0xE000) {
				// high CHR / FD select
				value &= 0x1F;
				if (value != nesCart.registers[5]) {
					// map to higher 4k of bank 0 and 1
					nesCart.registers[5] = value;
					MMC2_selectCHRMap();
				}
			} else if (address < 0xF000) {
				// high CHR / FE select
				value &= 0x1F;
				if (value != nesCart.registers[6]) {
					// map to higher 4k of bank 2 and 3
					nesCart.registers[6] = value;
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

	registers[0] = 0;
	registers[1] = 0;
	registers[2] = -1;
	registers[3] = 0;
	registers[4] = 0;
	registers[5] = 0;
	registers[6] = 0;

	// map memory to read-in ROM
	MapProgramBanks(0, 0, 1);
	MapProgramBanks(1, numPRGBanks * 2 - 3, 3);

	MMC2_selectCHRMap();

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color Dreams Mapper 11

void Mapper11_writeSpecial(unsigned int address, unsigned char value) {
	if (address >= 0x8000) {
		unsigned int prg = (value & 0x03) * 4;
		unsigned int chr = (value & 0xF0) >> 4;

		if (prg != Mapper11_PRG_SELECT) {
			nesCart.MapProgramBanks(0, prg, 4);
			Mapper11_PRG_SELECT = prg;
		}

		if (chr != Mapper11_CHR_SELECT) {
			nesCart.MapCharacterBanks(0, chr * 8, 8);
			Mapper11_CHR_SELECT = chr;
		}
	}
}

void nes_cart::setupMapper11_ColorDreams() {
	writeSpecial = Mapper11_writeSpecial;

	cachedBankCount = availableROMBanks;

	// map first 32 KB of PRG mamory to 80-FF by default
	MapProgramBanks(0, 0, 4);

	MapCharacterBanks(0, 0, 8);

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

			nesCart.MapProgramBanks(0, value * 2, 2);
		}
	}
}

void nes_cart::setupMapper71_Camerica() {
	writeSpecial = Mapper71_writeSpecial;

	cachedBankCount = availableROMBanks - 1;
	int chrBank = cachedBankCount;

	// read CHR bank (always one RAM) directly into PPU chrMap if not in ROM
	if (numCHRBanks == 1) {
		MapCharacterBanks(0, 0, 8);
	} else {
		nesPPU.chrPages[0] = cache[chrBank].ptr;
		nesPPU.chrPages[1] = cache[chrBank].ptr + 0x1000;
	}

	// map first 16 KB of PRG mamory to 80-BF by default, and last 16 KB to C0-FF
	MapProgramBanks(0, 0, 2);
	MapProgramBanks(2, numPRGBanks * 2 - 2, 2);

	// RAM bank if one is set up
	if (numRAMBanks == 1) {
		mapCPU(0x60, 8, cache[availableROMBanks].ptr);
	}
}