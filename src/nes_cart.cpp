
// Implementation of NES ROM files and mappers (following the popular iNES format)

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_cart nesCart;

nes_cart::nes_cart() {
	handle = 0;
	file[0] = 0;
}

void nes_cart::allocateROMBanks(unsigned char* withAlloced) {
	for (int i = 0; i < NUM_CACHED_ROM_BANKS; i++) {
		romBanks[i] = withAlloced + 8192 * i;
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
		nesPPU.mirror = nes_ppu::MT_4PANE;
	} else if (header[6] & (1 << 0)) {
		nesPPU.mirror = nes_ppu::MT_VERTICAL;
	} else {
		nesPPU.mirror = nes_ppu::MT_HORIZONTAL;
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
		printf("Not expected file size based on format.");
		Bfile_CloseFile_OS(file);
		return false;
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

bool nes_cart::setupMapper() {
	switch (mapper) {
		case 0:
			setupMapper0_NROM();
			return true;
		default:
			return false;
	}
}

void nes_cart::setupMapper0_NROM() {
	// read ROM banks (up to 2)
	for (int i = 0; i < 2 * numPRGBanks; i++) {
		Bfile_ReadFile_OS(handle, romBanks[i], 8192, -1);
	}

	// read CHR bank (always one)
	int chrBank = 2 * numPRGBanks;
	Bfile_ReadFile_OS(handle, romBanks[chrBank], 8192, -1);
	nesPPU.chrMap = romBanks[chrBank];

	// map memory to read-in ROM
	if (numPRGBanks == 1) {
		for (int m = 0x00; m < 0x20; m++) {
			mainCPU.map[m + 0x80] = &romBanks[0][m * 0x100];
			mainCPU.map[m + 0xA0] = &romBanks[1][m * 0x100];
			mainCPU.map[m + 0xC0] = &romBanks[0][m * 0x100];
			mainCPU.map[m + 0xE0] = &romBanks[1][m * 0x100];
		}
	} else {
		DebugAssert(numPRGBanks == 2);

		for (int m = 0x00; m < 0x20; m++) {
			mainCPU.map[m + 0x80] = &romBanks[0][m * 0x100];
			mainCPU.map[m + 0xA0] = &romBanks[1][m * 0x100];
			mainCPU.map[m + 0xC0] = &romBanks[2][m * 0x100];
			mainCPU.map[m + 0xE0] = &romBanks[3][m * 0x100];
		}
	}
}