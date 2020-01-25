// Resolves scanline direct to VRAM (for use in emulator or WindowsSim)

#if !TARGET_PRIZM

#include "platform.h"
#include "debug.h"
#include "nes.h"

// used for direct render of frame count
#include "calctype/calctype.h"
#include "calctype/fonts/arial_small/arial_small.h"	

static unsigned char ppu_scanlineBufferMem[256 + 16 * 2] = { 0 }; 
unsigned char* ppu_scanlineBuffer = ppu_scanlineBufferMem;

void resolveScanline_VRAM() {
	TIME_SCOPE();

	DebugAssert(ppu_scanline >= 13 && ppu_scanline <= 228);

	unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (ppu_scanline - 13) * 384 + 64;
	unsigned char* scanlineSrc = &ppu_scanlineBuffer[16];	// with clipping
	for (int i = 0; i < 256; i++, scanlineSrc++) {
		*(scanlineDest++) = ppu_workingPalette[*scanlineSrc];
	}
}

void finishFrame_VRAM() {
	TIME_SCOPE();

#if DEBUG
	char buffer[32];
	sprintf(buffer, "%d   ", ppu_frameCounter);
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