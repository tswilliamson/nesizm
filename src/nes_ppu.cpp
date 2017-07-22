
// Main NES ppu implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

extern void PPUBreakpoint();

// ppu statics
ppu_registers_type ppu_registers;
unsigned char ppu_oam[0x100] = { 0 };
nes_nametable ppu_nameTables[4];
unsigned char ppu_palette[0x20] = { 0 };
unsigned char* ppu_chrMap = { 0 };
nes_mirror_type ppu_mirror = MT_UNSET;

// private to this file

// current scanline and scanline buffer with padding to allow fast rendering with clipping
static int scanline = 1;	// scanline is 1-based... whatever!
static unsigned int scanlineBuffer[256 + 16 * 2] = { 0 }; 

static unsigned int frameCounter = 0;

// pointer to function to render current scanline to scanline buffer (based on mirror mode)
static void(*renderScanline)() = 0;
void resolveScanline();		// resolves the current scanline buffer to the screen

static void renderScanline_HorzMirror();

void ppu_setMirrorType(nes_mirror_type withType) {
	if (ppu_mirror == withType) {
		return;
	}

	ppu_mirror = withType;
	switch (ppu_mirror) {
		case MT_HORIZONTAL:
			renderScanline = renderScanline_HorzMirror;
			break;
		case MT_VERTICAL:
			DebugAssert(false);
			break;
		case MT_4PANE:
			DebugAssert(false);
			break;
	}
}

unsigned char* ppu_registers_type::latchReg(unsigned int regNum) {
	switch (regNum) {
		case 0x02:  
			latch = &PPUSTATUS;
			break;
		case 0x04:
			latch = &ppu_oam[OAMADDR];
			break;
		case 0x07:
			latch = ppu_resolveMemoryAddress((ADDRHI << 8) | ADDRLO);
			break;
		// the rest are write only registers, simply return current latch
		// case 0x00:  PPUCTRL
		// case 0x01:  PPUMASK
		// case 0x03:  OAMADDR
		// case 0x05:  SCROLLX/Y
		// case 0x06:  ADDRHI/LO
	}
	return latch;
}

void ppu_registers_type::postReadLatch() {
	if (latch == &PPUSTATUS) {
		// vblank flag cleared after read
		PPUSTATUS &= ~PPUCTRL_NMI;
	}
}

void ppu_registers_type::writeReg(unsigned int regNum, unsigned char value) {
	switch (regNum) {
		case 0x00:	// PPUCTRL
			if ((PPUCTRL & PPUCTRL_NMI) == 0 && (value & PPUCTRL_NMI) && (PPUSTATUS & PPUCTRL_NMI)) {
				mainCPU.NMI();
			}
			latch = &PPUCTRL;
			break;
		case 0x01:	// PPUMASK
			latch = &PPUMASK;
			break;
		case 0x02:  
			latch = &PPUSTATUS;
			break;
		case 0x03:  // OAMADDR
			latch = &OAMADDR;
			break;
		case 0x04:
			latch = &ppu_oam[OAMADDR];
			// force unused bits to be low when appropriate
			if ((OAMADDR & 3) == 2) {
				value = value & 0xE3;
			}
			OAMADDR++;	// writes to OAM data increment the OAM address
			break;
		case 0x05:  // SCROLLX/Y
			if (latch == &SCROLLX) {
				latch = &SCROLLY;
			} else {
				latch = &SCROLLX;
			}
			break;
		case 0x06:  // ADDRHI/LO
			if (latch == &ADDRHI) {
				latch = &ADDRLO;
			} else {
				latch = &ADDRHI;
			}
			break;
		case 0x07:
			unsigned int address = (ADDRHI << 8) | ADDRLO;

			latch = ppu_resolveMemoryAddress(address);
			if (PPUCTRL & PPUCTRL_VRAMINC) {
				address += 32;
			} else {
				address += 1;
			}
			ADDRHI = (address & 0xFF00) >> 8;
			ADDRLO = (address & 0xFF);
			break;
	}

	*latch = value;

	// least significant bits previously written will end up in PPUSTATUS when read
	PPUSTATUS = (PPUSTATUS & 0xE0) | (value & 0x1F);
}

