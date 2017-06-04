
// Main NES ppu implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_ppu nesPPU;

static unsigned char rgbPalette[64 * 3] = {
	84,84,84,0,30,116,8,16,144,48,0,136,68,0,100,92,0,48,84,4,0,60,24,0,32,42,0,8,58,0,0,64,0,0,60,0,0,50,60,0,0,0,
	152,150,152,8,76,196,48,50,236,92,30,228,136,20,176,160,20,100,152,34,32,120,60,0,84,90,0,40,114,0,8,124,0,0,118,40,0,102,120,0,0,0,
	236,238,236,76,154,236,120,124,236,176,98,236,228,84,236,236,88,180,236,106,100,212,136,32,160,170,0,116,196,0,76,208,32,56,204,108,56,180,204,60,60,60,
	236,238,236,168,204,236,188,188,236,212,178,236,236,174,236,236,174,212,236,180,176,228,196,144,204,210,120,180,222,120,168,226,144,152,226,180,160,214,228,160,162,160
};

nes_ppu::nes_ppu() {
	memset(this, 0, sizeof(nes_ppu));

	// initial latch
	latch = &PPUCTRL;
}

unsigned char* nes_ppu::latchReg(unsigned int regNum) {
	switch (regNum) {
		case 0x02:  
			latch = &PPUSTATUS;
			break;
		case 0x04:
			latch = &oam[OAMADDR];
			break;
		case 0x07:
			latch = resolvePPUMem((ADDRHI << 8) | ADDRLO);
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

void nes_ppu::postReadLatch() {
	if (latch == &PPUSTATUS) {
		// vblank flag cleared after read
		PPUSTATUS &= ~PPUSTAT_NMI;
	}
}

void nes_ppu::writeReg(unsigned int regNum, unsigned char value) {
	switch (regNum) {
		case 0x00:	// PPUCTRL
			if ((PPUCTRL & 0x80) == 0 && (value & 0x80)) {
				// TODO : activate NMI
				DebugAssert(0);
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
			latch = &oam[OAMADDR];
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
			latch = resolvePPUMem(address);
			if (PPUSTATUS & PPUSTAT_VRAMINC) {
				address += 32;
			} else {
				address += 1;
			}
			ADDRHI = (address & 0xFF00) >> 8;
			ADDRLO = (address & 0xFF);
			break;
	}

	*latch = value;
}

unsigned char* nes_ppu::resolvePPUMem(unsigned int address) {
	address &= 0x3FFF;
	if (address < 0x2000) {
		// pattern table memory
		return &chrMap[address];
	} else if (address < 0x3F00) {
		// name table memory
		switch (mirror) {
			case mirror_type::MT_HORIZONTAL:
				// $2400 = $2000, and $2c00 = $2800, so ignore bit 10:
				address = address & 0x03FF | ((address & 0x800) >> 1);
				break;
			case mirror_type::MT_VERTICAL:
				// $2800 = $2000, and $2c00 = $2400, so ignore bit 11:
				address = address & 0x07FF;
				break;
			case mirror_type::MT_4PANE:
				address = address & 0x0FFF;
				break;
		}
		// this is meant to overflow
		return &nameTables[0].table[address];
	} else {
		// palette memory
		address &= 0x1F;
		return &palette[address & 0x1F];
	}
}

// TODO : scanline check faster to do with table or function ptr?
void nes_ppu::step() {
	// cpu time for next scanline
	mainCPU.ppuClocks += (341 / 3) + (scanline % 3 != 0 ? 1 : 0);

	// TODO: we should be able to render 224 via DMA
	// 262 scanlines, we render 13-228 (middle 216 lines)
	if (scanline == 0) {
		// inactive scanline with even/odd clock cycle thing
		PPUSTATUS &= ~PPUSTAT_NMI;			// clear vblank flag
	} else if (scanline < 13) {
		// non-resolved but active scanline (may cause sprite 0 collision)
		renderScanline(scanline - 1);
	} else if (scanline < 229) {
		// rendered scanline
		renderScanline(scanline - 1);
		resolveScanline(scanline - 13);
	} else if (scanline < 241) {
		// non-resolved scanline
		renderScanline(scanline - 1);
	} else if (scanline == 241) {
		// blank scanline area
	} else if (scanline == 242) {
		// TODO NMI support 
		PPUSTATUS |= PPUSTAT_NMI;
	} else if (scanline < 262) {
		// inside vblank
	} else {
		// final scanline
		scanline = 0xFFFFFFFF;
	}

	scanline++;
}

void nes_ppu::renderScanline(unsigned int scanlineNum) {
	if (PPUMASK & PPUMASK_SHOWBG) {

	}
}

void nes_ppu::resolveScanline(unsigned int y) {
	unsigned short* dest = ((unsigned short*)GetVRAMAddress()) + 384 * y + 64;
	for (int i = 0; i < 256; i++) {
		DebugAssert(scanlineBuffer[i] < 64);

		unsigned char* rgb = &rgbPalette[scanlineBuffer[i]];
		*(dest++) =
			((rgb[0] & 0xF8) << 8) |
			((rgb[1] & 0xFC) << 3) |
			((rgb[2] & 0xF8) >> 3);
	}
}