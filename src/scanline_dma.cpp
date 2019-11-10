// Resolves scanline direct via direct DMA to LCD

#if TARGET_PRIZM

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "scope_timer/scope_timer.h"

#define LCD_GRAM	0x202
#define LCD_BASE	0xB4000000
#define SYNCO() __asm__ volatile("SYNCO\n\t":::"memory");

// DMA0 operation register
#define DMA0_DMAOR	(volatile unsigned short*)0xFE008060
#define DMA0_CHCR_0	(volatile unsigned*)0xFE00802C
#define DMA0_SAR_0	(volatile unsigned*)0xFE008020
#define DMA0_DAR_0	(volatile unsigned*)0xFE008024
#define DMA0_TCR_0	(volatile unsigned*)0xFE008028

// resolved line buffer in on chip mem
static unsigned short* scanGroup[2] = { (unsigned short*) 0xE5007000, (unsigned short*) 0xE5017000 };
static int curDMABuffer = 0;

// scanline buffer goes in on chip mem 2
unsigned int ppu_scanlineBufferPtr[256 + 16 * 2];
unsigned int* ppu_scanlineBuffer = ppu_scanlineBufferPtr;

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

	DmaDrawStrip(scanGroup[curDMABuffer], scanBufferSize);
	curDMABuffer = 1 - curDMABuffer;

	curScan = 0;
}

void resolveScanline_DMA() {
	TIME_SCOPE();

	const int bufferLines = 16;	// 512 bytes * 16 lines = 8192
	const int scanBufferSize = bufferLines * 256 * 2;

	// resolve le line
	unsigned int* scanlineSrc = &ppu_scanlineBuffer[16];	// with clipping
	unsigned int* scanlineDest = (unsigned int*) (scanGroup[curDMABuffer] + 256 * curScan);
	for (int i = 0; i < 128; i++, scanlineSrc += 2) {
		*(scanlineDest++) = (ppu_rgbPalettePtrShifted[ppu_workingPalette[scanlineSrc[0]]]) | ppu_rgbPalettePtr[ppu_workingPalette[scanlineSrc[1]]];
	}

	curScan++;
	if (curScan == bufferLines) {
		// send DMA
		flushScanBuffer(64, 319, ppu_scanline - 13 - bufferLines + 1, ppu_scanline - 13, scanBufferSize);
	}
}

void finishFrame_DMA() {
	TIME_SCOPE();

	// frame end.. kill DMA operations to make sure they stay in sync
	DmaWaitNext();
	*((volatile unsigned*)MSTPCR0) &= ~(1 << 21);//Clear bit 21
	curScan = 0;
}

#endif