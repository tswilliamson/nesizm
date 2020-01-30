// Resolves scanline direct to VRAM (for use in emulator or WindowsSim)

#if !TARGET_PRIZM

#include "platform.h"
#include "debug.h"
#include "nes.h"

// used for direct render of frame count
#include "calctype/calctype.h"
#include "calctype/fonts/arial_small/arial_small.h"	

static unsigned char scanlineBufferMem[256 + 16 * 2] = { 0 }; 

void nes_ppu::initScanlineBuffer() {
	nesPPU.scanlineBuffer = scanlineBufferMem;
}

void nes_ppu::resolveScanline(int scrollOffset) {
	TIME_SCOPE();

	if (nesPPU.scanline >= 13 && nesPPU.scanline <= 228) {
		unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (nesPPU.scanline - 13) * 384 + 72;
		unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
		for (int i = 0; i < 240; i++, scanlineSrc++) {
			*(scanlineDest++) = workingPalette[(*scanlineSrc) >> 1];
		}
	}
}

void nes_ppu::finishFrame() {
	TIME_SCOPE();

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
}

#endif