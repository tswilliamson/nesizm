
#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "mappers.h"

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

enum SaveStateError {
	SSE_NoError,
	SSE_BadData,
	SSE_Compressed
};

#define HandleSubsection(SectionName, SubsectionName, ExpectedSize) \
	if (strcmp(name, #SubsectionName) == 0) { \
		if (size != ExpectedSize) { OutputLog("      Bad Size!");  hasError = SSE_BadData; return false; } \
		Read_##SectionName##_##SubsectionName(data, size); return true; \
	}

struct FCEUX_File {
	int32 fileID;
	SaveStateError hasError;
	int32 Size;
	int32 Version;
	int32 WritingToSection;
	int32 SectionSizes[32];

	FCEUX_File(int withFileID) : fileID(withFileID), hasError(SSE_NoError) {
		memset(SectionSizes, 0, sizeof(SectionSizes));
	}

	// file size is Size value + header size
	int32 GetFileSize() {
		return Size + sizeof(FCEUX_Header);
	}

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

		if (header.CompressedSize && header.CompressedSize != 0xFFFFFFFF) {
			OutputLog("Compressed Savestate, not supported\n");
			hasError = SSE_Compressed;
			return false;
		}

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
				HandleSubsection(ST_EXTRA, CHRR, 0x2000);
				// MMC1
				HandleSubsection(ST_EXTRA, DREG, 4);
				HandleSubsection(ST_EXTRA, BFFR, 1);
				HandleSubsection(ST_EXTRA, BFRS, 1);
				// AOROM / UNROM
				HandleSubsection(ST_EXTRA, LATC, 1);
				// MMC3
				HandleSubsection(ST_EXTRA, REGS, 8);
				HandleSubsection(ST_EXTRA, CMD, 1);
				HandleSubsection(ST_EXTRA, A000, 1);
				HandleSubsection(ST_EXTRA, IRQR, 1);
				HandleSubsection(ST_EXTRA, IRQC, 1);
				HandleSubsection(ST_EXTRA, IRQL, 1);
				HandleSubsection(ST_EXTRA, IRQA, 1);
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

		nesPPU.memoryMap[2] = data[2];
		nesPPU.memoryMap[4] = data[3];
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

	void Read_ST_EXTRA_CHRR(uint8* data, uint32 size) {
		if (nesCart.mapper == 2) {
			if (nesCart.numCHRBanks == 0) {
				memcpy(nesPPU.chrPages[0], data, 0x1000);
				memcpy(nesPPU.chrPages[1], data + 0x1000, 0x2000);
			}
		}
	}

	void Read_ST_EXTRA_DREG(uint8* data, uint32 size) {
		// MMC1
		if (nesCart.mapper == 1) {
			// data[0] is the MMC1 control register

			// Mirror mode
			switch (data[0] & 3) {
				case 0:
					nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE);
					break;
				case 1:
					nesPPU.setMirrorType(nes_mirror_type::MT_SINGLE_UPPER);
					break;
				case 2:
					nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
					break;
				case 3:
					nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
					break;
			}

			// PRG BANKS
			unsigned int PRGBankMode = (data[0] & 0x0C) >> 2;
			nesCart.registers[3] = PRGBankMode;

			uint8 selectedPRGBlock = 0;
			if (nesCart.numPRGBanks >= 0x10) {
				selectedPRGBlock = (data[1] & 0x10);
			}
			uint8 prgBank = (data[3] & 0xF) | selectedPRGBlock;
			switch (PRGBankMode) {
			case 0:
			case 1:
				// 32 kb mode
				prgBank &= 0xFE;
				nesCart.registers[7] = prgBank * 2;
				nesCart.registers[8] = prgBank * 2 + 2;
				break;
			case 2:
				// fixed lower
				nesCart.registers[7] = selectedPRGBlock * 2;
				nesCart.registers[8] = prgBank * 2;
				break;
			case 3:
				// fixed upper
				nesCart.registers[7] = prgBank * 2;
				nesCart.registers[8] = (((nesCart.numPRGBanks - 1) & 0xF) | selectedPRGBlock) * 2;
				break;
			}
			nesCart.MapProgramBanks(0, nesCart.registers[7], 2);
			nesCart.MapProgramBanks(2, nesCart.registers[8], 2);

			// CHR BANKS : registers[4] indicates 4 KB mode:
			nesCart.registers[4] = (data[0] & 0x10) ? 1 : 0;
			if (nesCart.registers[4]) {
				// DRegs 1 and 2 for each character selection in 4KB
				nesCart.MapCharacterBanks(0, data[1] * 4, 4);
				nesCart.MapCharacterBanks(4, data[2] * 4, 4);
			} else {
				// 8 KB mode
				nesCart.MapCharacterBanks(0, data[1] * 4, 8);
			}

			// ram enable
			nesCart.MMC1_SetRAMBank(data[3] & 0x10);
		}
	}

	void Read_ST_EXTRA_BFFR(uint8* data, uint32 size) {
		// MMC1
		if (nesCart.mapper == 1) {
			// Buffer
			nesCart.registers[2] = data[0];
		}
	}

	void Read_ST_EXTRA_BFRS(uint8* data, uint32 size) {
		// MMC1
		if (nesCart.mapper == 1) {
			// Buffer shift
			nesCart.registers[1] = data[0];
		}
	}

	void Read_ST_EXTRA_LATC(uint8* data, uint32 size) {
		// UNROM
		if (nesCart.mapper == 2) {
			nesCart.MapProgramBanks(0, data[0] * 2, 2);
		}
		// CNROM
		else if (nesCart.mapper == 3) {
			nesCart.registers[0] = data[0];
			nesCart.MapCharacterBanks(0, data[0] * 8, 8);
		}
	}

	void Read_ST_EXTRA_REGS(uint8* data, uint32 size) {
		// MMC3
		if (nesCart.mapper == 4) {
			// internal MMC3 registers:
			for (int32 r = 0; r < 8; r++) {
				MMC3_BANK_REG[r] = data[r];
			}
		}
	}

	void Read_ST_EXTRA_CMD(uint8* data, uint32 size) {
		// MMC3
		if (nesCart.mapper == 4) {
			MMC3_BANK_SELECT = data[0];
		}
	}

	void Read_ST_EXTRA_A000(uint8* data, uint32 size) {
		// MMC3 
		if (nesCart.mapper == 4) {
			// A000 write (mapper select)
			if (nesPPU.mirror != nes_mirror_type::MT_4PANE) {
				if (data[0] & 1) {
					nesPPU.setMirrorType(nes_mirror_type::MT_HORIZONTAL);
				} else {
					nesPPU.setMirrorType(nes_mirror_type::MT_VERTICAL);
				}
			}
		}
	}

	void Read_ST_EXTRA_IRQR(uint8* data, uint32 size) {
		// MMC3
		if (nesCart.mapper == 4) {
			MMC3_IRQ_RELOAD = data[0];
		}
	}

	void Read_ST_EXTRA_IRQC(uint8* data, uint32 size) {
		// MMC3
		if (nesCart.mapper == 4) {
			MMC3_IRQ_COUNTER = data[0];
		}
	}

	void Read_ST_EXTRA_IRQL(uint8* data, uint32 size) {
		// MMC3
		if (nesCart.mapper == 4) {
			MMC3_IRQ_LATCH = data[0];
		}
	}

	void Read_ST_EXTRA_IRQA(uint8* data, uint32 size) {
		// MMC3
		if (nesCart.mapper == 4) {
			MMC3_IRQ_ENABLE = data[0];
		}
	}

	// write state support
	void StartWrite() {
		Size = 0;
		Version = 22000;  // we are testing against FCEUX v 2.2
	}

	bool WriteState() {
		WriteHeader();

		// ST_CPU
		WriteSection(ST_CPU);
		WriteChunk("PC", 2, mainCPU.PC & 0xFF, mainCPU.PC >> 8);
		WriteChunk("A", 1, mainCPU.A);
		WriteChunk("P", 1, mainCPU.P);
		WriteChunk("X", 1, mainCPU.X);
		WriteChunk("Y", 1, mainCPU.Y);
		WriteChunk("S", 1, mainCPU.SP);
		WriteChunk_Data("RAM", 0x800, &mainCPU.RAM[0]);

		// ST_PPU
		WriteSection(ST_PPU);
		WriteChunk_Data("NTAR", 0x800, nesPPU.nameTables);
		WriteChunk_Data("PRAM", 32, nesPPU.palette);
		WriteChunk_Data("SPRA", 0x100, nesPPU.oam);
		WriteChunk("PPUR", 4, nesPPU.PPUCTRL, nesPPU.PPUMASK, nesPPU.PPUSTATUS, nesPPU.OAMADDR);
		WriteChunk("XOFF", 1, nesPPU.SCROLLX);
		WriteChunk("VTGL", 1, nesPPU.writeToggle);
		WriteChunk("RADD", 2, nesPPU.ADDRLO, nesPPU.ADDRHI);
		WriteChunk("TADD", 2, 0, 0);
		WriteChunk("VBUF", 1, nesPPU.memoryMap[7]);
		WriteChunk("PGEN", 1, nesPPU.memoryMap[0]);

		// ST_EXTRA
		if (nesCart.numRAMBanks) {
			WriteSection(ST_EXTRA);
			WriteChunk_Data("WRAM", 0x2000, nesCart.GetWRAM());

			// MMC1 Mapper
			if (nesCart.mapper == 1) {
				WriteChunk("BFFR", 1, nesCart.registers[2]); // MMC1 shift register
				WriteChunk("BFRS", 1, nesCart.registers[1]); // MMC1 shift register bit

				// build DRegs:
				uint8 DRegs[4] = { 0, 0, 0, 0 };

				switch (nesPPU.mirror) {
					case nes_mirror_type::MT_SINGLE:
						DRegs[0] = 0;
						break;
					case nes_mirror_type::MT_SINGLE_UPPER:
						DRegs[0] = 1;
						break;
					case nes_mirror_type::MT_VERTICAL:
						DRegs[0] = 2;
						break;
					case nes_mirror_type::MT_HORIZONTAL:
						DRegs[0] = 3;
						break;
				}

				// CHR ROM
				if (nesCart.registers[4]) {
					DRegs[0] |= 0x10;
					DRegs[2] = nesCart.chrBanks[4] / 4;
				}
				DRegs[1] = nesCart.chrBanks[0] / 4;

				// PRG ROM
				DRegs[0] |= (nesCart.registers[3] << 2);
				if (nesCart.registers[3] == 2) {
					DRegs[3] = (nesCart.registers[8] / 2) & 0xF;
					if (nesCart.registers[8] & 0x20) DRegs[1] = 0x10;
				} else {
					DRegs[3] = (nesCart.registers[7] / 2) & 0xF;
					if (nesCart.registers[7] & 0x20) DRegs[1] = 0x10;
				}

				if (nesCart.registers[5]) {
					DRegs[3] |= 0x10;
				}
				WriteChunk("DREG", 4, DRegs[0], DRegs[1], DRegs[2], DRegs[3]);
			}
			// UNROM Mapper
			else if (nesCart.mapper == 2) {
				WriteChunk("LATC", 1, nesCart.programBanks[0] / 2);
				if (nesCart.numCHRBanks == 0) {
					DebugAssert(nesPPU.chrPages[0] + 4096 == nesPPU.chrPages[1]);
					WriteChunk_Data("CHRR", 8192, nesPPU.chrPages[0]);
				}
			}
			// CNROM Mapper
			else if (nesCart.mapper == 3) {
				WriteChunk("LATC", 1, nesCart.chrBanks[0] / 8);
			}
			// MMC3 Mapper
			else if (nesCart.mapper == 4) {
				uint8 regs[8];
				for (int32 r = 0; r < 8; r++) {
					regs[r] = MMC3_BANK_REG[r];
				}
				WriteChunk_Data("REGS", 8, regs);
				WriteChunk("CMD", 1, uint8(MMC3_BANK_SELECT));
				WriteChunk("A000", 1, nesPPU.mirror == nes_mirror_type::MT_HORIZONTAL ? 1 : 0);
				WriteChunk("IRQR", 1, uint8(MMC3_IRQ_RELOAD));
				WriteChunk("IRQC", 1, uint8(MMC3_IRQ_COUNTER));
				WriteChunk("IRQL", 1, uint8(MMC3_IRQ_LATCH));
				WriteChunk("IRQA", 1, uint8(MMC3_IRQ_ENABLE));
			}
		}

		return true;
	}

	void WriteHeader() {
		if (fileID) {
			FCEUX_Header toWrite;
			toWrite.FCS[0] = 'F';
			toWrite.FCS[1] = 'C';
			toWrite.FCS[2] = 'S';
			toWrite.OldVersion = 'X';
			toWrite.Size = Size;
			toWrite.NewVersion = Version;
			toWrite.CompressedSize = -1;
			EndianSwap_Little(toWrite.Size);
			EndianSwap_Little(toWrite.NewVersion);
			EndianSwap_Little(toWrite.CompressedSize);
			Bfile_WriteFile_OS(fileID, &toWrite, sizeof(toWrite));
		}
	}

	void WriteSection(SectionTypes section) {
		DebugAssert(section < 32);
		WritingToSection = (int)section;

		if (fileID) {
			FCEUX_SectionHeader toWrite;
			toWrite.sectionType = (uint8)section;
			toWrite.sectionSize = SectionSizes[toWrite.sectionType];
			EndianSwap_Little(toWrite.sectionSize);
			Bfile_WriteFile_OS(fileID, &toWrite, sizeof(toWrite));
		} else {
			Size += sizeof(FCEUX_SectionHeader);
			SectionSizes[WritingToSection] = 0; // online docs say this includes the section size but it doesn't appear to
		}
	}

	void WriteChunk_Data(const char* name, uint32 size, void* dataArray) {
		if (fileID) {
			FCEUX_ChunkHeader toWrite;
			memset(&toWrite.chunkName, 0, sizeof(toWrite.chunkName));
			strcpy(toWrite.chunkName, name);
			toWrite.chunkSize = size;
			EndianSwap_Little(toWrite.chunkSize);
			Bfile_WriteFile_OS(fileID, &toWrite, sizeof(toWrite));
			Bfile_WriteFile_OS(fileID, dataArray, size);
		} else {
			Size += sizeof(FCEUX_ChunkHeader) + size;
			SectionSizes[WritingToSection] += sizeof(FCEUX_ChunkHeader) + size;
		}
	}

	void WriteChunk(const char* name, uint32 size, uint8 data0, uint8 data1 = 0, uint8 data2 = 0, uint8 data3 = 0) {
		uint8 dataArray[4] = { data0, data1, data2, data3 };
		WriteChunk_Data(name, size, dataArray);
	}
};

