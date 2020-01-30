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
unsigned char scanlineBufferPtr[256 + 16 * 2];

// resolve assembly defines used by various .S files
uint16* ppu_workingPalette = &nesPPU.workingPalette[0];
unsigned char* ppu_scanlineBuffer = &scanlineBufferPtr[0];

void nes_ppu::initScanlineBuffer() {
	scanlineBuffer = scanlineBufferPtr;
}

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

void flushScanBuffer(int startX, int endX, int startY, int endY, int scanBufferSize) {
	TIME_SCOPE();

	DmaWaitNext();

	Bdisp_WriteDDRegister3_bit7(1);
	Bdisp_DefineDMARange(startX, endX, startY, endY);
	Bdisp_DDRegisterSelect(LCD_GRAM);

	DmaDrawStrip(scanGroup[curDMABuffer], scanBufferSize);
	curDMABuffer = 1 - curDMABuffer;

	curScan = 0;
}

#if 0
inline void RenderScanlineBuffer(unsigned char* scanlineSrc, unsigned int* scanlineDest) {
	for (int i = 0; i < 120; i++, scanlineSrc += 2) {
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[0] >> 1] << 16) | ppu_workingPalette[scanlineSrc[1] >> 1];
	}
}
#else
extern "C" {
	void RenderScanlineBuffer_ASM(unsigned char* scanlineSrc, unsigned int* scanlineDest);
};
#define RenderScanlineBuffer RenderScanlineBuffer_ASM
#endif

void nes_ppu::resolveScanline(int scrollOffset) {
	TIME_SCOPE();

	const int bufferLines = 16;	// 512 bytes * 16 lines = 8192
	const int scanBufferSize = bufferLines * 240 * 2;

	// resolve le line
	unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
	unsigned int* scanlineDest = (unsigned int*) (scanGroup[curDMABuffer] + 240 * curScan);
	RenderScanlineBuffer(scanlineSrc, scanlineDest);

	curScan++;
	if (curScan == bufferLines) {
		// send DMA
		flushScanBuffer(72, 311, nesPPU.scanline - 9 - bufferLines + 1, nesPPU.scanline - 9, scanBufferSize);
	}
}

void nes_ppu::finishFrame() {
	TIME_SCOPE();

	// frame end.. kill DMA operations to make sure they stay in sync
	DmaWaitNext();
	*((volatile unsigned*)MSTPCR0) &= ~(1 << 21);//Clear bit 21
	curScan = 0;
}

#endif