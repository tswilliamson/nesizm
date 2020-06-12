
// Implementation of NES ROM files and mappers (following the popular iNES format)

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "mappers.h"
#include "scope_timer/scope_timer.h"
#include "snd/snd.h"

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
	int fileSize = Bfile_GetFileSize_OS(file);
	if (fileSize < 16 || Bfile_ReadFile_OS(file, header, 16, 0) != 16) {
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

	// byte 5: num CHR ROM banks in 8 KB units
	numCHRBanks = header[5];
	expectedSize += numCHRBanks * 8 * 1024;

	// determine iNES type now that we may need to account for more ROM banks
	int format = 0; // default to "archaic iNES"
	if ((header[7] & 0x0C) == 0 && header[12] == 0 && header[13] == 0 && header[14] == 0 && header[15] == 0) {
		format = 1; // standard iNES
	} else if ((header[7] & 0x0C) == 0x08) {
		int proposedPRGBanks = (header[9] << 8) + numPRGBanks;
		int newMinSize = expectedSize + proposedPRGBanks * 16 * 1024;
		if (fileSize <= newMinSize) {
			format = 2; // iNES 2.0!
			numPRGBanks = proposedPRGBanks;
		}
	}
	expectedSize += numPRGBanks * 16 * 1024;

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
	isLowPRGROM = 0;

	bool hasTrainer = (header[6] & (1 << 2)) != 0;
	if (hasTrainer) {
		printf("ROM has trainer (not supported).");
		Bfile_CloseFile_OS(file);
		return false;
	}

	// TV system on different bytes based on format
	if (format == 1) {
		isPAL = header[9] & 1;
	} else if (format == 2) {
		isPAL = (header[12] & 3) == 1 ? 1 : 0;
	} else {
		isPAL = 0;
	}
	// force PAL hack as well as basic europe checks
	if (strstr(withFile, "PAL") || strstr(withFile, "(E)") || strstr(withFile, "(Europe)")) {
 		isPAL = 1;
	}

	// check for unsupported system types
	if (format != 0) {
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
	}

	if (format == 1) {
		// byte 8: num RAM banks in 8 KB allotments, or byte 10 if NES 2.0
		numRAMBanks = header[8];

		// mapper 
		mapper = (header[7] & 0xF0) | (header[6] >> 4);
		subMapper = -1;
	}

	if (format == 2) {
		if (header[10]) {
			int ramSize = 64 << (header[10]);
			numRAMBanks = (ramSize + 8191) / 8192;
		} else {
			numRAMBanks = 0;
		}

		mapper = ((header[8] & 0x0F) << 8) | (header[7] & 0xF0) | (header[6] >> 4);
		subMapper = (header[8] & 0xF0) >> 4;
	}

	if (format == 0) {
		numRAMBanks = 1;
		mapper = (header[6] >> 4);
		subMapper = -1;
	}

	if (numRAMBanks == 0)
		numRAMBanks = 1; // always allocate 1 just in case for bad ROMS
	if (numRAMBanks > 1 && isBatteryBacked) {
		printf("More than 1 RAM bank (%d) unsupported", numRAMBanks);
		Bfile_CloseFile_OS(file);
		return false;
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

	// initialize TV settings
	nesPPU.initTV();

	// initialize APU
	nesAPU.init();

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

	// reset frame skip value
	nesPPU.autoFrameSkip = 0;
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

	// by default map 0x5000 - 0x7FFF to open bus to start
	mapOpenBus(0x50, 12);

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
		case 10:
			setupMapper9_MMC2();
			return true;
		case 11:
			setupMapper11_ColorDreams();
			return true;
		case 34:
			setupMapper34_BNROM();
			return true;
		case 64:
			setupMapper64_Rambo1();
			return true;
		case 66:
		case 140:
			setupMapper66_GXROM();
			return true;
		case 67:
			setupMapper67_Sunsoft3();
			return true;
		case 68:
			setupMapper68_Sunsoft4();
			return true;
		case 69:
			setupMapper69_Sunsoft();
			return true;
		case 71:
			setupMapper71_Camerica();
			return true;
		case 79:
			setupMapper79_AVE();
			return true;
		case 163:
			setupMapper163_Nanjing();
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

		condSoundUpdate();
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

	for (int i = 0; i < 4 + isLowPRGROM; i++) {
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

// caches a single 8 KB CHR bank based on the 8 KB index, returns result bank memory pointer
unsigned char* nes_cart::cacheSingleCHRBank(int16 index) {
	int16 indices[8] = {
		index * 8 + 0, index * 8 + 1, index * 8 + 2, index * 8 + 3,
		index * 8 + 4, index * 8 + 5, index * 8 + 6, index * 8 + 7
	};

	return cacheCHRBank(indices);
}

void nes_cart::MapProgramBanks(int32 toBank, int32 cartBank, int32 numBanks) {
	DebugAssert(toBank + numBanks <= 4 + isLowPRGROM);

	const unsigned int addrTarget[5] = {0x80, 0xA0, 0xC0, 0xE0, 0x60};

	for (int32 i = 0; i < numBanks; i++) {
		const int32 destBank = i + toBank;
		if (programBanks[destBank] != cartBank + i) {
			programBanks[destBank] = cartBank + i;
			mapCPU(addrTarget[destBank], 8, cachePRGBank(cartBank + i));
		}
	}
}

void nes_cart::CommitChrBanks() {
	DebugAssert(numCHRBanks); // should not happen with CHR RAM

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

bool nes_cart::IRQReached() {
	if (mapper == 64) {
		if (Mapper64_IRQ_MODE == 1) {
			// set next IRQ breakpoint
			Mapper64_IRQ_CLOCKS = mainCPU.irqClock[0] + Mapper64_IRQ_COUNT + 4;
			if (Mapper64_IRQ_ENABLE) {
				mainCPU.setIRQ(0, Mapper64_IRQ_CLOCKS);
				return true;
			}

			mainCPU.ackIRQ(0);
			return false;
		}
	}

	if (mapper == 4) {
		MMC3_IRQ_LATCH = 0;
	}

	if (mapper == 67) {
		Mapper67_IRQ_Counter = 0xFFFF;
		Mapper67_IRQ_Enable = 0;
		return true;
	}

	if (mapper == 69) {
		if ((Mapper69_IRQCONTROL & 0x1) == 0) {
			mainCPU.ackIRQ(0);
			return false;
		}
	}

	// by default disable IRQ
	mainCPU.ackIRQ(0);

	return true;
}

void nes_cart::rollbackClocks(unsigned int clockCount) {
	if (mapper == 4) {
		MMC3_IRQ_LASTSET -= clockCount;
	} else if (mapper == 64) {
		if (Mapper64_IRQ_CLOCKS) {
			Mapper64_IRQ_CLOCKS -= clockCount;
		}
	} else if (mapper == 67) {
		Mapper67_IRQ_LastSet -= clockCount;
	} else if (mapper == 69) {
		if (Mapper69_LASTCOUNTERCLK) {
			Mapper69_LASTCOUNTERCLK -= clockCount;
		}
	}
}