void ppu_oamDMA(unsigned int addr) {
	// perform the oam DMA
	for (int i = 0; i < 256; i++, addr++) {
		ppu_oam[i] = mainCPU.read(addr);
	}

	// force unused attribute bits low
	for (int i = 2; i < 256; i += 4) {
		ppu_oam[i] &= 0xE3;
	}

	mainCPU.clocks += 513 + (mainCPU.clocks & 1);
}

unsigned char* ppu_resolveMemoryAddress(unsigned int address) {
	address &= 0x3FFF;
	if (address < 0x2000) {
		// pattern table memory
		return &ppu_chrMap[address];
	} else if (address < 0x3F00) {
		// name table memory
		switch (ppu_mirror) {
			case nes_mirror_type::MT_HORIZONTAL:
				// $2400 = $2000, and $2c00 = $2800, so ignore bit 10:
				address = address & 0x03FF | ((address & 0x800) >> 1);
				break;
			case nes_mirror_type::MT_VERTICAL:
				// $2800 = $2000, and $2c00 = $2400, so ignore bit 11:
				address = address & 0x07FF;
				break;
			case nes_mirror_type::MT_4PANE:
				address = address & 0x0FFF;
				break;
		}
		// this is meant to overflow
		return &ppu_nameTables[0].table[address];
	} else {
		// palette memory 
		address &= 0x1F;
		
		// mirror background color between BG and OBJ types
		if (address & 0x03 == 0)
			address &= 0x0F;

		return &ppu_palette[address];
	}
}

// TODO : scanline check faster to do with table or function ptr?
void ppu_step() {
	// cpu time for next scanline
	mainCPU.ppuClocks += (341 / 3) + (scanline % 3 != 0 ? 1 : 0);

	// TODO: we should be able to render 224 via DMA

	/*
		262 scanlines, we render 13-228 (middle 216 screen lines)

		The idea is to render each scanline at the beginning of its line, and then
		to get sprite 0 timing right use a different ppuClocks value / update function
	*/
	if (scanline == 1) {
		// inactive scanline with even/odd clock cycle thing
		ppu_registers.PPUSTATUS &= ~PPUSTAT_NMI;			// clear vblank flag
		ppu_registers.PPUSTATUS &= ~PPUSTAT_SPRITE0;		// clear sprite 0 collision flag
	} else if (scanline < 13) {
		// non-resolved but active scanline (may cause sprite 0 collision)
		// TODO : sprite 0 collision only render version?
		renderScanline();
	} else if (scanline < 229) {
		// rendered scanline
		renderScanline();
		resolveScanline();
	} else if (scanline < 241) {
		// non-resolved scanline
		renderScanline();
	} else if (scanline == 242) {
		// blank scanline area
	} else if (scanline == 243) {
		if (mainCPU.clocks > 30000) {
			ppu_registers.PPUSTATUS |= PPUCTRL_NMI;
		}
		if ((ppu_registers.PPUCTRL & PPUCTRL_NMI) && (mainCPU.clocks > 140000)) {
			mainCPU.NMI();
		}
		frameCounter++;
		Bdisp_PutDisp_DD();
	} else if (scanline < 262) {
		// inside vblank
	} else {
		// final scanline
		scanline = 0;

		// frame timing .. total ppu frame should be every 29780.5 ppu clocks
		if (frameCounter & 1)
			mainCPU.ppuClocks -= 1;
	}

	scanline++;
}

