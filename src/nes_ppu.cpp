
// Main NES ppu implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

#include "../../calctype/calctype.h"
#include "../../calctype/fonts/arial_small/arial_small.h"		

extern void PPUBreakpoint();

// ppu statics
ppu_registers_type ppu_registers;
unsigned char ppu_oam[0x100] = { 0 };
nes_nametable ppu_nameTables[4];
unsigned char ppu_palette[0x20] = { 0 };
unsigned char* ppu_chrMap = { 0 };
int ppu_mirror = nes_mirror_type::MT_UNSET;

// private to this file

// current scanline and scanline buffer with padding to allow fast rendering with clipping
static int scanline = 1;	// scanline is 1-based... whatever!
static unsigned int scanlineBuffer[256 + 16 * 2] = { 0 }; 
static unsigned int oamBuffer[264] = { 0 };

static unsigned int frameCounter = 0;

// pointer to function to render current scanline to scanline buffer (based on mirror mode)
static void(*renderScanline)() = 0;
void resolveScanline();		// resolves the current scanline buffer to the screen

static void renderScanline_HorzMirror();

void ppu_setMirrorType(int withType) {
	if (ppu_mirror == withType) {
		return;
	}

	ppu_mirror = withType;
	switch (ppu_mirror) {
		case nes_mirror_type::MT_HORIZONTAL:
			renderScanline = renderScanline_HorzMirror;
			break;
		case nes_mirror_type::MT_VERTICAL:
			DebugAssert(false);
			break;
		case nes_mirror_type::MT_4PANE:
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
				address = (address & 0x03FF) | ((address & 0x800) >> 1);
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
		if ((address & 0x03) == 0)
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

#if DEBUG
		char buffer[32];
		sprintf(buffer, "%d   ", frameCounter);
		int x = 2;
		int y = 200;
		for (int y1 = y; y1 < y + 10; y1++) {
			unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + y1 * 384;
			for (int x1 = 0; x1 < 60; x1++) {
				scanlineDest[x1] = 0;
			}
		}
		CalcType_Draw(&arial_small, buffer, x, y, COLOR_WHITE, 0, 0);
#endif

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

void renderOAM() {
	if ((ppu_registers.PPUMASK & PPUMASK_SHOWLEFTBG) == 0) {
		for (int i = 16; i < 24; i++) {
			scanlineBuffer[i] = 0;
		}
	}

	if (ppu_registers.PPUMASK & PPUMASK_SHOWOBJ) {
		// TODO : 8x16 mode
		DebugAssert((ppu_registers.PPUCTRL & PPUCTRL_SPRSIZE) == 0);

		// render objects to separate buffer
		bool hadSprite = false;
		unsigned char* curObj = &ppu_oam[252];
		unsigned char* patternTable = ppu_chrMap + ((ppu_registers.PPUCTRL & PPUCTRL_OAMTABLE) ? 0x1000 : 0x0000);
		for (int i = 0; i < 64; i++) {
			unsigned int yCoord = scanline - curObj[0] - 2;
			if (yCoord < 8) {
				hadSprite = true;
				unsigned char* tile = patternTable + (curObj[1] << 4);
				if (curObj[2] & OAMATTR_VFLIP) yCoord = 8 - yCoord;
				unsigned int x = curObj[3];

				// interleave the but planes and assign to char buffer (only unmapped pixels)
				unsigned int bitPlane = MortonTable[tile[yCoord]] | (MortonTable[tile[yCoord + 8]] << 1);
				unsigned int palette = ((curObj[2] & 3) << 2) + (curObj[2] & OAMATTR_PRIORITY) + 16;

				if (curObj[2] & OAMATTR_HFLIP) {
										if (bitPlane & 3) oamBuffer[x + 0] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 1] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 2] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 3] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 4] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 5] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 6] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 7] = palette + (bitPlane & 3);
				} else {
										if (bitPlane & 3) oamBuffer[x + 7] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 6] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 5] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 4] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 3] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 2] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 1] = palette + (bitPlane & 3);
					bitPlane >>= 2;		if (bitPlane & 3) oamBuffer[x + 0] = palette + (bitPlane & 3);
				}
			}
			
			curObj -= 4;
		}

		if (hadSprite) {
			// mask left 8 pixels
			if ((ppu_registers.PPUMASK & PPUMASK_SHOWLEFTOBJ) == 0) {
				for (int i = 0; i < 8; i++) {
					oamBuffer[i] = 0;
				}
			}

			// sprite 0 hit
			if ((ppu_registers.PPUSTATUS & PPUSTAT_SPRITE0) == 0) {
				unsigned int charBuffer[8];

				// simulate the 8 bits from the write, but a little faster
				unsigned int yCoord0 = scanline - ppu_oam[0] - 2;
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
					*oamPixel = 0;
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

		for (int loop = 0; loop < 17; loop++) {
			// grab and rotate palette selection
			int palette = (attrPalette & 0x03) << 2;
			attrPalette = (attrPalette >> 2) | (attrPalette << 30);

			for (int twice = 0; twice < 2; twice++, x += 8)
			{
				int chr = nameTable[tileX++] << 4;

				int bitPlane = MortonTable[patternTable[chr]] | (MortonTable[patternTable[chr + 8]] << 1);
				scanlineBuffer[x + 7] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 6] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 5] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 4] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 3] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 2] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 1] = palette + (bitPlane & 3); bitPlane >>= 2;
				scanlineBuffer[x + 0] = palette + (bitPlane & 3);
			}
		}
	} else {
		for (int i = 0; i < 288; i++) {
			scanlineBuffer[i] = 0;
		}
	}

	renderOAM();
}

// TODO : handle emphasis bits
static unsigned short rgbPalette[64] = {
	0x5AAB, 0x010F, 0x0892, 0x3011, 0x480D, 0x6006, 0x5820, 0x40C0, 
	0x2160, 0x09E0, 0x0200, 0x01E0, 0x01A8, 0x0000, 0x0000, 0x0000, 
	0x9CD3, 0x0A79, 0x31BE, 0x611D, 0x88B6, 0xA0AD, 0x9924, 0x79E0, 
	0x5AE0, 0x2BA0, 0x0BE0, 0x03C5, 0x034F, 0x0000, 0x0000, 0x0000, 
	0xF79E, 0x54FE, 0x7BFE, 0xB33E, 0xEABE, 0xF2D7, 0xF36D, 0xDC44, 
	0xA560, 0x7E20, 0x5684, 0x3E6E, 0x3DBA, 0x41E8, 0x0000, 0x0000, 
	0xF79E, 0xAE7E, 0xC5FE, 0xDDBE, 0xF59E, 0xF59B, 0xF5B6, 0xEE32, 
	0xD6AF, 0xBF0F, 0xAF32, 0x9F37, 0xA6DD, 0xA534, 0x0000, 0x0000
};

void resolveScanline() {
	DebugAssert(scanline >= 13 && scanline <= 228);

	unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (scanline - 13) * 384 + 64;
	unsigned int* scanlineSrc = &scanlineBuffer[16];	// with clipping
	for (int i = 0; i < 256; i++, scanlineSrc++) {
		*(scanlineDest++) = rgbPalette[ppu_palette[*scanlineSrc]];
	}
}