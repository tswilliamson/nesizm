// Resolves scanline direct via direct DMA to LCD

#if TARGET_PRIZM

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "settings.h"
#include "scope_timer/scope_timer.h"
#include "ptune2_simple/Ptune2_direct.h"
#include "snd/snd.h"

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

static unsigned int curScan = 0;
static unsigned int dmaFrame = 0;

void DmaWaitNext(void) {
	while (1) {
		if ((*DMA0_DMAOR) & 4)//Address error has occurred stop looping
			break;
		if ((*DMA0_CHCR_0) & 2)//Transfer is done
			break;
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

void FillBlackBorder() {
	// DrawFrame is too inconsistent, do it ourselves!
	memset(scanGroup[0], 0, 8192);
	memset(scanGroup[1], 0, 8192);

	flushScanBuffer(0, 395, 0, 4, 396 * 2 * 4);
	flushScanBuffer(0, 395, 216, 224, 396 * 2 * 8);
	flushScanBuffer(0, 5, 0, 224, 6 * 224 * 2);
	flushScanBuffer(390, 395, 0, 224, 6 * 224 * 2);

	DmaWaitNext();
}


#if 0
inline void RenderScanlineBufferWide1(unsigned char* scanlineSrc, unsigned int* scanlineDest) {
	for (int i = 0; i < 60; i++, scanlineSrc += 4) {
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[0] >> 1] << 16) | ppu_workingPalette[scanlineSrc[1] >> 1];
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[1] >> 1] << 16) | ppu_workingPalette[scanlineSrc[2] >> 1];
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[2] >> 1] << 16) | ppu_workingPalette[scanlineSrc[3] >> 1];
	}
}

inline void RenderScanlineBufferWide2(unsigned char* scanlineSrc, unsigned int* scanlineDest) {
	for (int i = 0; i < 60; i++, scanlineSrc += 4) {
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[0] >> 1] << 16) | ppu_workingPalette[scanlineSrc[0] >> 1];
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[1] >> 1] << 16) | ppu_workingPalette[scanlineSrc[2] >> 1];
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[3] >> 1] << 16) | ppu_workingPalette[scanlineSrc[3] >> 1];
	}
}

inline void RenderScanlineBuffer(unsigned char* scanlineSrc, unsigned int* scanlineDest) {
	for (int i = 0; i < 120; i++, scanlineSrc += 2) {
		*(scanlineDest++) = (ppu_workingPalette[scanlineSrc[0] >> 1] << 16) | ppu_workingPalette[scanlineSrc[1] >> 1];
	}
}
#else
extern "C" {
	void RenderScanlineBuffer_ASM(unsigned char* scanlineSrc, unsigned int* scanlineDest);
	void RenderScanlineBufferWide1_ASM(unsigned char* scanlineSrc, unsigned int* scanlineDest);
	void RenderScanlineBufferWide2_ASM(unsigned char* scanlineSrc, unsigned int* scanlineDest);
};
#define RenderScanlineBuffer RenderScanlineBuffer_ASM
#define RenderScanlineBufferWide1 RenderScanlineBufferWide1_ASM
#define RenderScanlineBufferWide2 RenderScanlineBufferWide2_ASM
#endif

void nes_ppu::resolveScanline(int scrollOffset) {
	TIME_SCOPE();

	if (nesSettings.GetSetting(ST_StretchScreen) == 1) {
		const unsigned int bufferLines = 8;	// 720 bytes * 8 lines = 5760
		const unsigned int scanBufferSize = bufferLines * 360 * 2;

		// resolve le line
		unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
		unsigned int* scanlineDest = (unsigned int*)(scanGroup[curDMABuffer] + 360 * curScan);
		if ((dmaFrame + nesPPU.scanline) & 1) {
			RenderScanlineBufferWide1(scanlineSrc, scanlineDest);
		} else {
			RenderScanlineBufferWide2(scanlineSrc, scanlineDest);
		}

		curScan++;
		if (curScan == bufferLines) {
			// send DMA
			flushScanBuffer(18, 377, nesPPU.scanline - 9 - bufferLines + 1, nesPPU.scanline - 9, scanBufferSize);
		}
	} else {
		const unsigned int bufferLines = 14;	// 480 bytes * 14 lines = 6720
		const unsigned int scanBufferSize = bufferLines * 240 * 2;

		// resolve le line
		unsigned char* scanlineSrc = &nesPPU.scanlineBuffer[8 + scrollOffset];	// with clipping
		unsigned int* scanlineDest = (unsigned int*)(scanGroup[curDMABuffer] + 240 * curScan);
		RenderScanlineBuffer(scanlineSrc, scanlineDest);

		curScan++;
		if (curScan == bufferLines) {
			// send DMA
			flushScanBuffer(78, 317, nesPPU.scanline - 9 - bufferLines + 1, nesPPU.scanline - 9, scanBufferSize);
		}
	}
}

