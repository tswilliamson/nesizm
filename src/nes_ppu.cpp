
// Main NES ppu implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

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
			if (PPUSTATUS & PPUCTRL_VRAMINC) {
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
		ppu_registers.PPUSTATUS &= ~PPUCTRL_NMI;			// clear vblank flag
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
		ppu_registers.PPUSTATUS |= PPUCTRL_NMI;
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


void renderScanline_HorzMirror() {
	DebugAssert(scanline >= 1 && scanline <= 240);
	if (ppu_registers.PPUMASK & PPUMASK_SHOWBG) {
		int line = scanline + ppu_registers.SCROLLY - 1;
		int tileLine = line >> 3;

		// determine base addresses
		unsigned char* nameTable;
		unsigned char* attr;
		int attrShift = (tileLine & 1) << 2;	// 4 bit shift for bottom row of attribute
		unsigned char* patternTable = ppu_chrMap + ((ppu_registers.PPUCTRL & PPUCTRL_BGDTABLE) << 8) + (line & 7);
		if ((line < 30) == ((ppu_registers.PPUCTRL & PPUCTRL_FLIPYTBL) == 0)) {
			nameTable = &ppu_nameTables[0].table[tileLine << 5];
			attr = &ppu_nameTables[0].attr[(tileLine >> 1) << 3];
		} else {
			tileLine -= 30;
			nameTable = &ppu_nameTables[1].table[tileLine << 5];
			attr = &ppu_nameTables[0].attr[(tileLine >> 1) << 3];
		}

		// pre-build attribute table lookup into one big 32 bit int
		int attrX = (ppu_registers.SCROLLX >> 5);
		unsigned int attrPalette = 0;
		if (ppu_registers.SCROLLX & 0x8) {
			// odd scroll
			for (int loop = 0; loop < 8; loop++) {
				attrPalette <<= 4;
				attrPalette |= ((attr[attrX] >> attrShift) & 0xC);
				attrX = (attrX + 1) & 7;
				attrPalette |= ((attr[attrX] >> attrShift) & 0x3);
			}
		} else {
			// even scroll
			for (int loop = 0; loop < 8; loop++) {
				attrPalette <<= 4;
				attrPalette |= ((attr[attrX] >> attrShift) & 0xF);
				attrX = (attrX + 1) & 7;
			}
		}

		// we render 16 pixels at a time (easy attribute table lookup), 17 times and clip
		int x = 16 - (ppu_registers.SCROLLX & 15);
		int tileX = (ppu_registers.SCROLLX >> 4) * 2;	// always start on an even numbered tile
		int palColors[4];
		palColors[0] = ppu_palette[0];
		for (int loop = 0; loop < 17; loop++) {
			// grab and rotate palette selection
			int palette = (attrPalette & 0x03) << 2;
			attrPalette = (attrPalette >> 2) | (attrPalette << 30);
			palColors[1] = ppu_palette[palette + 1];
			palColors[2] = ppu_palette[palette + 2];
			palColors[3] = ppu_palette[palette + 3];

			for (int twice = 0; twice < 2; twice++, x += 8)
			{
				int chr = nameTable[tileX++] << 4;
				int loPlane = patternTable[chr];
				int hiPlane = patternTable[chr + 8];
				scanlineBuffer[x + 7] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 6] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 5] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 4] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 3] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 2] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 1] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)]; hiPlane >>= 1; loPlane >>= 1;
				scanlineBuffer[x + 0] = palColors[((hiPlane & 1) << 1) | (loPlane & 1)];
			}
		}
	}
}

// TODO - Obviously don't convert this in real time
static unsigned char rgbPalette[64 * 3] = {
	84,84,84,0,30,116,8,16,144,48,0,136,68,0,100,92,0,48,84,4,0,60,24,0,32,42,0,8,58,0,0,64,0,0,60,0,0,50,60,0,0,0,
	152,150,152,8,76,196,48,50,236,92,30,228,136,20,176,160,20,100,152,34,32,120,60,0,84,90,0,40,114,0,8,124,0,0,118,40,0,102,120,0,0,0,
	236,238,236,76,154,236,120,124,236,176,98,236,228,84,236,236,88,180,236,106,100,212,136,32,160,170,0,116,196,0,76,208,32,56,204,108,56,180,204,60,60,60,
	236,238,236,168,204,236,188,188,236,212,178,236,236,174,236,236,174,212,236,180,176,228,196,144,204,210,120,180,222,120,168,226,144,152,226,180,160,214,228,160,162,160
};

void resolveScanline() {
	DebugAssert(scanline >= 13 && scanline <= 228);

	unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (scanline - 13) * 384 + 64;
	unsigned int* scanlineSrc = &scanlineBuffer[16];	// with clipping
	for (int i = 0; i < 256; i++) {
		unsigned char* rgb = &rgbPalette[*(scanlineSrc++)];
		*(scanlineDest++) =
			((rgb[0] & 0xF8) << 8) |
			((rgb[1] & 0xFC) << 3) |
			((rgb[2] & 0xF8) >> 3);
	}
}