void renderOAM() {
	if ((ppu_registers.PPUMASK & PPUMASK_SHOWLEFTBG) == 0) {
		for (int i = 16; i < 24; i++) {
			scanlineBuffer[i] = 0;
		}
	}

	if (ppu_registers.PPUMASK & PPUMASK_SHOWOBJ) {
		unsigned int oamBuffer[264];
		unsigned int charBuffer[8];

		// TODO : 8x16 mode
		DebugAssert((ppu_registers.PPUCTRL & PPUCTRL_SPRSIZE) == 0);

		// render objects to separate buffer
		unsigned char* curObj = &ppu_oam[252];
		unsigned char* patternTable = ppu_chrMap + ((ppu_registers.PPUCTRL & PPUCTRL_OAMTABLE) ? 0x1000 : 0x0000);
		for (int i = 0; i < 64; i++, curObj -= 4) {
			unsigned int yCoord = scanline - curObj[0] - 1;
			if (yCoord < 8) {
				unsigned char* tile = patternTable + (curObj[1] << 4);
				if (curObj[2] & OAMATTR_VFLIP) yCoord = 8 - yCoord;
				unsigned int x = curObj[3];

				unsigned int loPlane = tile[yCoord];
				unsigned int hiPlane = tile[yCoord + 8];

				if (curObj[2] & OAMATTR_HFLIP) {
					charBuffer[0] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[1] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[2] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[3] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[4] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[5] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[6] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[7] = ((hiPlane & 1) << 1) | (loPlane & 1);
				} else {
					charBuffer[7] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[6] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[5] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[4] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[3] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[2] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[1] = ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
					charBuffer[0] = ((hiPlane & 1) << 1) | (loPlane & 1);
				}

				// only write unmasked pixels
				unsigned int palette = ((curObj[2] & 3) << 2) + (curObj[2] & OAMATTR_PRIORITY) + 16;
				for (int p = 0; p < 8; p++) {
					if (charBuffer[p] & 3) {
						oamBuffer[x + p] = palette + charBuffer[p];
					}
				}
			}
		}

		// mask left 8 pixels
		if ((ppu_registers.PPUMASK & PPUMASK_SHOWLEFTOBJ) == 0) {
			for (int i = 0; i < 8; i++) {
				oamBuffer[i] = 0;
			}
		}

		// sprite 0 hit
		if ((ppu_registers.PPUSTATUS & PPUSTAT_SPRITE0) == 0) {
			// simulate the 8 bits from the write, but a little faster
			unsigned int yCoord0 = scanline - ppu_oam[0] - 1;
			if (yCoord0 < 8) {
				unsigned char* tile = patternTable + (ppu_oam[1] << 4);
				if (curObj[2] & OAMATTR_VFLIP) yCoord0 = 8 - yCoord0;
				unsigned int x = curObj[3];

				unsigned int plane = tile[yCoord0] | tile[yCoord0 + 8];

				if (curObj[2] & OAMATTR_HFLIP) {
					charBuffer[0] = plane & 1;
					charBuffer[1] = plane & 2;
					charBuffer[2] = plane & 4;
					charBuffer[3] = plane & 8;
					charBuffer[4] = plane & 16;
					charBuffer[5] = plane & 32;
					charBuffer[6] = plane & 64;
					charBuffer[7] = plane & 128;
				} else {
					charBuffer[7] = plane & 1;
					charBuffer[6] = plane & 2;
					charBuffer[5] = plane & 4;
					charBuffer[4] = plane & 8;
					charBuffer[3] = plane & 16;
					charBuffer[2] = plane & 32;
					charBuffer[1] = plane & 64;
					charBuffer[0] = plane & 128;
				}

				for (int p = 0; p < 8; p++) {
					if (charBuffer[p] && scanlineBuffer[16+x+p] && (x+p != 255)) {
						// TODO : this is approximate since it is at start of line!
						// It could be exact by storing actual upcoming clocks here and filling
						// PPUSTATUS when queried

						ppu_registers.PPUSTATUS |= PPUSTAT_SPRITE0;
					}
				}
			}
		}

		// resolve objects
		unsigned int* targetPixel = &scanlineBuffer[16];
		unsigned int* oamPixel = &oamBuffer[0];
		for (int i = 0; i < 256; i++, oamPixel++, targetPixel++) {
			if (*oamPixel & 3) {
				if ((*oamPixel & OAMATTR_PRIORITY) == 0 || (*targetPixel & 3) == 0) {
					*targetPixel = (*oamPixel & 0x1F);
				}
			}
		}
	}
}