void nes_ppu::finishFrame(bool bSkippedFrame) {
	// run frame timing
	if (nesSettings.GetSetting(ST_Speed) != 4 && nesSettings.CheckCachedKey(NES_FASTFORWARD) == false) {
		// use TMU1 to establish time between frames
		static unsigned int counterStart = 0x7FFFFFFF;		// max int
		unsigned int counterRegs = 0x0004;
		unsigned int tmu1Clocks = 0;
		if (REG_TMU_TCR_1 != counterRegs || (REG_TMU_TSTR & 2) == 0) {
			// tmu1 needs to be set up
			REG_TMU_TSTR &= ~(1 << 1);

			REG_TMU_TCOR_1 = counterStart;   // max int
			REG_TMU_TCNT_1 = counterStart;
			REG_TMU_TCR_1 = counterRegs;    // max division, no interrupt needed

			// enable TMU1
			REG_TMU_TSTR |= (1 << 1);

			tmu1Clocks = 0;
		} else {
			// expected TMU1 based sim frame time (for 60.09 FPS)
			unsigned int simFrameTime = Ptune2_GetPLLFreq() * 235 >> Ptune2_GetPFCDiv();

			// PAL is slower (50 Hz)
			if (nesCart.isPAL) {
				simFrameTime = simFrameTime * 5 / 6;
			}

			// adjust expected sim time by various speed settings
			switch (nesSettings.GetSetting(ST_Speed)) {
				case 0: simFrameTime = simFrameTime * 4 / 5; break;
				case 2: simFrameTime = simFrameTime * 10 / 9; break;
				case 3: simFrameTime = simFrameTime * 4 / 3; break;
			}

			// clamp speed by waiting for frame time only on rendered frames
			static uint32 accumulatedSimTime = 0;
			accumulatedSimTime += simFrameTime;
			
			if (!bSkippedFrame) {
				tmu1Clocks = counterStart - REG_TMU_TCNT_1;

				// auto frameskip adjustment based on pre clamped time
				static unsigned int collectedTime = 0;
				static unsigned int collectedFrames = 0;
				collectedTime += tmu1Clocks;
				collectedFrames++;
				if (nesSettings.GetSetting(ST_FrameSkip) == 0 && collectedFrames > 7) {
					int speedUpTime = simFrameTime * 124 / 128;
					int slowDownTime = simFrameTime * 129 / 128;
					int avgTime = collectedTime / 8;
					collectedTime = 0;
					collectedFrames = 0;

					if (avgTime < speedUpTime && nesPPU.autoFrameSkip != 0) {
						nesPPU.autoFrameSkip--;
					} else if (avgTime > slowDownTime && nesPPU.autoFrameSkip != 24) {
						nesPPU.autoFrameSkip++;
					}
				}

				int rtcBackup = RTC_GetTicks();
				int maxIter = 1000;
				while (tmu1Clocks < accumulatedSimTime && RTC_GetTicks() - rtcBackup < 10 && maxIter--) {
					// sleep .1 milliseconds at a time til we are ready for the frame
					CMT_Delay_micros(100);
					tmu1Clocks = counterStart - REG_TMU_TCNT_1;

					condSoundUpdate();
				}

				accumulatedSimTime = 0;
				counterStart = REG_TMU_TCNT_1;
			}
		}
	}

	if (!bSkippedFrame) {
		// frame end.. kill DMA operations to make sure they stay in sync
		DmaWaitNext();
		*((volatile unsigned*)MSTPCR0) &= ~(1 << 21);//Clear bit 21
		curScan = 0;
		dmaFrame++;
	}
}

#endif