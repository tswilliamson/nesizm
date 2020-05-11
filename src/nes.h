
#pragma once

// NES emulator specific header

#define NES 1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT

// in report order
enum NesKeys {
	NES_P1_A = 0,
	NES_P1_B,
	NES_P1_SELECT,
	NES_P1_START,
	NES_P1_UP,
	NES_P1_DOWN,
	NES_P1_LEFT,
	NES_P1_RIGHT,
	NES_P1_TURBO_A,
	NES_P1_TURBO_B,
	NES_SAVESTATE,
	NES_LOADSTATE,
	NES_FASTFORWARD,
	NES_VOL_UP,
	NES_VOL_DOWN,
	NES_P2_A,
	NES_P2_B,
	NES_P2_SELECT,
	NES_P2_START,
	NES_P2_UP,
	NES_P2_DOWN,
	NES_P2_LEFT,
	NES_P2_RIGHT,
	NES_MAX_KEYS
};

bool keyDown_fast(unsigned char keyCode);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CART

#define MAX_CACHED_ROM_BANKS 32
#define STATIC_CACHED_ROM_BANKS 24

struct nes_cached_bank {
	unsigned char* ptr;
	int32 request;

	int32 prgIndex;
	int16 chrIndex[8];

	void clear() {
		request = -1;
		prgIndex = -2;
	}
};

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

	// bank index storage for program memory (which 8 KB from cart at 0x8000, 0xA000, 0xC000, amd 0xE000)
	int32 programBanks[4];

	// chr rom index map (which 1 KB from cart at 0x0000 - 0x1FFF in ppu memory)
	int16 chrBanks[8];

	// when chrBanks is dirty, actual bank data is set to the chrPages at the start of the rendered scanline
	bool bDirtyChrBanks;

	// whether chr bank pages should be swapped (for MMC2/3 support)
	bool bSwapChrPages;

	// sets chrPages with banks as specified in chrBanks
	void CommitChrBanks();

	// caches program memory as needed, maps to CPU mem, and updates programBanks[]
	void MapProgramBanks(int32 toBank, int32 cartBank, int32 numBanks);

	// this simply updates chrBanks[], which is then flushed to chrPages on CommitChrBanks()
	void MapCharacterBanks(int32 toBank, int32 cartBank, int32 numBanks);

	// 8kb cached banks for various usages based on mapper (allocated on stack due to Prizm deficiencies
	int availableROMBanks;
	int allocatedROMBanks;
	nes_cached_bank cache[MAX_CACHED_ROM_BANKS];

	// common bank caching set up. Caching is used for PRG and CHR. RAM is stored permanently in memory
	int requestIndex;

	// number of 8 KB banks to use for PRG & CHR (some cached banks are used for permanently mapped RAM, etc)
	int cachedBankCount;

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

	// uncaches all cached block data and resets program banks
	void FlushCache();

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

	// caches a 4 KB CHR bank, returns result bank memory pointer
	unsigned char* cacheCHRBank(int16* indices);

	// sets up bank pointers with the given allocated data
	void allocateBanks(unsigned char* staticAlloced);

	// Loads a ROM from the given file path
	bool loadROM(const char* withFile);

	// unloads ROM if loaded
	void unload();

	// called for when IRQ clocks are reached for mappers that need to reset counters
	bool IRQReached();

	// Sets up loaded ROM File with the selected mapper (returns false if unsupported)
	bool setupMapper();

	// various mapper setups and functions
	void setupMapper0_NROM();

	void setupMapper1_MMC1();
	void MMC1_Write(unsigned int addr, int regValue);
	void MMC1_SetRAMBank(int value);
	void MMC1_StateLoaded();

	void setupMapper2_UNROM();

	void setupMapper3_CNROM();

	void setupMapper4_MMC3();
	void MMC3_UpdateMapping(int regNumber);
	static void MMC3_ScanlineClock();
	void MMC3_StateLoaded();

	void setupMapper7_AOROM();
	void AOROM_StateLoaded();

	void setupMapper9_MMC2();
	void MMC2_StateLoaded();

	void setupMapper11_ColorDreams();

	void setupMapper71_Camerica();

	void setupMapper64_Rambo1();
	void Mapper64_Update();
	static void Mapper64_ScanlineClock();

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

	// character memory split into 4 kb pages (0x0000 and 0x1000)
	unsigned char* chrPages[2];

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
	unsigned int autoFrameSkip;

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
			if (nesCart.bDirtyChrBanks) {
				nesCart.CommitChrBanks();
			}

			// pattern table memory
			return &chrPages[address >> 12][address & 0x0FFF];
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
	void initTV();
	void step();

	// internals
	void resolveWorkingPalette();

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
	void fastSprite0(bool bValidBackground);
	void doOAMRender();
	void resolveScanline(int scrollOffset);
	void finishFrame(bool bSkippedFrame);

	// checks conditions for a sprite hit being possible
	bool canSprite0Hit() {
		return (PPUSTATUS & PPUSTAT_SPRITE0) == 0 && (PPUMASK & (PPUMASK_SHOWOBJ | PPUMASK_SHOWBG));
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
// APU

// pulse wave generator
struct nes_apu_pulse {
	void writeReg(unsigned int regNum, uint8 value);
	void step_quarter();
	void step_half();

	int mixOffset;

	int rawPeriod;
	int lengthCounter;

	int dutyCycle;
	bool enableLengthCounter;

	// volume / envelope
	int constantVolume;
	int envelopeVolume;
	int envelopePeriod;				// in quarter frames
	int envelopeCounter;
	bool useConstantVolume;

	// sweep unit
	bool sweepEnabled;
	int sweepCounter;
	int sweepTargetPeriod;			// current target period of the sweep unit (regardless of being enabled)
	int sweepPeriod;				// in quarter frames	
	bool sweepNegate;				
	int sweepBarrelShift;			// sweep calculation shift bits
	bool sweepTwosComplement;		// set on Pulse 2 but not Pulse 1
};

// triangle wave generator
struct nes_apu_triangle {
	void writeReg(unsigned int regNum, uint8 value);
	void step_quarter();
	void step_half();

	int mixOffset;

	int rawPeriod;
	int lengthCounter;
	int linearCounter;
	int linearPeriod;

	bool enableLengthCounter;
	bool reloadLinearCounter;
};

// noise generator
struct nes_apu_noise {
	void writeReg(unsigned int regNum, uint8 value);
	void step_quarter();
	void step_half();

	int shiftRegister;

	int clocks;
	int samplesPerPeriod;
	int noiseMode;

	int lengthCounter;
	bool enableLengthCounter;

	// volume / envelope
	int constantVolume;
	int envelopeVolume;
	int envelopePeriod;				// in quarter frames
	int envelopeCounter;
	bool useConstantVolume;
};

// delta modulated channel generator
struct nes_apu_dmc {
	void writeReg(unsigned int regNum, uint8 value);
	void bitClear();
	void bitSet();

	void step();

	// current state
	int output;
	int clocks;
	int sampleBuffer;
	int bitCount;
	bool silentFlag;
	unsigned int curSampleAddress;
	unsigned int remainingLength;

	// sample data
	int samplesPerPeriod;
	unsigned int sampleAddress;
	int length;

	// flags
	bool irqEnabled;
	bool loop;
};

// audio processing unit main struct
struct nes_apu {
	nes_apu() {
		init();
	}

	nes_apu_pulse pulse1;
	nes_apu_pulse pulse2;
	nes_apu_triangle triangle;
	nes_apu_noise noise;
	nes_apu_dmc dmc;
	
	int cycle;
	int mode;	// 0 = 4 step, 1 = 5 step

	// frame counter IRQ
	bool inhibitIRQ;
	bool irqFlag;
	bool dmcIRQFlag;
	
	void init();
	void startup();

	// write to APU registers (address is low byte of range $4000-$19)
	void writeReg(unsigned int address, uint8 value);

	// latched the status value (4015)
	void clearFrameIRQ();

	void clearDMCIRQ();

	// apu update step called every at 240 Hz by cpu (nes_cpu.apuClocks)
	void step();

	// step quarter frame counters of generators
	void step_quarter();

	// step half frame counters of generators
	void step_half();

	void mix(int* intoBuffer, int length);

	void shutdown();

};

extern nes_apu nesAPU;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT

void input_Initialize();

void input_cacheKeys();

void input_writeStrobe(unsigned char value);

void input_readController1();
void input_readController2();

#include "6502.h"