void renderScanline_HorzMirror() {
	DebugAssert(scanline >= 1 && scanline <= 240);
	if (ppu_registers.PPUMASK & PPUMASK_SHOWBG) {
		int line = scanline + ppu_registers.SCROLLY - 1;
		int tileLine = line >> 3;

		// determine base addresses
		unsigned char* nameTable;
		unsigned char* attr;
		int attrShift = (tileLine & 2) << 1;	// 4 bit shift for bottom row of attribute
		unsigned char* patternTable = ppu_chrMap + ((ppu_registers.PPUCTRL & PPUCTRL_BGDTABLE) << 8) + (line & 7);

		// TODO : Probably some pretty obvious logic to avoid the %=
		if (ppu_registers.PPUCTRL & PPUCTRL_FLIPYTBL) tileLine += 30;
		tileLine %= 60;

		if (tileLine < 30) {
			nameTable = &ppu_nameTables[0].table[tileLine << 5];
			attr = &ppu_nameTables[0].attr[(tileLine >> 2) << 3];
		} else {
			tileLine -= 30;
			nameTable = &ppu_nameTables[1].table[tileLine << 5];
			attr = &ppu_nameTables[1].attr[(tileLine >> 2) << 3];
		}

		// pre-build attribute table lookup into one big 32 bit int
		// this is done backwards so the value is easily popped later
		int attrX = ((ppu_registers.SCROLLX >> 5) + 7) & 7;
		unsigned int attrPalette = 0;
		if (ppu_registers.SCROLLX & 0x8) {
			// odd scroll
			for (int loop = 0; loop < 8; loop++) {
				attrPalette <<= 4;
				attrPalette |= ((attr[attrX] >> attrShift) & 0xC);
				attrX = (attrX + 7) & 7;
				attrPalette |= ((attr[attrX] >> attrShift) & 0x3);
			}
		} else {
			// even scroll
			for (int loop = 0; loop < 8; loop++) {
				attrPalette <<= 4;
				attrPalette |= ((attr[attrX] >> attrShift) & 0xF);
				attrX = (attrX + 7) & 7;
			}
		}

		// we render 16 pixels at a time (easy attribute table lookup), 17 times and clip
		int x = 16 - (ppu_registers.SCROLLX & 15);
		int tileX = (ppu_registers.SCROLLX >> 4) * 2;	// always start on an even numbered tile
		int palColors[4];
		for (int loop = 0; loop < 17; loop++) {
			// grab and rotate palette selection
			int palette = (attrPalette & 0x03) << 2;
			attrPalette = (attrPalette >> 2) | (attrPalette << 30);

			for (int twice = 0; twice < 2; twice++, x += 8)
			{
				int chr = nameTable[tileX++] << 4;
				int loPlane = patternTable[chr];
				int hiPlane = patternTable[chr + 8];
				scanlineBuffer[x + 7] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 6] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 5] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 4] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 3] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 2] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 1] = palette + ((hiPlane & 1) << 1) | (loPlane & 1); hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 0] = palette + ((hiPlane & 1) << 1) | (loPlane & 1);
			}
		}
	} else {
		for (int i = 0; i < 288; i++) {
			scanlineBuffer[i] = 0;
		}
	}

	renderOAM();
}

// TODO - Obviously don't convert this in real time
static unsigned char rgbPalette[64 * 3] = {
	84,84,84,0,30,116,8,16,144,48,0,136,68,0,100,92,0,48,84,4,0,60,24,0,32,42,0,8,58,0,0,64,0,0,60,0,0,50,60,0,0,0,0,0,0,0,0,0,
	152,150,152,8,76,196,48,50,236,92,30,228,136,20,176,160,20,100,152,34,32,120,60,0,84,90,0,40,114,0,8,124,0,0,118,40,0,102,120,0,0,0,0,0,0,0,0,0,
	236,238,236,76,154,236,120,124,236,176,98,236,228,84,236,236,88,180,236,106,100,212,136,32,160,170,0,116,196,0,76,208,32,56,204,108,56,180,204,60,60,60,0,0,0,0,0,0,
	236,238,236,168,204,236,188,188,236,212,178,236,236,174,236,236,174,212,236,180,176,228,196,144,204,210,120,180,222,120,168,226,144,152,226,180,160,214,228,160,162,160,0,0,0,0,0,0
};

void resolveScanline() {
	DebugAssert(scanline >= 13 && scanline <= 228);

	unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (scanline - 13) * 384 + 64;
	unsigned int* scanlineSrc = &scanlineBuffer[16];	// with clipping
	for (int i = 0; i < 256; i++, scanlineSrc++) {
		unsigned char* rgb = &rgbPalette[ppu_palette[*scanlineSrc] * 3];
		*(scanlineDest++) =
			((rgb[0] & 0xF8) << 8) |
			((rgb[1] & 0xFC) << 3) |
			((rgb[2] & 0xF8) >> 3);
	}
}