
#pragma once

// NES emulator specific header

#define NES 1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT

// in report order
enum NesKeys {
	NES_A = 0,
	NES_B,
	NES_SELECT,
	NES_START,
	NES_UP,
	NES_DOWN,
	NES_LEFT,
	NES_RIGHT,
	NES_TURBO_A,
	NES_TURBO_B,
	NES_SAVESTATE,
	NES_LOADSTATE,
	NES_MAX
};

#define NES_NUM_CONTROLLER_KEYS (NES_TURBO_B + 1)

bool keyDown_fast(unsigned char keyCode);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CART

#define MAX_CACHED_ROM_BANKS 32
#define STATIC_CACHED_ROM_BANKS 24

// specifications about the cart, rom file, mapper, etc
struct nes_cart {
	nes_cart();

	int handle;						// current file handle

	uint32 savHash;					// hash of battery backed RAM contents
	char romFile[128];				// file name, if this is set, then the cart is setup to run 
	char savFile[128];				// cached .sav file name for writing to .SAV on exit

	int mapper;						// mapper ID used with ROM
	int numPRGBanks;				// num 16 kb Program ROM banks
	int numCHRBanks;				// num 8 kb CHR ROM banks (0 means CHR RAM)
	int numRAMBanks;				// num 8 kb RAM banks

	int isBatteryBacked;			// 0 if no battery backup
	int isPAL;						// 1 if PAL, 0 if NTSC

	// up to 32 internal registers
	unsigned int registers[32];

	// bank index storage for program memory (which 8KB from cart at 0x8000, 0xA000, 0xC000, amd 0xE000)
	unsigned int programBanks[4];

	// bank index storage for program memory (which 1KB from cart at 0x0000 - 0x1FFF in ppu memory)
	unsigned int chrBanks[8];

	// caches program memory as needed, maps to CPU mem, and updates programBanks[]
	void MapProgramBanks(int32 toBank, int32 cartBank, int32 numBanks);

	// caches character memory as needed, maps or copies to PPU mem, and updates chrBanks[]
	void MapCharacterBanks(int32 toBank, int32 cartBank, int32 numBanks, bool bDirectMap);

	// 8kb cached banks for various usages based on mapper (allocated on stack due to Prizm deficiencies
	int availableROMBanks;
	int allocatedROMBanks;
	unsigned char* banks[MAX_CACHED_ROM_BANKS];
	int bankIndex[MAX_CACHED_ROM_BANKS];
	int bankRequest[MAX_CACHED_ROM_BANKS];
	void* mappedCPUPtrs[8];

	// common bank caching set up. Caching is used for PRG and CHR. RAM is stored permanently in memory
	int requestIndex;

	int cachedBankCount;			// number of 8 KB banks to use for PRG & CHR (some cached banks are used for extra RAM, etc)

	// returns hash of RAM contents
	uint32 GetRAMHash();

	// called when pausing emulator back to frontend
	void OnPause();

	// called when continuing emulator from the menu (forces a rebuild of ROM file blocks)
	void OnContinue();

	// loads the default save state for this cart (filename replaces .nes with .fcs)
	bool LoadState();

	// writes the save state for this cart. Requires block addresses to be reset
	bool SaveState();

	// direct memory block support (direct memcpy from ROM, prevents OS call to read game data as needed)
	unsigned char* blocks[1024];	
	bool BuildFileBlocks();
	void BlockRead(unsigned char* intoMem, int size, int offset);

	// called on all writes over 0x4020
	void(*writeSpecial)(unsigned int address, unsigned char value);

	// called on PPU render reads beyond 0xFD0 in order to last different CHR map (used by MMC2 for Punch Out)
	void(*renderLatch)(unsigned int ppuAddress);

	// called per scanling from PPU if set, used for MMC3
	void(*scanlineClock)();

	void clearCacheData();

	// returns whether the bank given is in use by the memory map
	bool isBankUsed(int index);

	// finds least recently requested bank that is currently unused
	int findOldestUnusedBank();

	// caches an 8 KB PRG bank (so index up to 2 * numPRGBanks), returns result bank memory pointer
	unsigned char* cachePRGBank(int index);

	// caches an 8 KB CHR bank, returns result bank memory pointer
	unsigned char* cacheCHRBank(int index);

	// sets up bank pointers with the given allocated data
	void allocateBanks(unsigned char* staticAlloced);

