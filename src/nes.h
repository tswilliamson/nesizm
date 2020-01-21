
#pragma once

// NES emulator specific header

#define NES 1
#include "6502.h"

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

#define NUM_CACHED_ROM_BANKS 20

// specifications about the cart, rom file, mapper, etc
struct nes_cart {
	nes_cart();

	int handle;						// current file handle
	char file[128];					// file name

	int mapper;						// mapper ID used with ROM
	int numPRGBanks;				// num 16 kb Program ROM banks
	int numCHRBanks;				// num 8 kb CHR ROM banks (0 means CHR RAM)
	int numRAMBanks;				// num 8 kb RAM banks

	int isBatteryBacked;			// 0 if no battery backup
	int isPAL;						// 1 if PAL, 0 if NTSC

	// up to 32 internal registers
	unsigned int registers[32];

	// called on all writes over 0x4020
	void(*writeSpecial)(unsigned int address, unsigned char value);

	// called on PPU render reads beyond 0xFD0 in order to last different CHR map (used by MMC2 for Punch Out)
	void(*renderLatch)(unsigned int ppuAddress);

	// called per scanling from PPU if set, used for MMC3
	void(*scanlineClock)();

	// 8kb banks for various usages based on mapper (allocated on stack due to Prizm deficiencies
	unsigned char* banks[NUM_CACHED_ROM_BANKS];	

	// common bank caching set up. Caching is used for PRG and CHR. RAM is stored permanently in memory
	int requestIndex;

	int bankIndex[NUM_CACHED_ROM_BANKS];
	int bankRequest[NUM_CACHED_ROM_BANKS];

	int cachedPRGCount;				// number of 8 KB banks to use for PRG
	int cachedCHRCount;				// number of 8 KB banks to use for CHR

	void clearCacheData();

	// returns whether the bank given is in use by the memory map
	bool isBankUsed(int index);

	// finds least recently requested bank that is currently unused
	int findOldestUnusedBank(int startIndex, int lastIndexExclusive);

	// caches an 8 KB PRG bank (so index up to 2 * numPRGBanks), returns result bank memory pointer
	unsigned char* cachePRGBank(int index);

	// caches an 8 KB CHR bank, returns result bank memory pointer
	unsigned char* cacheCHRBank(int index);

	// sets up bank pointers with the given allocated data
	void allocateBanks(unsigned char* withAlloced);

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

struct ppu_registers_type {
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

	unsigned char* latch;			// cur latch pointer (to the registers above)

	unsigned char* latchReg(unsigned int regNum);
	void writeReg(unsigned int regNum, unsigned char value);

	ppu_registers_type() {
		memset(this, 0, sizeof(ppu_registers_type));
		latch = &PPUCTRL;
	}
};

#define USE_DMA TARGET_PRIZM

// main ppu registers (2000-2007 and emulated latch)
extern ppu_registers_type ppu_registers;

// oam data
extern unsigned char ppu_oam[0x100];

// up to four name tables potentially (most games use 2)
extern nes_nametable* ppu_nameTables;

// palette ram (first 16 bytes are BG, second are OBJ palettes)
extern unsigned char ppu_palette[0x20];

// working palette ram (accounts for background color mirroring). Index 0 set to -1 when dirty
extern int ppu_workingPalette[0x20];

// all 8 kb mapped for character memory at once (0x0000 - 0x2000)
extern unsigned char* ppu_chrMap;

extern void ppu_setMirrorType(int withType);

// perform an OAM dma from the given CPU memory address
void ppu_oamDMA(unsigned int addr);

// reading / writing
extern unsigned char* ppu_resolveMemoryAddress(unsigned int addr, bool mirrorBehindPalette);

extern void ppu_step();

// current scanline (0 = prerender line, 1 = first real scanline)
extern unsigned int ppu_scanline;

// curent frame
extern unsigned int ppu_frameCounter;

// pointer to buffer representing palette entries for current scanline
extern unsigned int* ppu_scanlineBuffer;

// pointer to current 565 color palette (can change due to emphasis bits)
extern unsigned int* ppu_rgbPalettePtr;
extern unsigned int* ppu_rgbPalettePtrShifted;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT

void input_Initialize();

void input_writeStrobe(unsigned char value);

unsigned char input_readController1();

unsigned char input_readController2();