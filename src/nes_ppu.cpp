
// Main NES ppu implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"	
#include "settings.h"

#include "snd/snd.h"
#include "scope_timer/scope_timer.h"
#include "frontend.h"

#if TRACE_DEBUG
static unsigned int ppuWriteBreakpoint = 0x10000;
extern void PPUBreakpoint();
#endif

nes_ppu nesPPU;

static uint16 reverseBytelookup[16] = {
	0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
	0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
};

uint16 reverseByte(uint16 b) {
	// Reverse the top and bottom nibble then swap them.
	return (reverseBytelookup[b & 0xF] << 4) | reverseBytelookup[b >> 4];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scanline handling

inline void Rotate_2Bit(uint32& value) {
#if TARGET_WINSIM
	value = (value >> 2) | (value << 30);
#else
	asm volatile inline (
		"rotr %[value]						\n\t"
		"rotr %[value]						\n\t"
		// outputs
		: [value] "+r" (value)
		// inputs
		: "0" (value)
		// clobbers
		: "cc"
	);
#endif
}

inline void CopyOver16(uint8* srcBytes) {
#if TARGET_WINSIM
	memcpy(srcBytes + 16, srcBytes, 16);
#elif TARGET_PRIZM
	asm (
		"mov.l @%[src],r0						\n\t"
		"mov.l r0,@(16,%[src])					\n\t" 
		"mov.l @(4, %[src]),r0					\n\t"
		"mov.l r0,@(20,%[src])					\n\t" 
		"mov.l @(8, %[src]),r0					\n\t"
		"mov.l r0,@(24,%[src])					\n\t" 
		"mov.l @(12, %[src]),r0					\n\t"
		"mov.l r0,@(28,%[src])					\n\t" 
		// outputs
		: 
		// inputs
		: [src] "r" (srcBytes)
		// clobbers
		: "r0"
	);
#endif
}

// scanline buffers with padding to allow fast rendering with clipping
static unsigned char oamScanlineBuffer[256 + 8] = { 0 }; 

void nes_ppu::copyYScrollRegs() {
	scrollY = SCROLLY;
	flipY = (PPUCTRL & PPUCTRL_FLIPYTBL) != 0;
	causeDecrement = false;
}

void nes_ppu::midFrameScrollUpdate() {
	bool renderingDisabled = (PPUMASK & PPUMASK_SHOWBG) == 0;

	// update scrollY to account for current scanline offset
	int actualScrollY = scrollY;
	if (actualScrollY >= 240) {
		actualScrollY -= 256;
	}
	if (mainCPU.ppuClocks > mainCPU.clocks && mainCPU.ppuClocks - mainCPU.clocks >= 50 && !renderingDisabled) {
		actualScrollY -= scanline - 2;
	} else {
		actualScrollY -= scanline - 1;
	}

	if (actualScrollY < 0) {
		flipY = !flipY;
		actualScrollY += 240;
	}

	scrollY = actualScrollY;

	// cause scroll decrement if background rendering is disabled
	causeDecrement = renderingDisabled;
}

void nes_ppu::prepPPUREAD(unsigned int address) {
	if (address < 0x3F00) {
		memoryMap[7] = *resolveMemoryAddress(address, false);
	} else {
		// palette special case
		memoryMap[7] = *resolveMemoryAddress(address, true);
	}
}

void nes_ppu::latchedReg(unsigned int addr) {
	// if this occurs we will need to add mirroring into our latched PPU memory map (an extra 16 clks or so)
	DebugAssert(addr < 0x2008);

	addr = addr & 0x7;
	if (addr == 0x02) {
		DebugAssert(PPUSTATUS == memoryMap[2]);
		unsigned char val = PPUSTATUS;

		// set latched values
		memoryMap[0] = val;
		memoryMap[1] = val;
		memoryMap[3] = val;
		memoryMap[5] = val;
		memoryMap[6] = val;

		// suppress NMI on the exact clock cycle in the PPU it may be triggered (some games depend on this)
		if (scanline == 243 && triggerNMI) {
			if (mainCPU.clocks + 1 == mainCPU.ppuClocks) {
				// one before, so donn't set the VBL as well
				setVBL = false;
				triggerNMI = false;
			} else if (mainCPU.clocks == mainCPU.ppuClocks) {
				triggerNMI = false;
			}
		}

		// vblank flag cleared after read
		SetPPUSTATUS(PPUSTATUS & (~PPUSTAT_NMI));

		// switch write toggle to 0
		writeToggle = 0;
	} else if (addr == 0x07) {
		unsigned int address = ((ADDRHI << 8) | ADDRLO) & 0x3FFF;
		unsigned int newAddress = address;

		// auto increment
		if (PPUCTRL & PPUCTRL_VRAMINC) {
			newAddress += 32;
		} else {
			newAddress += 1;
		}
		ADDRHI = (newAddress & 0xFF00) >> 8;
		ADDRLO = (newAddress & 0xFF);

		// set latched values
		unsigned char val = memoryMap[7];
		memoryMap[0] = val;
		memoryMap[1] = val;
		memoryMap[3] = val;
		memoryMap[5] = val;
		memoryMap[6] = val;

		// latch previous address for next read unless we are in the palette memory
		prepPPUREAD(address < 0x3F00 ? address : newAddress);
	} else if (addr == 0x04) {
		unsigned char val = oam[OAMADDR];

		// set latched values
		memoryMap[0] = val;
		memoryMap[1] = val;
		memoryMap[3] = val;
		memoryMap[5] = val;
		memoryMap[6] = val;
	} 

	// the rest are write only registers, so no action needed
	// case 0x00:  PPUCTRL
	// case 0x01:  PPUMASK
	// case 0x03:  OAMADDR
	// case 0x05:  SCROLLX/Y
	// case 0x06:  ADDRHI/LO
}

void nes_ppu::writeReg(unsigned int regNum, unsigned char value) {
	switch (regNum) {
		case 0x00:	// PPUCTRL
			if ((PPUCTRL & PPUCTRL_NMI) == 0 && (value & PPUCTRL_NMI) && (PPUSTATUS & PPUSTAT_NMI)) {
				mainCPU.ppuNMI = true;
				mainCPU.nextClocks = mainCPU.clocks + 1;	// force an NMI check AFTER the next instruction
			}
			PPUCTRL = value;
			break;
		case 0x01:	// PPUMASK
			if (value != PPUMASK) {
				if ((value ^ PPUMASK) & (PPUMASK_EMPHRED | PPUMASK_EMPHGREEN | PPUMASK_EMPHBLUE)) {
					// emphasis bits changed, invalidate the palette
					dirtyPalette = true;
				}

				PPUMASK = value;
			}
			break;
		case 0x02:  
			SetPPUSTATUS(value);
			return;
		case 0x03:  // OAMADDR
			OAMADDR = value;
			memoryMap[4] = oam[OAMADDR];
			break;
		case 0x04:
			// force unused bits to be low when appropriate
			if ((OAMADDR & 3) == 2) {
				value = value & 0xE3;
			}
			if (value != oam[OAMADDR]) {
				oam[OAMADDR] = value;
				dirtyOAM = true;
			}
			OAMADDR++;	// writes to OAM data increment the OAM address
			memoryMap[4] = oam[OAMADDR];
			break;
		case 0x05:  // SCROLLX/Y
			if (writeToggle == 1) {
				SCROLLY = value;
			} else {
				SCROLLX = value;
			}
			writeToggle = 1 - writeToggle;
			break;
		case 0x06:  // ADDRHI/LO
		{
			// address writes always effect nametable and scroll bits as well, and will copy result mid-render

			if (writeToggle == 1) {
				// effects coarse scrollX, and scrollY, and applies write to Y scroll mid frame if need be
				if (scanline > 1 && scanline < 241) {
					SCROLLX = (SCROLLX & 0x07) | ((value & 0x1F) << 3);
					SCROLLY = (SCROLLY & 0xC7) | ((value & 0xE0) >> 2);
					copyYScrollRegs();
					midFrameScrollUpdate();
				}
				ADDRLO = value;
			} else {
				// nametable bits in first write
				PPUCTRL = (PPUCTRL & 0xFC) | ((value & 0x0C) >> 2);
				if (scanline > 1 && scanline < 241) {
					// also scroll Y a bit weirdly
					SCROLLY = (SCROLLY & 0x38) | ((value & 0x03) << 6) | ((value & 0x30) >> 4);
				}
				ADDRHI = value;
			}

			writeToggle = 1 - writeToggle;

			int newAddress = ((ADDRHI << 8) | ADDRLO) & 0x3FFF;
			// palette available immediately
			if (newAddress >= 0x3F00) {
				prepPPUREAD(newAddress);
			}
			break;
		}
		case 0x07:
		{
			unsigned int address = ((ADDRHI << 8) | ADDRLO) & 0x3FFF;

			if (address >= 0x3F00) {
				// dirty palette
				dirtyPalette = true;
				value = value & 0x3F; // mask palette values
			}

			// discard writes to CHR ROM when it is ROM
			if (address < 0x2000 && nesCart.numCHRBanks) {
				break;
			}

			// address will be incremented after the instruction due to latching
			*resolveMemoryAddress(address, false) = value;

#if TRACE_DEBUG
			if (address - ((PPUCTRL & PPUCTRL_VRAMINC) ? 32 : 1) == ppuWriteBreakpoint) {
				PPUBreakpoint();
			}
#endif

			break;
		}
	}
	
	// least significant bits previously written will end up in PPUSTATUS when read
	SetPPUSTATUS((PPUSTATUS & 0xE0) | (value & 0x1F));
}

void nes_ppu::oamDMA(unsigned int addr) {
	// perform the oam DMA
	for (int i = 0; i < 256; i++, addr++) {
		oam[i] = mainCPU.read(addr);
	}

	// force unused attribute bits low
	for (int i = 2; i < 256; i += 4) {
		oam[i] &= 0xE3;
	}

	dirtyOAM = true;
	mainCPU.clocks += 513 + (mainCPU.clocks & 1);
}

void nes_ppu::fastSprite0(bool bValidBackground) {
	DebugAssert(canSprite0Hit()); // should have already been checked

	// simulate the 8 bits from the write, but a little faster
	unsigned int yCoord = scanline - oam[0] - 2;
	unsigned int spriteSize = ((PPUCTRL & PPUCTRL_SPRSIZE) == 0) ? 8 : 16;
	if (yCoord >= spriteSize)
		return;

	if (oam[2] & OAMATTR_VFLIP) yCoord = (spriteSize - 1) - yCoord;

	unsigned char* patternTable = chrPages[((spriteSize == 8 && (PPUCTRL & PPUCTRL_OAMTABLE)) ? 1 : 0)];

	// determine tile index
	unsigned char* tile;
	if (spriteSize == 16) {
		tile = chrPages[oam[1] & 1] + ((oam[1] & 0xFE) << 4) + ((yCoord & 8) << 1) + (yCoord & 7);
	} else {
		tile = patternTable + (oam[1] << 4) + yCoord;
	}

	// interleave the bit planes and assign to char buffer (only unmapped pixels)
	const uint8 tile0 = tile[0];
	const uint8 tile8 = tile[8];
	uint16 tileMask = (tile0 | tile8);
	unsigned int spriteX = oam[3];

	if (tileMask && oam[3] != 255) {
		if (!(oam[2] & OAMATTR_HFLIP)) {
			tileMask = reverseByte(tileMask);
		}

		const uint8 LeftMask = (PPUMASK_SHOWLEFTBG | PPUMASK_SHOWLEFTOBJ);
		if ((PPUMASK & LeftMask) != LeftMask) {
			for (int x = spriteX, bit = 1; x < 8; x++, bit <<= 1) {
				tileMask = tileMask & ~bit;
			}
		}

		if (bValidBackground) {
			int baseX = SCROLLX & 15;
			uint8* buffer = &scanlineBuffer[baseX + spriteX];
			for (int32 x = 0; x < 8; x++, buffer++) {
				if ((tileMask & 1) && (*buffer)) {
					SetPPUSTATUS(PPUSTATUS | PPUSTAT_SPRITE0);
					break;
				}
				tileMask >>= 1;
			}
		} else if (tileMask) {
			SetPPUSTATUS(PPUSTATUS | PPUSTAT_SPRITE0);
		}
	}
}

void nes_ppu::fastOAMLatchCheck() {
	unsigned char* curObj = &oam[252];

	bool sprite16 = (PPUCTRL & PPUCTRL_SPRSIZE);
	unsigned int patternOffset = ((!sprite16 && (PPUCTRL & PPUCTRL_OAMTABLE)) ? 0x1000 : 0x0000);

	int furthestRight = -1;
	int furthestLatch = 0;
	for (int i = 0; i < 64; i++) {
		unsigned int yCoord = scanline - curObj[0] - 2;
		if (yCoord == 0) {
			// determine tile index
			unsigned int tileMem;
			if (sprite16) {
				tileMem = ((((curObj[1] & 1) << 8) + (curObj[1] & 0xFE)) << 4);
			} else {
				tileMem = (curObj[1] << 4);
			}

			if (tileMem >= 0xFD0 && tileMem <= 0xFE0) {
				if (curObj[3] > furthestRight) {
					furthestRight = curObj[3];
					furthestLatch = tileMem + patternOffset + 8;
				}
			}
		}

		curObj -= 4;
	}

	if (furthestLatch) {
		nesCart.renderLatch(furthestLatch);
	}
}

// clocks per scanline formula: (341 / 3) + (scanline % 3 != 0 ? 1 : 0) for NTSC;
char scanlineClocks[245];

void nes_ppu::step() {
	TIME_SCOPE_NAMED("PPU Step");
	
	// calculated once per frame on scanline 1
	static bool skipFrame = false;

	// cpu time for next scanline
	DebugAssert(scanline < 245);
	mainCPU.ppuClocks += scanlineClocks[scanline];
    
	/*
		262 scanlines, we render 9-232 (middle 224 screen lines)

		The idea is to render each scanline at the beginning of its line, and then
		to get sprite 0 timing right use a different ppuClocks value / update function
	*/
	if (scanline == 1) {
		const int32 frameSkipValue = nesSettings.GetSetting(ST_FrameSkip);
		switch (frameSkipValue) {
			case 0: // auto
			{
				unsigned int f = (frameCounter % 32);
				f = ((f & 0x10) >> 4) | ((f & 0x8) >> 2) | ((f & 0x2) << 2) | ((f & 0x01) << 4);

				skipFrame = f < autoFrameSkip;
				break;
			}
			case 1: // none
				skipFrame = false;
				break;
			case 2:
			case 3:
			case 4:
			case 5:
				skipFrame = (frameCounter % frameSkipValue) != 0;
		}

		if (nesSettings.CheckCachedKey(NES_FASTFORWARD)) {
			skipFrame = (frameCounter & 7) != 0;
		}

		static bool bWasVolumeUp = false;
		if (nesSettings.CheckCachedKey(NES_VOL_UP)) {
			if (!bWasVolumeUp) {
				sndVolumeUp();
			}
			bWasVolumeUp = true;
		} else {
			bWasVolumeUp = false;
		}

		static bool bWasVolumeDown = false;
		if (nesSettings.CheckCachedKey(NES_VOL_DOWN)) {
			if (!bWasVolumeDown) {
				sndVolumeDown();
			}
			bWasVolumeDown = true;
		} else {
			bWasVolumeDown = false;
		}

		// clear vblank and sprite 0 flag
		SetPPUSTATUS(PPUSTATUS & ~(PPUSTAT_NMI | PPUSTAT_SPRITE0));		

		// time to copy y scroll regs
		copyYScrollRegs();

		if (nesCart.scanlineClock) {
			nesCart.scanlineClock();
		}
	} else if (scanline < 9) {
		if (nesCart.bDirtyChrBanks) {
			nesCart.CommitChrBanks();
		}

		// non-resolved but active scanline (may cause sprite 0 collision)
		if (canSprite0Hit()) {
			if (!skipFrame) {
				renderScanline(*this);
			} else {
				fastSprite0(false);
			}
		}

		// MMC2/4 support
		if (nesCart.renderLatch) {
			fastOAMLatchCheck();
		}

		if (nesCart.scanlineClock) {
			nesCart.scanlineClock();
		}
	} else if (scanline < 233) {
		if (nesCart.bDirtyChrBanks) {
			nesCart.CommitChrBanks();
		}

		// rendered scanline
		if (!skipFrame) {
			renderScanline(*this);

			if (dirtyPalette) {
				resolveWorkingPalette();
			}

			resolveScanline(SCROLLX & 15);
		} else if (canSprite0Hit()) {
			fastSprite0(false);
		}

		if (nesCart.scanlineClock) {
			nesCart.scanlineClock();
		}
	} else if (scanline < 241) {
		if (nesCart.bDirtyChrBanks) {
			nesCart.CommitChrBanks();
		}

		// non-resolved scanline
		if (canSprite0Hit()) {
			if (!skipFrame) {
				renderScanline(*this);
			} else {
				fastSprite0(false);
			}
		}

		if (nesCart.scanlineClock && scanline != 240) {
			nesCart.scanlineClock();
		}
	} else if (scanline == 241) {
		// good time to reset scroll
		scrollY = 0;

		// blank scanline area, trigger NMI next scanline
		triggerNMI = (mainCPU.clocks > 140000);
		setVBL = (mainCPU.clocks > 30000);
	} else if (scanline == 242) {
		if (setVBL) {
			SetPPUSTATUS(PPUSTATUS | PPUSTAT_NMI);
		}
		if ((PPUCTRL & PPUCTRL_NMI) && triggerNMI) {
			mainCPU.ppuNMI = true;
		}

		// update background color if we are in that BG mode
		if (nesSettings.GetSetting(ST_Background) == 2 && skipFrame == false) {
			unsigned short curColor = workingPalette[0];
			if (curColor != currentBGColor) {
				currentBGColor = curColor;
				nesSettings.cachedTime = -1; // if we are rendering a clock, it needs to be invalidated
				renderBGOverscan();
			}
		}

		// update system clock if we requested on screen
		if (nesSettings.GetSetting(ST_ShowClock) && skipFrame == false) {
			unsigned int hour = 0, minute = 0, second = 0, ms = 0;
			RTC_GetTime(&hour, &minute, &second, &ms);
			int curTime = hour * 256 + minute;
			if (curTime != nesSettings.cachedTime) {
				nesSettings.cachedTime = curTime;
				renderClock();
			}
		}

		// update the FPS counter
		if (nesSettings.GetSetting(ST_ShowFPS) && skipFrame == false) {
			// track fps in 8 half second buckets
			static int32 frameBuckets[8] = { 0, 0, 0, 0, 0, 0, 0, 0};
			static int32 curBucket = 0;
			static int32 lastTicks = 0;
			frameBuckets[curBucket]++;

			int32 ticks = RTC_GetTicks();
			if (ticks % 64 < lastTicks) {
				curBucket = (curBucket + 1) % 8;
				int totalFrames = 2; // rounding
				for (int x = 0; x < 8; x++) {
					totalFrames += frameBuckets[x];
				}
				totalFrames /= 4; // calculated FPS
				if (totalFrames < 1) totalFrames = 1;
				if (totalFrames > 240) totalFrames = 240;
				renderFPS(totalFrames);
				frameBuckets[curBucket] = 0;
			}
			lastTicks = ticks % 64;
		}

		finishFrame(skipFrame);

		ScopeTimer::ReportFrame();

		frameCounter++;

		bool keyDown_fast(unsigned char keyCode);
		if (keyDown_fast(48)) // Menu
		{
			extern bool shouldExit;
			shouldExit = true;
			while (keyDown_fast(48)) {}
		}

#if DEBUG
		if (keyDown_fast(69)) // F2
		{
			ScopeTimer::DisplayTimes();
		}
#endif

		if (keyDown_fast(10)) // AC/ON
		{
			nesFrontend.ResetPressed();
		}

		input_cacheKeys();

		if (nesSettings.CheckCachedKey(NES_SAVESTATE)) // F3 in simulator, 'S" on device
		{
			nesCart.SaveState();
			nesCart.BuildFileBlocks();
		}

		if (nesSettings.CheckCachedKey(NES_LOADSTATE)) // F4 in simulator, 'L' on device
		{
			nesCart.LoadState();
		}

		// good time to synchronize cpu clock if we are getting too high (to avoid wraparound at 30 min of play)
		mainCPU.syncClocks();

	} else if (scanline == 243) {
		// frame is over, don't run until scanline 262, so add 18 scanlines worth (2047 extra clocks!)
		if (nesCart.isPAL == 0) {
			mainCPU.ppuClocks += 18 * (341 / 3) + 12;
		} else {
			mainCPU.ppuClocks += (68 * 1705) / 16;
		}

	} else {
		// final scanline 
		scanline = 0;

		// frame timing .. total ppu frame should be every 29780.5 ppu clocks
		if (nesCart.isPAL == 0) {
			mainCPU.ppuClocks -= 1;
		}

		// one scanline "ahead"
		if (nesCart.scanlineClock) {
			nesCart.scanlineClock();
		}
	}

	scanline++;

	condSoundUpdate();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scanline Rendering (to buffer)

// 16 byte overlay for bit plane -> 8 bit palette plane

// Credit to Stanford bit twiddling hacks
static const unsigned short MortonTable[256] = 
{
  0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015, 
  0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055, 
  0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 
  0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155, 
  0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415, 
  0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455, 
  0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515, 
  0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555, 
  0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015, 
  0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055, 
  0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115, 
  0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155, 
  0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415, 
  0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455, 
  0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 
  0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555, 
  0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015, 
  0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055, 
  0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115, 
  0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155, 
  0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415, 
  0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455, 
  0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515, 
  0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555, 
  0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015, 
  0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055, 
  0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 
  0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155, 
  0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415, 
  0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455, 
  0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515, 
  0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555
};

// table of 8 byte wide bit overlays per all possible 256 byte combinations
const uint8 OverlayTable[8 * 256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,2,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0,2,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2,2,
	0,0,0,0,2,0,0,0,0,0,0,0,2,0,0,2,0,0,0,0,2,0,2,0,0,0,0,0,2,0,2,2,0,0,0,0,2,2,0,0,0,0,0,0,2,2,0,2,0,0,0,0,2,2,2,0,0,0,0,0,2,2,2,2,
	0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,2,0,0,0,2,0,0,2,0,0,0,0,2,0,0,2,2,0,0,0,2,0,2,0,0,0,0,0,2,0,2,0,2,0,0,0,2,0,2,2,0,0,0,0,2,0,2,2,2,
	0,0,0,2,2,0,0,0,0,0,0,2,2,0,0,2,0,0,0,2,2,0,2,0,0,0,0,2,2,0,2,2,0,0,0,2,2,2,0,0,0,0,0,2,2,2,0,2,0,0,0,2,2,2,2,0,0,0,0,2,2,2,2,2,
	0,0,2,0,0,0,0,0,0,0,2,0,0,0,0,2,0,0,2,0,0,0,2,0,0,0,2,0,0,0,2,2,0,0,2,0,0,2,0,0,0,0,2,0,0,2,0,2,0,0,2,0,0,2,2,0,0,0,2,0,0,2,2,2,
	0,0,2,0,2,0,0,0,0,0,2,0,2,0,0,2,0,0,2,0,2,0,2,0,0,0,2,0,2,0,2,2,0,0,2,0,2,2,0,0,0,0,2,0,2,2,0,2,0,0,2,0,2,2,2,0,0,0,2,0,2,2,2,2,
	0,0,2,2,0,0,0,0,0,0,2,2,0,0,0,2,0,0,2,2,0,0,2,0,0,0,2,2,0,0,2,2,0,0,2,2,0,2,0,0,0,0,2,2,0,2,0,2,0,0,2,2,0,2,2,0,0,0,2,2,0,2,2,2,
	0,0,2,2,2,0,0,0,0,0,2,2,2,0,0,2,0,0,2,2,2,0,2,0,0,0,2,2,2,0,2,2,0,0,2,2,2,2,0,0,0,0,2,2,2,2,0,2,0,0,2,2,2,2,2,0,0,0,2,2,2,2,2,2,
	0,2,0,0,0,0,0,0,0,2,0,0,0,0,0,2,0,2,0,0,0,0,2,0,0,2,0,0,0,0,2,2,0,2,0,0,0,2,0,0,0,2,0,0,0,2,0,2,0,2,0,0,0,2,2,0,0,2,0,0,0,2,2,2,
	0,2,0,0,2,0,0,0,0,2,0,0,2,0,0,2,0,2,0,0,2,0,2,0,0,2,0,0,2,0,2,2,0,2,0,0,2,2,0,0,0,2,0,0,2,2,0,2,0,2,0,0,2,2,2,0,0,2,0,0,2,2,2,2,
	0,2,0,2,0,0,0,0,0,2,0,2,0,0,0,2,0,2,0,2,0,0,2,0,0,2,0,2,0,0,2,2,0,2,0,2,0,2,0,0,0,2,0,2,0,2,0,2,0,2,0,2,0,2,2,0,0,2,0,2,0,2,2,2,
	0,2,0,2,2,0,0,0,0,2,0,2,2,0,0,2,0,2,0,2,2,0,2,0,0,2,0,2,2,0,2,2,0,2,0,2,2,2,0,0,0,2,0,2,2,2,0,2,0,2,0,2,2,2,2,0,0,2,0,2,2,2,2,2,
	0,2,2,0,0,0,0,0,0,2,2,0,0,0,0,2,0,2,2,0,0,0,2,0,0,2,2,0,0,0,2,2,0,2,2,0,0,2,0,0,0,2,2,0,0,2,0,2,0,2,2,0,0,2,2,0,0,2,2,0,0,2,2,2,
	0,2,2,0,2,0,0,0,0,2,2,0,2,0,0,2,0,2,2,0,2,0,2,0,0,2,2,0,2,0,2,2,0,2,2,0,2,2,0,0,0,2,2,0,2,2,0,2,0,2,2,0,2,2,2,0,0,2,2,0,2,2,2,2,
	0,2,2,2,0,0,0,0,0,2,2,2,0,0,0,2,0,2,2,2,0,0,2,0,0,2,2,2,0,0,2,2,0,2,2,2,0,2,0,0,0,2,2,2,0,2,0,2,0,2,2,2,0,2,2,0,0,2,2,2,0,2,2,2,
	0,2,2,2,2,0,0,0,0,2,2,2,2,0,0,2,0,2,2,2,2,0,2,0,0,2,2,2,2,0,2,2,0,2,2,2,2,2,0,0,0,2,2,2,2,2,0,2,0,2,2,2,2,2,2,0,0,2,2,2,2,2,2,2,
	2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,2,2,0,0,0,0,0,2,0,2,0,0,0,0,0,2,2,2,0,0,0,0,2,0,0,2,0,0,0,0,2,0,2,2,0,0,0,0,2,2,0,2,0,0,0,0,2,2,2,
	2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,2,2,0,0,0,2,0,2,0,2,0,0,0,2,0,2,2,2,0,0,0,2,2,0,0,2,0,0,0,2,2,0,2,2,0,0,0,2,2,2,0,2,0,0,0,2,2,2,2,
	2,0,0,2,0,0,0,0,2,0,0,2,0,0,0,2,2,0,0,2,0,0,2,0,2,0,0,2,0,0,2,2,2,0,0,2,0,2,0,0,2,0,0,2,0,2,0,2,2,0,0,2,0,2,2,0,2,0,0,2,0,2,2,2,
	2,0,0,2,2,0,0,0,2,0,0,2,2,0,0,2,2,0,0,2,2,0,2,0,2,0,0,2,2,0,2,2,2,0,0,2,2,2,0,0,2,0,0,2,2,2,0,2,2,0,0,2,2,2,2,0,2,0,0,2,2,2,2,2,
	2,0,2,0,0,0,0,0,2,0,2,0,0,0,0,2,2,0,2,0,0,0,2,0,2,0,2,0,0,0,2,2,2,0,2,0,0,2,0,0,2,0,2,0,0,2,0,2,2,0,2,0,0,2,2,0,2,0,2,0,0,2,2,2,
	2,0,2,0,2,0,0,0,2,0,2,0,2,0,0,2,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2,2,2,0,2,0,2,2,0,0,2,0,2,0,2,2,0,2,2,0,2,0,2,2,2,0,2,0,2,0,2,2,2,2,
	2,0,2,2,0,0,0,0,2,0,2,2,0,0,0,2,2,0,2,2,0,0,2,0,2,0,2,2,0,0,2,2,2,0,2,2,0,2,0,0,2,0,2,2,0,2,0,2,2,0,2,2,0,2,2,0,2,0,2,2,0,2,2,2,
	2,0,2,2,2,0,0,0,2,0,2,2,2,0,0,2,2,0,2,2,2,0,2,0,2,0,2,2,2,0,2,2,2,0,2,2,2,2,0,0,2,0,2,2,2,2,0,2,2,0,2,2,2,2,2,0,2,0,2,2,2,2,2,2,
	2,2,0,0,0,0,0,0,2,2,0,0,0,0,0,2,2,2,0,0,0,0,2,0,2,2,0,0,0,0,2,2,2,2,0,0,0,2,0,0,2,2,0,0,0,2,0,2,2,2,0,0,0,2,2,0,2,2,0,0,0,2,2,2,
	2,2,0,0,2,0,0,0,2,2,0,0,2,0,0,2,2,2,0,0,2,0,2,0,2,2,0,0,2,0,2,2,2,2,0,0,2,2,0,0,2,2,0,0,2,2,0,2,2,2,0,0,2,2,2,0,2,2,0,0,2,2,2,2,
	2,2,0,2,0,0,0,0,2,2,0,2,0,0,0,2,2,2,0,2,0,0,2,0,2,2,0,2,0,0,2,2,2,2,0,2,0,2,0,0,2,2,0,2,0,2,0,2,2,2,0,2,0,2,2,0,2,2,0,2,0,2,2,2,
	2,2,0,2,2,0,0,0,2,2,0,2,2,0,0,2,2,2,0,2,2,0,2,0,2,2,0,2,2,0,2,2,2,2,0,2,2,2,0,0,2,2,0,2,2,2,0,2,2,2,0,2,2,2,2,0,2,2,0,2,2,2,2,2,
	2,2,2,0,0,0,0,0,2,2,2,0,0,0,0,2,2,2,2,0,0,0,2,0,2,2,2,0,0,0,2,2,2,2,2,0,0,2,0,0,2,2,2,0,0,2,0,2,2,2,2,0,0,2,2,0,2,2,2,0,0,2,2,2,
	2,2,2,0,2,0,0,0,2,2,2,0,2,0,0,2,2,2,2,0,2,0,2,0,2,2,2,0,2,0,2,2,2,2,2,0,2,2,0,0,2,2,2,0,2,2,0,2,2,2,2,0,2,2,2,0,2,2,2,0,2,2,2,2,
	2,2,2,2,0,0,0,0,2,2,2,2,0,0,0,2,2,2,2,2,0,0,2,0,2,2,2,2,0,0,2,2,2,2,2,2,0,2,0,0,2,2,2,2,0,2,0,2,2,2,2,2,0,2,2,0,2,2,2,2,0,2,2,2,
	2,2,2,2,2,0,0,0,2,2,2,2,2,0,0,2,2,2,2,2,2,0,2,0,2,2,2,2,2,0,2,2,2,2,2,2,2,2,0,0,2,2,2,2,2,2,0,2,2,2,2,2,2,2,2,0,2,2,2,2,2,2,2,2,
};

#if TARGET_WINSIM
// super fast blitting method!
inline void RenderToScanline(unsigned char*patternTable, int chr, uint32 unrolledPalette, uint8* buffer) {
	DebugAssert((uint32(buffer) & 3) == 0); // long alignment required in SH4

	unsigned int* bitPlane1 = (unsigned int*) &OverlayTable[patternTable[chr] * 8];
	unsigned int* bitPlane2 = (unsigned int*) &OverlayTable[patternTable[chr + 8] * 8];
	unsigned int* scanline = (unsigned int*) buffer;
	scanline[0] = unrolledPalette | bitPlane1[0] | (bitPlane2[0] << 1);
	scanline[1] = unrolledPalette | bitPlane1[1] | (bitPlane2[1] << 1);
}
#else
extern "C" {
	void RenderToScanline(unsigned char*patternTable, int chr, uint32 unrolledPalette, uint8* buffer);
}
#endif

// mask of which sprites to check per scanline (bit 0 = sprites 56-63, bit 1 = sprites 48-55, and so on)
uint8 fetchMask[272];
template<int spriteSize>
void nes_ppu::resolveOAM() {
	// rebuild the fetch mask
	memset(fetchMask, 0, 240);

	unsigned char* curObj = &oam[252];
	const int scanlineOffset = 2;

	for (int b = 0; b < 8; b++) {
		uint8 bitMask = (1 << b);
		for (int i = 0; i < 8; i++, curObj -= 4) {
			unsigned int line = curObj[0] + scanlineOffset;
			if (line <= 242) {
				for (int x = 0; x < spriteSize; x++, line++) {
					fetchMask[line] |= bitMask;
				}
			}
		}
	}

	dirtyOAM = false;
}

#define PRIORITY_PIXEL 0x40
template<bool sprite16,int spriteSize>
void static renderOAM(nes_ppu& ppu) {
	if (ppu.dirtyOAM) {
		ppu.resolveOAM<spriteSize>();
	}

	// MMC2/4 support
	if (nesCart.renderLatch) {
		ppu.fastOAMLatchCheck();
	}

	int baseX = ppu.SCROLLX & 15;

	if ((ppu.PPUMASK & PPUMASK_SHOWLEFTBG) == 0) {
		for (int i = 0; i < 8; i++) {
			ppu.scanlineBuffer[baseX+i] = 0;
		}
	}

	if ((ppu.PPUMASK & PPUMASK_SHOWOBJ) && fetchMask[ppu.scanline]) {
		// render objects to separate buffer
		int numSprites = 0;
		unsigned char* curObj = &ppu.oam[252];
		unsigned int patternOffset = ((!sprite16 && (ppu.PPUCTRL & PPUCTRL_OAMTABLE)) ? 1 : 0);
		unsigned char* patternTable = ppu.chrPages[patternOffset];
		static uint8 spriteMask[33] = { 0 };
		int minSpriteMask = 32;
		int maxSpriteMask = 0;
		int scanlineOffset = ppu.scanline - 2;

		// mask out 8 at a time
		for (int b = fetchMask[ppu.scanline]; b; b = b >> 1) {
			if (!(b & 1)) {
				curObj -= 32;
				continue;
			}

			for (int i = 0; i < 8; i++, curObj -= 4) {
				unsigned int yCoord = scanlineOffset - curObj[0];
				if (yCoord < spriteSize) {
					numSprites++;

					if (curObj[2] & OAMATTR_VFLIP) yCoord = (spriteSize - 1) - yCoord;

					// determine tile index
					unsigned char* tile;
					if (sprite16) {
						tile = ppu.chrPages[curObj[1] & 1] + ((curObj[1] & 0xFE) << 4) + ((yCoord & 8) << 1) + (yCoord & 7);
					} else {
						tile = patternTable + (curObj[1] << 4) + yCoord;
					}

					unsigned int x = curObj[3];

					// interleave the bit planes and assign to char buffer (only unmapped pixels)
					const uint8 tile0 = tile[0];
					const uint8 tile8 = tile[8];
					uint16 tileMask = (tile0 | tile8);
					unsigned int bitPlane = (MortonTable[tile0] | (MortonTable[tile8] << 1)) << 1;
					unsigned int palette = (((curObj[2] & 3) << 2) + (curObj[2] & OAMATTR_PRIORITY) + 16) << 1;
					unsigned char* buffer = oamScanlineBuffer + x;

					if (curObj[2] & OAMATTR_HFLIP) {
											if (bitPlane & 6) buffer[0] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[1] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[2] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[3] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[4] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[5] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[6] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[7] = palette | (bitPlane & 6);
					} else {
						tileMask = reverseByte(tileMask);
											if (bitPlane & 6) buffer[7] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[6] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[5] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[4] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[3] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[2] = palette | (bitPlane & 6); 
						bitPlane >>= 2;		if (bitPlane & 6) buffer[1] = palette | (bitPlane & 6);
						bitPlane >>= 2;		if (bitPlane & 6) buffer[0] = palette | (bitPlane & 6);
					}

					int mask = x >> 3;
					tileMask = tileMask << (x & 7);
					spriteMask[mask] |= tileMask & 0xFF;
					if (mask < minSpriteMask) minSpriteMask = mask;
					if (mask > maxSpriteMask) maxSpriteMask = mask;
					mask = (x+7) >> 3;
					spriteMask[mask] |= tileMask >> 8;
					if (mask < minSpriteMask) minSpriteMask = mask;
					if (mask > maxSpriteMask) maxSpriteMask = mask;
				}
			}
		}

		if (numSprites) {
			// mask left 8 pixels
			if ((ppu.PPUMASK & PPUMASK_SHOWLEFTOBJ) == 0) {
				for (int i = 0; i < 8; i++) {
					oamScanlineBuffer[i] = 0;
				}
			}

			// resolve objects
			if (maxSpriteMask > 31) maxSpriteMask = 31;
			unsigned char* targetPixelBase = &ppu.scanlineBuffer[baseX];
			for (int sprite = minSpriteMask; sprite <= maxSpriteMask; sprite++) {
				if (spriteMask[sprite]) {
					unsigned char* targetPixel = targetPixelBase + sprite * 8;
					unsigned char* oamPixel = &oamScanlineBuffer[sprite *8];
					for (int b = spriteMask[sprite]; b; b >>= 1, oamPixel++, targetPixel++) {
						if (b & 1) {
							if ((*oamPixel & PRIORITY_PIXEL) == 0 || (*targetPixel & 6) == 0) {
								*targetPixel = (*oamPixel & (PRIORITY_PIXEL - 1));
							}
							*oamPixel = 0;
						}
					}
					spriteMask[sprite] = 0;
				}
			}
		}
	}
}

void nes_ppu::doOAMRender() {
	TIME_SCOPE();

	if (canSprite0Hit()) {
		fastSprite0(true);
	}

	if ((PPUCTRL & PPUCTRL_SPRSIZE) == 0) {
		renderOAM<false, 8>(*this);
	} else {
		renderOAM<true, 16>(*this);
	}
}

void nes_ppu::resolveOAMExternal() {
	if ((PPUCTRL & PPUCTRL_SPRSIZE) == 0) {
		resolveOAM<8>();
	} else {
		resolveOAM<16>();
	}
}

inline void UnrollPalette(uint32& palette) {
	// put palette in every 4 bytes (* 2 to account for offset into word sized color table during resolve)
	palette <<= 1;
	palette = palette | (palette << 8);
	palette = palette | (palette << 16);
}

void nes_ppu::renderScanline_SingleMirror(nes_ppu& ppu) {
	TIME_SCOPE_NAMED(scanline_single);

	DebugAssert(ppu.scanline >= 1 && ppu.scanline <= 240);
	DebugAssert(nesCart.renderLatch == NULL);

	int line = ppu.scanline - 1;
	if (ppu.scrollY < 240) {
		line += ppu.scrollY;
	} else {
		// "negative scroll" which we'll just skip the top lines for
		line += ppu.scrollY - 256;
	}

	if ((ppu.PPUMASK & PPUMASK_SHOWBG) && line >= 0) {
		int tileLine = line >> 3;

		// determine base addresses
		unsigned char* nameTable;
		unsigned char* attr;
		unsigned int chrOffset = (line & 7);
		unsigned char* patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;

		if (tileLine >= 30) {
			tileLine -= 30;
		}
		
		if (ppu.mirror == nes_mirror_type::MT_SINGLE) {
			nameTable = &ppu.nameTables[0].table[tileLine << 5];
			attr = &ppu.nameTables[0].attr[(tileLine >> 2) << 3];
		} else {
			DebugAssert(ppu.mirror == nes_mirror_type::MT_SINGLE_UPPER);
			nameTable = &ppu.nameTables[1].table[tileLine << 5];
			attr = &ppu.nameTables[1].attr[(tileLine >> 2) << 3];
		}

		// pre-build attribute table lookup into one big 32 bit int
		// this is done backwards so the value is easily popped later
		int attrX = (ppu.SCROLLX >> 5) & 7;
		int attrShift = (tileLine & 2) << 1;	// 4 bit shift for bottom row of attribute
		unsigned int attrPalette = 0;
		for (int loop = 0; loop < 8; loop++) {
			attrPalette <<= 4;
			attrPalette |= ((attr[attrX] >> attrShift) & 0xF);
			attrX = (attrX + 7) & 7;
		}

		if ((ppu.SCROLLX & 0x10) == 0) {
			attrPalette = (attrPalette << 6) | (attrPalette >> 26);
		} else {
			attrPalette = (attrPalette << 4) | (attrPalette >> 28);
		}

		// we render 16 pixels at a time (easy attribute table lookup), 17 times and clip
		uint8* buffer = ppu.scanlineBuffer;
		int tileX = (ppu.SCROLLX >> 4) * 2;	// always start on an even numbered tile
		uint32 lastChr = -1;

		for (int loop = 0; loop < 17; loop++) {
			// grab and rotate palette selection
			uint32 palette = (attrPalette & 0x0C);
			Rotate_2Bit(attrPalette);

			// keep tileX mirroring
			tileX &= 0x1F;

			uint8 chr1 = nameTable[tileX++];
			uint8 chr2 = nameTable[tileX++];
			uint32 chrSig = (chr1 << 16) | (chr2 << 8) | palette;

			if (chrSig == lastChr) {
				CopyOver16(buffer - 16);
				buffer += 16;
			} else {
				UnrollPalette(palette);
				lastChr = chrSig;

				RenderToScanline(patternTable, chr1 << 4, palette, buffer);
				buffer += 8;
				RenderToScanline(patternTable, chr2 << 4, palette, buffer);
				buffer += 8;
			}
		}
	} else {
		for (int i = 0; i < 288; i++) {
			ppu.scanlineBuffer[i] = 0;
		}
		if (ppu.causeDecrement) {
			ppu.scrollY--;
		}
	}

	ppu.doOAMRender();
}

void nes_ppu::renderScanline_HorzMirror(nes_ppu& ppu) {
	TIME_SCOPE_NAMED(scanline_horz);

	DebugAssert(ppu.scanline >= 1 && ppu.scanline <= 240);

	int line = ppu.scanline - 1;
	if (ppu.scrollY < 240) {
		line += ppu.scrollY;
	} else {
		// "negative scroll" which we'll just skip the top lines for
		line += ppu.scrollY - 256;
	}

	if ((ppu.PPUMASK & PPUMASK_SHOWBG) && line >= 0) {
		int tileLine = line >> 3;

		// determine base addresses
		unsigned char* nameTable;
		unsigned char* attr;
		unsigned int chrOffset = (line & 7);
		unsigned char* patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;

		if (ppu.flipY) {
			tileLine += 30;
			if (tileLine >= 60) tileLine -= 60;
		}

		if (tileLine < 30) {
			nameTable = &ppu.nameTables[0].table[tileLine << 5];
			attr = &ppu.nameTables[0].attr[(tileLine >> 2) << 3];
		} else {
			tileLine -= 30;
			nameTable = &ppu.nameTables[1].table[tileLine << 5];
			attr = &ppu.nameTables[1].attr[(tileLine >> 2) << 3];
		}

		// pre-build attribute table lookup into one big 32 bit int
		// this is done backwards so the value is easily popped later
		uint32 attrX = (ppu.SCROLLX >> 5) & 7;
		unsigned int attrPalette = 0;
		if (tileLine & 2) { // 4 bit shift for bottom row of attribute
			attrPalette = attr[attrX] >> 4;
			attrX = (attrX + 7) & 7;
			for (int loop = 0; loop < 7; loop++) {
				attrPalette <<= 4;
				attrPalette |= (attr[attrX] >> 4);
				attrX = (attrX + 7) & 7;
			}
		} else {
			attrPalette |= (attr[attrX] & 0xF);
			attrX = (attrX + 7) & 7;
			for (int loop = 0; loop < 7; loop++) {
				attrPalette <<= 4;
				attrPalette |= (attr[attrX] & 0xF);
				attrX = (attrX + 7) & 7;
			}
		}

		if ((ppu.SCROLLX & 0x10) == 0) {
			attrPalette = (attrPalette << 6) | (attrPalette >> 26);
		} else {
			attrPalette = (attrPalette << 4) | (attrPalette >> 28);
		}

		// we render 16 pixels at a time (easy attribute table lookup), 17 times and clip
		uint8* buffer = ppu.scanlineBuffer;
		int tileX = (ppu.SCROLLX >> 4) * 2;	// always start on an even numbered tile

		// MMC2/4 too branch here for faster code:
		if (nesCart.renderLatch) {
			uint32 lastChr = -1;
			for (int loop = 0; loop < 17; loop++) {
				// grab and rotate palette selection
				uint32 palette = (attrPalette & 0x0C);
				Rotate_2Bit(attrPalette);

				// keep tileX mirroring
				tileX &= 0x1F;

				uint8 chr1 = nameTable[tileX++];
				uint8 chr2 = nameTable[tileX++];
				uint32 chrSig = (chr1 << 16) | (chr2 << 8) | palette;

				if (chrSig == lastChr) {
					CopyOver16(buffer - 16);
					buffer += 16;
				} else {
					UnrollPalette(palette);
					lastChr = chrSig;

					RenderToScanline(patternTable, chr1 << 4, palette, buffer);
					if (chr1 >= 0xFD) {
						nesCart.renderLatch((chr1 << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
						patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;
					}
					buffer += 8;
					RenderToScanline(patternTable, chr2 << 4, palette, buffer);
					if (chr2 >= 0xFD) {
						nesCart.renderLatch((chr2 << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
						patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;
					}
					buffer += 8;
				}
			}
		} else {
			uint32 lastChr = -1;

			for (int loop = 0; loop < 17; loop++) {
				// grab and rotate palette selection
				uint32 palette = (attrPalette & 0x0C);
				Rotate_2Bit(attrPalette);

				// keep tileX mirroring
				tileX &= 0x1F;

				uint32 chr1 = nameTable[tileX++];
				uint32 chr2 = nameTable[tileX++] << 8;
				uint32 chrSig = (chr1 << 16) | chr2 | palette;

				if (chrSig == lastChr) {
					CopyOver16(buffer - 16);
				} else {
					UnrollPalette(palette);
					lastChr = chrSig;

					RenderToScanline(patternTable, chr1 << 4, palette, buffer);
					RenderToScanline(patternTable, chr2 >> 4, palette, buffer + 8);
				}

				buffer += 16;
			}
		}
	} else {
		for (int i = 0; i < 288; i++) {
			ppu.scanlineBuffer[i] = 0;
		}
		if (ppu.causeDecrement) {
			ppu.scrollY--;
		}
	}

	ppu.doOAMRender();
}

template<bool hasLatch, bool is4Pane>
static void renderScanline_VertMirror_Latched(nes_ppu& ppu) {
	DebugAssert(ppu.scanline >= 1 && ppu.scanline <= 240);
	if (ppu.PPUMASK & PPUMASK_SHOWBG) {
		int line = ppu.scanline - 1;

		if (ppu.scrollY < 240) {
			line += ppu.scrollY;
		} else {
			// "negative scroll" which we'll just skip the top lines for
			line += ppu.scrollY - 256;
		}

		int tileLine = line >> 3;

		int nameTableIndex = 0;
		if (is4Pane) {
			if (tileLine >= 60) {
				tileLine -= 60;
			}
			else if (tileLine < 0) {
				tileLine += 60;
			}
			if (tileLine >= 30) {
				nameTableIndex = 2;
				tileLine -= 30;
			}
			if (ppu.PPUCTRL & PPUCTRL_FLIPYTBL) {
				nameTableIndex = nameTableIndex ^ 2;
			}
		} else {
			if (tileLine >= 30) {
				tileLine -= 30;
			} else if (tileLine < 0) {
				tileLine += 30;
			}
		}

		int scrollX = ppu.SCROLLX;

		// determine base addresses
		unsigned char* nameTable;
		unsigned char* attr;
		const bool attrShift = (tileLine & 2);	// 4 bit shift for bottom row of attribute
		unsigned int chrOffset = (line & 7);
		unsigned char* patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;

		if (ppu.PPUCTRL & PPUCTRL_FLIPXTBL) scrollX += 256;

		// we render 16 pixels at a time (easy attribute table lookup), 17 times and clip
		uint8* buffer = ppu.scanlineBuffer;
		int tileX = ((scrollX >> 4) * 2) & 0x3F;	// always start on an even numbered tile
		nameTableIndex += (tileX & 0x20) >> 5;
		
		nameTable = &ppu.nameTables[nameTableIndex].table[tileLine << 5];
		attr = &ppu.nameTables[nameTableIndex].attr[(tileLine >> 2) << 3];

		// render tileX up to the end of the current nametable
		int curTileX = tileX & 0x1F;
		uint32 lastChr = -1;
		while (curTileX < 32) {
			// grab and rotate palette selection
			bool hadLatch = false;
			uint32 attrPalette;
			if (attrShift) {
				attrPalette = (attr[(curTileX >> 2)] >> 2) >> (curTileX & 2);
			} else {
				attrPalette = (attr[(curTileX >> 2)] << 2) >> (curTileX & 2);
			}

			uint32 palette = (attrPalette & 0x0C);
			
			uint8 chr1 = nameTable[curTileX++];
			uint8 chr2 = nameTable[curTileX++];
			uint32 chrSig = (chr1 << 16) | (chr2 << 8) | palette;

			if (chrSig == lastChr) {
				CopyOver16(buffer - 16);
				buffer += 16;
			} else {
				UnrollPalette(palette);
				lastChr = chrSig;

				if (hasLatch) {
					if (chr1 == 0xFD || chr1 == 0xFE) {
						nesCart.renderLatch((chr1 << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
						hadLatch = true;
					}

					RenderToScanline(patternTable, chr1 << 4, palette, buffer);
					buffer += 8;

					if (hadLatch) {
						patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;
						hadLatch = false;
					}

					if (chr2 == 0xFD || chr2 == 0xFE) {
						nesCart.renderLatch((chr2 << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
						hadLatch = true;
					}

					RenderToScanline(patternTable, chr2 << 4, palette, buffer);
					buffer += 8;

					if (hadLatch) {
						patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;
						hadLatch = false;
					}
				} else {
					RenderToScanline(patternTable, chr1 << 4, palette, buffer);
					buffer += 8;
					RenderToScanline(patternTable, chr2 << 4, palette, buffer);
					buffer += 8;
				}
			}
		}

		// now render the other nametable until scanline is complete
		curTileX = 0;
		nameTableIndex = nameTableIndex ^ 1;
		nameTable = &ppu.nameTables[nameTableIndex].table[tileLine << 5];
		attr = &ppu.nameTables[nameTableIndex].attr[(tileLine >> 2) << 3];
		uint8* bufferEnd = &ppu.scanlineBuffer[16 * 17];

		while (buffer < bufferEnd) {
			// grab and rotate palette selection
			bool hadLatch = false;
			uint32 attrPalette;
			if (attrShift) {
				attrPalette = (attr[(curTileX >> 2)] >> 2) >> (curTileX & 2);
			} else {
				attrPalette = (attr[(curTileX >> 2)] << 2) >> (curTileX & 2);
			}
			uint32 palette = (attrPalette & 0x0C);

			uint8 chr1 = nameTable[curTileX++];
			uint8 chr2 = nameTable[curTileX++];
			uint32 chrSig = (chr1 << 16) | (chr2 << 8) | palette;
			
			if (chrSig == lastChr) {
				CopyOver16(buffer - 16);
				buffer += 16;
			} else {
				UnrollPalette(palette);
				lastChr = chrSig;

				if (hasLatch) {
					if (chr1 == 0xFD || chr1 == 0xFE) {
						nesCart.renderLatch((chr1 << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
						hadLatch = true;
					}

					RenderToScanline(patternTable, chr1 << 4, palette, buffer);
					buffer += 8;

					if (hadLatch) {
						patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;
						hadLatch = false;
					}

					if (chr2 == 0xFD || chr2 == 0xFE) {
						nesCart.renderLatch((chr2 << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
						hadLatch = true;
					}

					RenderToScanline(patternTable, chr2 << 4, palette, buffer);
					buffer += 8;

					if (hadLatch) {
						patternTable = ppu.chrPages[(ppu.PPUCTRL & PPUCTRL_BGDTABLE) >> 4] + chrOffset;
						hadLatch = false;
					}
				} else {
					RenderToScanline(patternTable, chr1 << 4, palette, buffer);
					buffer += 8;
					RenderToScanline(patternTable, chr2 << 4, palette, buffer);
					buffer += 8;
				}
			}
		}

		if (hasLatch) {
			// fetch 34th tile
			if (buffer < bufferEnd + 8 && curTileX < 32) {
				int chr = nameTable[curTileX];

				if (chr == 0xFD || chr == 0xFE) {
					nesCart.renderLatch((chr << 4) + chrOffset + 8 + ((ppu.PPUCTRL & PPUCTRL_BGDTABLE) << 8));
				}
			}
		}
	} else {
		for (int i = 0; i < 288; i++) {
			ppu.scanlineBuffer[i] = 0;
		}
		if (ppu.causeDecrement) {
			ppu.scrollY--;
		}
	}
	 
	ppu.doOAMRender();
}

void nes_ppu::renderScanline_VertMirror(nes_ppu& ppu) {
	TIME_SCOPE();

	if (nesCart.renderLatch) {
		renderScanline_VertMirror_Latched<true, false>(ppu);
	} else {
		renderScanline_VertMirror_Latched<false, false>(ppu);
	}
}

void nes_ppu::renderScanline_4PaneMirror(nes_ppu& ppu) {
	renderScanline_VertMirror_Latched<false, true>(ppu);
}

void nes_ppu::setMirrorType(int withType) {
	if (mirror == withType) {
		return;
	}

	mirror = withType;
	switch (mirror) {
		case nes_mirror_type::MT_HORIZONTAL:
			renderScanline = renderScanline_HorzMirror;
			break;
		case nes_mirror_type::MT_VERTICAL:
			renderScanline = renderScanline_VertMirror;
			break;
		case nes_mirror_type::MT_SINGLE:
		case nes_mirror_type::MT_SINGLE_UPPER:
			renderScanline = renderScanline_SingleMirror;
			break;
		case nes_mirror_type::MT_4PANE:
			renderScanline = renderScanline_4PaneMirror;
			break;
	}
}

void nes_ppu::init() {
	memset(this, 0, sizeof(nes_ppu));
	scanline = 1;
	mirror = nes_mirror_type::MT_UNSET;
	autoFrameSkip = 0;
}

void nes_ppu::initTV() {
	if (nesCart.isPAL == 0) {
		for (int i = 0; i < 245; i++) {
			scanlineClocks[i] = (341 / 3) + (scanline % 3 != 0 ? 1 : 0);
		}
	} else {
		for (int i = 0; i < 245; i++) {
			scanlineClocks[i] = ((i+1) * 1705) / 16 - (i * 1705) / 16;
		}
	}
}