	// Loads a ROM from the given file path
	bool loadROM(const char* withFile);

	// unloads ROM if loaded
	void unload();

	// Sets up loaded ROM File with the selected mapper (returns false if unsupported)
	bool setupMapper();

	// various mapper setups and functions
	void setupMapper0_NROM();

	void setupMapper1_MMC1();
	void MMC1_Write(unsigned int addr, int regValue);
	void MMC1_SetRAMBank(int value);

	void setupMapper2_UNROM();

	void setupMapper3_CNROM();

	void setupMapper4_MMC3();
	void MMC3_UpdateMapping(int regNumber);
	static void MMC3_ScanlineClock();

	void setupMapper7_AOROM();

	void setupMapper9_MMC2();

	void setupMapper11_ColorDreams();

	void setupMapper71_Camerica();

	void readState_WRAM(uint8* data);
	uint8* GetWRAM();
};

struct nes_nametable {
	unsigned char table[960];
	unsigned char attr[64];
};

extern nes_cart nesCart;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PPU

#define PPUCTRL_FLIPXTBL    (1 << 0)		// flip the x axis nametable lookup? (doesn't matter for horizontal lookup)
#define PPUCTRL_FLIPYTBL    (1 << 1)		// flip the y axis nametable lookup? (doesn't matter for vertical lookup)
#define PPUCTRL_VRAMINC		(1 << 2)		// 0 = +1, 1 = +32
#define PPUCTRL_OAMTABLE	(1 << 3)		// 1 = 0x1000
#define PPUCTRL_BGDTABLE	(1 << 4)		// 1 = 0x1000
#define PPUCTRL_SPRSIZE		(1 << 5)		// 1 = 8x16
#define PPUCTRL_SLAVE		(1 << 6)
#define PPUCTRL_NMI			(1 << 7)

#define PPUMASK_GRAYSCALE	(1 << 0)
#define PPUMASK_SHOWLEFTBG	(1 << 1)		// if on, show left 8 pixels of background screen
#define PPUMASK_SHOWLEFTOBJ (1 << 2)		// if on, show left 8 pixels of objects screen
#define PPUMASK_SHOWBG		(1 << 3)
#define PPUMASK_SHOWOBJ		(1 << 4)
#define PPUMASK_EMPHRED		(1 << 5)
#define PPUMASK_EMPHGREEN	(1 << 6)
#define PPUMASK_EMPHBLUE	(1 << 7)

#define PPUSTAT_OVERFLOW	(1 << 5)
#define PPUSTAT_SPRITE0		(1 << 6)
#define PPUSTAT_NMI			(1 << 7)

#define OAMATTR_VFLIP		(1 << 7)
#define OAMATTR_HFLIP		(1 << 6)
#define OAMATTR_PRIORITY	(1 << 5)

namespace nes_mirror_type {
	enum {
		MT_UNSET,
		MT_HORIZONTAL,
		MT_VERTICAL,
		MT_SINGLE,
		MT_SINGLE_UPPER,	// same as MT_SINGLE, except nametable is mapped to 0x2400
		MT_4PANE
	};
}

struct nes_ppu {
	// registers (some of them map to $2000-$2007, but this is handled case by case)
	unsigned char PPUCTRL;			// $2000
	unsigned char PPUMASK;			// $2001
	unsigned char PPUSTATUS;		// $2002
	unsigned char OAMADDR;			// $2003
									// unsigned char OAMDATA;		// $2004 (unused with latch behavior)
	unsigned char SCROLLX;			// $2005 (a)
	unsigned char SCROLLY;			// $2005 (b)
	unsigned char ADDRHI;			// $2006 (a)
	unsigned char ADDRLO;			// $2006 (b)
									// unsigned char DATA;			// $2007 (unused with latch behavior)

	// register write toggle
	int writeToggle;

	void prepPPUREAD(unsigned int address);
	void latchedReg(unsigned int addr);
	void writeReg(unsigned int regNum, unsigned char value);

	inline void SetPPUSTATUS(unsigned int value) {
		PPUSTATUS = value;
		memoryMap[2] = value;
	}

	// mirror type
	int mirror;

	// up to four name tables potentially (most games use 2)
	nes_nametable* nameTables;

	// all 8 kb mapped for character memory at once (0x0000 - 0x2000)
	unsigned char* chrMap;

	// current scanline (0 = prerender line, 1 = first real scanline)
	unsigned int scanline;

