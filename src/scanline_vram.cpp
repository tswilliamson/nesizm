// Resolves scanline direct to VRAM (for use in emulator or WindowsSim)

#if !TARGET_PRIZM

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "settings.h"

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
		if (nesSettings.GetSetting(ST_StretchScreen) == 1) {
			unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (nesPPU.scanline - 13) * 384 + 42 + scanlineOffset;
			unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
			const int interlacePixel = (nesPPU.frameCounter + nesPPU.scanline) & 3;
			for (int i = 0; i < 60; i++) {
				const uint16 pixels[4] = {
					workingPalette[(*scanlineSrc++) >> 1],
					workingPalette[(*scanlineSrc++) >> 1],
					workingPalette[(*scanlineSrc++) >> 1],
					workingPalette[(*scanlineSrc++) >> 1]
				};
				const uint16* pixel = pixels;
				for (int j = 0; j < 5; j++) {
					*(scanlineDest++) = *pixel;
					if (j != interlacePixel) pixel++;
				}
			}
		} else if (nesSettings.GetSetting(ST_StretchScreen) == 2) {
			unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (nesPPU.scanline - 13) * 384 + 12 + scanlineOffset;
			unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
			const bool bInterlace = (nesPPU.frameCounter + nesPPU.scanline) & 1;
			for (int i = 0; i < 120; i++) {
				const uint16 pixel1 = workingPalette[(*scanlineSrc++) >> 1];
				const uint16 pixel2 = workingPalette[(*scanlineSrc++) >> 1];
				*(scanlineDest++) = pixel1;
				*(scanlineDest++) = bInterlace ? pixel1 : pixel2;
				*(scanlineDest++) = pixel2;
			}
		} else {
			unsigned short* scanlineDest = ((unsigned short*)GetVRAMAddress()) + (nesPPU.scanline - 13) * 384 + 72 + scanlineOffset;
			unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
			for (int i = 0; i < 240; i++, scanlineSrc++) {
				*(scanlineDest++) = workingPalette[(*scanlineSrc) >> 1];
			}
		}
	}
}

void nes_ppu::renderBGOverscan() {

	unsigned short* scanlineDest = (unsigned short*)GetVRAMAddress();

	if (nesSettings.GetSetting(ST_StretchScreen) == 1) {
		for (int y = 0; y < 216; y++) {
			unsigned short* scanlineLeft = scanlineDest;
			unsigned short* scanlineRight = scanlineDest + (384 - 42);
			for (int i = 0; i < 42; i++) {
				*(scanlineLeft++) = currentBGColor;
				*(scanlineRight++) = currentBGColor;
			}
			scanlineDest += 384;
		}
	} else if (nesSettings.GetSetting(ST_StretchScreen) == 2) {
		for (int y = 0; y < 216; y++) {
			unsigned short* scanlineLeft = scanlineDest;
			unsigned short* scanlineRight = scanlineDest + (384 - 12);
			for (int i = 0; i < 12; i++) {
				*(scanlineLeft++) = currentBGColor;
				*(scanlineRight++) = currentBGColor;
			}
			scanlineDest += 384;
		}
	} else {
		for (int y = 0; y < 216; y++) {
			unsigned short* scanlineLeft = scanlineDest;
			unsigned short* scanlineRight = scanlineDest + (384 - 72);
			for (int i = 0; i < 72; i++) {
				*(scanlineLeft++) = currentBGColor;
				*(scanlineRight++) = currentBGColor;
			}
			scanlineDest += 384;
		}
	}
}

void nes_ppu::finishFrame(bool bSkippedFrame) {
	if (bSkippedFrame) {
		return;
	}

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