static void SetStateName(const char* romFile, uint16* intoName, int32 nameSize) {
	char saveStateFile[256];
	strcpy(saveStateFile, romFile);
	*(strrchr(saveStateFile, '.') + 1) = 0;
	strcat(saveStateFile, "fcs");

	Bfile_StrToName_ncpy(intoName, saveStateFile, nameSize-1);
}

// loads the default save state for this cart (filename replaces .nes with .fcs)
bool nes_cart::LoadState() {

	int fileID;
	{
		uint16 saveStateName[256];
		SetStateName(romFile, saveStateName, 256);
		fileID = Bfile_OpenFile_OS(saveStateName, READ, 0);
	}

	if (fileID < 0) {
		return false;
	}

	FCEUX_File fceuxFile(fileID);

	if (!fceuxFile.ReadHeader()) {
		return false;
	}

	void* scratch = nesCart.cache[nesCart.findOldestUnusedBank()].ptr;
	while (fceuxFile.ReadSection(scratch)) {
	}

	Bfile_CloseFile_OS(fileID);

	if (!fceuxFile.hasError) {
		if (mapper == 4) {
			MMC3_StateLoaded();
		}
	}

	return fceuxFile.hasError;
}

bool nes_cart::SaveState() {
	// if the file is set to 0, FCEUX_File will collect sizes instead
	FCEUX_File fceuxFile(0);
	fceuxFile.StartWrite();

	if (!fceuxFile.WriteState())
		return false;

	int32 Size = fceuxFile.GetFileSize();

	int fileID;
	{
		uint16 saveStateName[256];
		SetStateName(romFile, saveStateName, 256);
		int32 result = Bfile_CreateEntry_OS(saveStateName, CREATEMODE_FILE, (size_t*) &Size);
		if (result != 0) {
			return false;
		}

		fileID = Bfile_OpenFile_OS(saveStateName, WRITE, 0);
		if (fileID < 0) {
			return false;
		}
	}

	// prepare data for actual writing
	mainCPU.resolveToP();

	fceuxFile.fileID = fileID;
	bool success = fceuxFile.WriteState();
	Bfile_CloseFile_OS(fileID);
	return success;
}