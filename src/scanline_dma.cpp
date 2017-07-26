// Resolves scanline direct via direct DMA to LCD

#if TARGET_PRIZM

#include "platform.h"
#include "debug.h"
#include "nes.h"

#define LCD_GRAM	0x202
#define LCD_BASE	0xB4000000
#define SYNCO() __asm__ volatile("SYNCO\n\t":::"memory");

// DMA0 operation register
#define DMA0_DMAOR	(volatile unsigned short*)0xFE008060
#define DMA0_CHCR_0	(volatile unsigned*)0xFE00802C
#define DMA0_SAR_0	(volatile unsigned*)0xFE008020
#define DMA0_DAR_0	(volatile unsigned*)0xFE008024
#define DMA0_TCR_0	(volatile unsigned*)0xFE008028

// resolved line buffer in on chip mem 1
#define scanGroup ((unsigned char*) 0xE5007000)
static int curDMABuffer = 0;

// scanline buffer goes in on chip mem 2
unsigned int* ppu_scanlineBuffer = ((unsigned int*)0xE5017000);

static int curScan = 0;

void DmaWaitNext(void) {
	while (1) {
		if ((*DMA0_DMAOR) & 4)//Address error has occurred stop looping
			break;
		if ((*DMA0_CHCR_0) & 2)//Transfer is done
			break;

		//condSoundUpdate();
	}

	SYNCO();
	*DMA0_CHCR_0 &= ~1;
	*DMA0_DMAOR = 0;
}

void DmaDrawStrip(void* srcAddress, unsigned int size) {
	// disable dma so we can issue new command
	*DMA0_CHCR_0 &= ~1;
	*DMA0_DMAOR = 0;

	*((volatile unsigned*)MSTPCR0) &= ~(1 << 21);//Clear bit 21
	*DMA0_SAR_0 = ((unsigned int)srcAddress);           //Source address is VRAM
	*DMA0_DAR_0 = LCD_BASE & 0x1FFFFFFF;				//Destination is LCD
	*DMA0_TCR_0 = size / 32;							//Transfer count bytes/32
	*DMA0_CHCR_0 = 0x00101400;
	*DMA0_DMAOR |= 1;//Enable DMA on all channels
	*DMA0_DMAOR &= ~6;//Clear flags

	*DMA0_CHCR_0 |= 1;//Enable channel0 DMA
}

inline void flushScanBuffer(int startX, int endX, int startY, int endY, int scanBufferSize) {
	DmaWaitNext();

	Bdisp_WriteDDRegister3_bit7(1);
	Bdisp_DefineDMARange(startX, endX, startY, endY);
	Bdisp_DDRegisterSelect(LCD_GRAM);

	DmaDrawStrip(&scanGroup[curDMABuffer * scanBufferSize], scanBufferSize);
	curDMABuffer = 1 - curDMABuffer;

	curScan = 0;
}

void resolveScanline_DMA() {

	const int bufferLines = 6;	// 512 bytes * 6 lines = 3072
	const int scanBufferSize = bufferLines * 256 * 2;

	curScan++;
	if (curScan == bufferLines) {
		// we've rendered as much as we can buffer, resolve to pixels and DMA it:
		ppu_scanlineBuffer = ((unsigned int*)0xE5017000);
		unsigned short* scanlineDest = (unsigned short*)&scanGroup[curDMABuffer*scanBufferSize];
		for (int i = 0; i < bufferLines; i++) {

			unsigned int* scanlineSrc = &ppu_scanlineBuffer[16];	// with clipping
			for (int i = 0; i < 256; i++, scanlineSrc++) {
				*(scanlineDest++) = ppu_rgbPalettePtr[ppu_palette[*scanlineSrc]];
			}

			ppu_scanlineBuffer += 256 + 16 * 2;

			// condSoundUpdate();
		}

		// send DMA
		flushScanBuffer(64, 319, ppu_scanline - 13 - bufferLines + 1, ppu_scanline - 13, scanBufferSize);

		// move line buffer to front
		ppu_scanlineBuffer = ((unsigned int*)0xE5017000);
	} else {
		// move line buffer down a line
		ppu_scanlineBuffer += 256 + 16 * 2;
	}
}

void finishFrame_DMA() {
	// frame end.. kill DMA operations to make sure they stay in sync
	DmaWaitNext();
	*((volatile unsigned*)MSTPCR0) &= ~(1 << 21);//Clear bit 21
	curScan = 0;
}

#endif