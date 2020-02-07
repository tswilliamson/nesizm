
#include "platform.h"
#include "debug.h"
#include "nes.h"

// save state support is designed to be partially compatible with FCEUX, provided the states
// from FCEUX are uncompressed

#pragma pack(push,1)
struct FCEUX_Header {
	uint8 FCS[3];
	uint8 OldVersion;
	uint32 Size;
	uint32 NewVersion;
	uint32 CompressedSize;
};
struct FCEUX_SectionHeader {
	uint8 sectionType;
	uint32 sectionSize;
};
struct FCEUX_ChunkHeader {
	char chunkName[4];
	uint32 chunkSize;
};
#pragma pack(pop)

enum SectionTypes {
	ST_CPU = 1,
	ST_CPUC = 2,
	ST_PPU = 3,
	ST_CTLR = 4,
	ST_SND = 5,
	ST_EXTRA = 16
};

#define HandleSubsection(SectionName, SubsectionName, ExpectedSize) \
	if (strcmp(name, #SubsectionName) == 0) { \
		if (size != ExpectedSize) { OutputLog("      Bad Size!");  hasError = true; return false; } \
		Read_##SectionName##_##SubsectionName(data, size); return true; \
	}

struct FCEUX_File {
	int32 fileID;
	bool hasError;
	int32 Size;
	int32 Version;

	FCEUX_File(int withFileID) : fileID(withFileID), hasError(false) {}

	bool ReadHeader() {
		DebugAssert(sizeof(FCEUX_Header) == 16);
		FCEUX_Header header;
		Bfile_ReadFile_OS(fileID, &header, sizeof(header), -1);
		EndianSwap_Little(header.Size);
		EndianSwap_Little(header.NewVersion);
		if (header.FCS[0] != 'F' || header.FCS[1] != 'C' || header.FCS[2] != 'S') {
			return false;
		}
		Size = header.Size;
		Version = header.NewVersion;
		OutputLog("FCEUX savestate: Version %d, Size %u\n", Version, Size);
		return true;
	}

	bool ReadSection(void* scratchMem) {
		FCEUX_SectionHeader header;
		if (Bfile_ReadFile_OS(fileID, &header, sizeof(header), -1) != sizeof(header)) {
			return false;
		}
		EndianSwap_Little(header.sectionSize);

		if (header.sectionType == 8) {
			OutputLog("  Skipping load of state section '%d', %u bytes\n", header.sectionType, header.sectionSize);
			int32 tell = Bfile_TellFile_OS(fileID);
			Bfile_SeekFile_OS(fileID, tell + header.sectionSize);
			return true;
		}

		OutputLog("  Loading state section '%d', %u bytes\n", header.sectionType, header.sectionSize);
		int32 sizeLeft = header.sectionSize;
		while (sizeLeft > 0) {
			FCEUX_ChunkHeader chunkHeader;
			Bfile_ReadFile_OS(fileID, &chunkHeader, sizeof(chunkHeader), -1);
			EndianSwap_Little(chunkHeader.chunkSize);

			char chunkName[5] = { 0 };
			memcpy(chunkName, chunkHeader.chunkName, 4);
			OutputLog("    Chunk '%s', %d bytes\n", chunkName, chunkHeader.chunkSize);

			Bfile_ReadFile_OS(fileID, scratchMem, chunkHeader.chunkSize, -1);

			if (!ReadStateChunk(header.sectionType, chunkName, (uint8*)scratchMem, chunkHeader.chunkSize)) {
				return false;
			}

			sizeLeft -= sizeof(chunkHeader) + chunkHeader.chunkSize;
		}
		OutputLog("  End section '%d'\n", header.sectionType);

		DebugAssert(sizeLeft == 0); // overrun if hit
		return true;
	}

	bool ReadStateChunk(int32 headerType, const char* name, uint8* data, uint32 size) {
		switch (headerType) {
			case ST_CPU:
			{
				HandleSubsection(ST_CPU, PC, 2);
				HandleSubsection(ST_CPU, A, 1);
				HandleSubsection(ST_CPU, P, 1);
				HandleSubsection(ST_CPU, X, 1);
				HandleSubsection(ST_CPU, Y, 1);
				HandleSubsection(ST_CPU, S, 1);
				HandleSubsection(ST_CPU, RAM, 0x800);
				break;
			}
			case ST_PPU:
			{
				HandleSubsection(ST_PPU, NTAR, 0x800);
				HandleSubsection(ST_PPU, PRAM, 32);
				HandleSubsection(ST_PPU, SPRA, 0x100);
				HandleSubsection(ST_PPU, PPUR, 4);
				HandleSubsection(ST_PPU, XOFF, 1);
				HandleSubsection(ST_PPU, VTOG, 1);
				HandleSubsection(ST_PPU, VTGL, 1);
				HandleSubsection(ST_PPU, RADD, 2);
				HandleSubsection(ST_PPU, TADD, 2);
				HandleSubsection(ST_PPU, VBUF, 1);
				HandleSubsection(ST_PPU, PGEN, 1);
				break;
			}
			case ST_EXTRA:
			{
				HandleSubsection(ST_EXTRA, WRAM, 0x2000);
				break;
			}
		}

		OutputLog("      (ignored) ");
		if (size <= 8) {
			for (uint32 i = 0; i < size; i++) {
				OutputLog("%02X ", data[i]);
			}
		}
		OutputLog("\n");
		return true;
	}

	inline void Read_ST_CPU_PC(uint8* data, uint32 size) {
		mainCPU.PC = data[0] + (data[1] << 8);
	}

	inline void Read_ST_CPU_A(uint8* data, uint32 size) {
		mainCPU.A = data[0];
	}

	inline void Read_ST_CPU_P(uint8* data, uint32 size) {
		mainCPU.P = data[0];
		mainCPU.resolveFromP();
	}

	inline void Read_ST_CPU_X(uint8* data, uint32 size) {
		mainCPU.X = data[0];
	}

	inline void Read_ST_CPU_Y(uint8* data, uint32 size) {
		mainCPU.Y = data[0];
	}

	inline void Read_ST_CPU_S(uint8* data, uint32 size) {
		mainCPU.SP = data[0];
	}

	inline void Read_ST_CPU_RAM(uint8* data, uint32 size) {
		memcpy(&mainCPU.RAM[0], data, 0x800);
	}

	// nametable RAM
	inline void Read_ST_PPU_NTAR(uint8* data, uint32 size) {
		nes_nametable* curTable = nesPPU.nameTables;
		while (size) {
			memcpy(curTable, data, 0x400);
			data += 0x400;
			size -= 0x400;
			curTable++;
		}
	}

	// palette memory
	inline void Read_ST_PPU_PRAM(uint8* data, uint32 size) {
		memcpy(nesPPU.palette, data, 32);
		nesPPU.dirtyPalette = true;
	}

	// sprite memory
	inline void Read_ST_PPU_SPRA(uint8* data, uint32 size) {
		memcpy(nesPPU.oam, data, 0x100);
		nesPPU.dirtyOAM = true;
	}

	// ppu registers
	inline void Read_ST_PPU_PPUR(uint8* data, uint32) {
		nesPPU.PPUCTRL = data[0];
		nesPPU.PPUMASK = data[1];
		nesPPU.PPUSTATUS = data[2];
		nesPPU.OAMADDR = data[3];
	}

	inline void Read_ST_PPU_XOFF(uint8* data, uint32) {
		nesPPU.SCROLLX = data[0];
	}

	inline void Read_ST_PPU_VTGL(uint8* data, uint32 size) {
		Read_ST_PPU_VTOG(data, size);
	}
	inline void Read_ST_PPU_VTOG(uint8* data, uint32) {
		nesPPU.writeToggle = data[0] ? 1 : 0;
	}

	inline void Read_ST_PPU_RADD(uint8* data, uint32) {
		nesPPU.ADDRLO = data[0];
		nesPPU.ADDRHI = data[1];
	}

	inline void Read_ST_PPU_TADD(uint8* data, uint32) {
		// unused
	}

	inline void Read_ST_PPU_VBUF(uint8* data, uint32) {
		nesPPU.memoryMap[7] = data[0];
	}

	inline void Read_ST_PPU_PGEN(uint8* data, uint32) {
		// set latched values
		nesPPU.memoryMap[0] = data[0];
		nesPPU.memoryMap[1] = data[0];
		nesPPU.memoryMap[3] = data[0];
		nesPPU.memoryMap[5] = data[0];
		nesPPU.memoryMap[6] = data[0];
	}

	inline void Read_ST_EXTRA_WRAM(uint8* data, uint32 size) {
		nesCart.readState_WRAM(data);
	}
};

// loads the default save state for this cart (filename replaces .nes with .fcs)
bool nes_cart::LoadState() {
	char saveStateFile[256];
	strcpy(saveStateFile, romFile);
	*(strrchr(saveStateFile, '.') + 1) = 0;
	strcat(saveStateFile, "fcs");

	int fileID;
	{
		uint16 saveStateName[256];
		Bfile_StrToName_ncpy(saveStateName, saveStateFile, 255);
		fileID = Bfile_OpenFile_OS(saveStateName, READ, 0);
	}

	if (fileID < 0) {
		return false;
	}

	FCEUX_File fceuxFile(fileID);

	if (!fceuxFile.ReadHeader()) {
		return false;
	}

	void* scratch = nesCart.banks[nesCart.findOldestUnusedBank()];
	while (fceuxFile.ReadSection(scratch)) {
	}

	return fceuxFile.hasError;
}