	// pointer to buffer representing palette entries for current scanline
	uint8* scanlineBuffer;

	// pointer to function to render current scanline to scanline buffer (based on mirror mode)
	void(*renderScanline)(nes_ppu& ppu);

	// palette ram (first 16 bytes are BG, second are OBJ palettes)
	unsigned char palette[0x20];

	// oam data
	unsigned char oam[0x100];
	bool dirtyOAM;
	
	// working palette (actual LCD colors of each palette entry from palette[], accounts for background color mirroring)
	uint16 workingPalette[0x20];
	bool dirtyPalette;

	// current frame
	unsigned int frameCounter;

	void setMirrorType(int withType);

	// pointer to current 565 color palette (TODO: can change due to emphasis bits)
	unsigned short* rgbPalettePtr;

	// perform an OAM dma from the given CPU memory address
	void oamDMA(unsigned int addr);
	template<int spriteSize> void resolveOAM();
	void resolveOAMExternal();
	void fastOAMLatchCheck();

	// reading / writing
	inline unsigned char* resolveMemoryAddress(unsigned int address, bool mirrorBehindPalette) {
		address &= 0x3FFF;
		if (address < 0x2000) {
			// pattern table memory
			return &chrMap[address];
		} else if (address < 0x3F00 || mirrorBehindPalette) {
			// name table memory
			switch (mirror) {
				case nes_mirror_type::MT_HORIZONTAL:
					// $2400 = $2000, and $2c00 = $2800, so ignore bit 10:
					address = (address & 0x03FF) | ((address & 0x800) >> 1);
					break;
				case nes_mirror_type::MT_VERTICAL:
					// $2800 = $2000, and $2c00 = $2400, so ignore bit 11:
					address = address & 0x07FF;
					break;
				case nes_mirror_type::MT_SINGLE:
					// everything is just the first 1 kb
					address = address & 0x03FF;
					break;
				case nes_mirror_type::MT_SINGLE_UPPER:
					// everything is just the 2nd 1 kb
					address = (address & 0x03FF) | 0x0400;
					break;
				case nes_mirror_type::MT_4PANE:
					address = address & 0x0FFF;
					break;
			}
			// this is meant to overflow
			return &nameTables[0].table[address];
		} else {
			// palette memory 
			address &= 0x1F;

			// mirror background color between BG and OBJ types
			if ((address & 0x03) == 0)
				address &= 0x0F;

			return &palette[address];
		}
	}

	void init();
	void step();

	// internals
	inline void resolveWorkingPalette() {
		for (int i = 0; i < 0x20; i++) {
			if (i & 3) {
				workingPalette[i] = rgbPalettePtr[palette[i]];
			} else {
				workingPalette[i] = rgbPalettePtr[palette[0]];
			}
		}

		dirtyPalette = false;
	}

	// internal NMI handling
	bool triggerNMI;
	bool setVBL;

	// scroll handling
	int scrollY;
	bool flipY;
	bool causeDecrement;
	void copyYScrollRegs();
	void midFrameScrollUpdate();

	// memory and mapping
	unsigned char memoryMap[256];

	// main rendering
	void initScanlineBuffer();
	void fastSprite0();
	void doOAMRender();
	void resolveScanline(int scrollOffset);
	void finishFrame();

	// checks conditions for a sprite hit being possible
	bool canSprite0Hit() {
		if ((PPUSTATUS & PPUSTAT_SPRITE0) == 0 && (PPUMASK & (PPUMASK_SHOWOBJ | PPUMASK_SHOWBG))) {
			unsigned int yCoord0 = scanline - oam[0] - 2;
			unsigned int spriteSize = ((PPUCTRL & PPUCTRL_SPRSIZE) == 0) ? 8 : 16;
			return yCoord0 < spriteSize;
		}

		return false;
	}

	static void renderScanline_SingleMirror(nes_ppu& ppu);
	static void renderScanline_HorzMirror(nes_ppu& ppu);
	static void renderScanline_VertMirror(nes_ppu& ppu);
	static void renderScanline_4PaneMirror(nes_ppu& ppu);
};

#define USE_DMA TARGET_PRIZM

// main ppu registers (2000-2007 and emulated latch)
extern nes_ppu nesPPU;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT

void input_Initialize();

void input_cacheKeys();

void input_writeStrobe(unsigned char value);

void input_readController1();
void input_readController2();

#include "6502.h"