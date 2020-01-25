
# void RenderToScanline(int* scanline, unsigned int tileRow);
// r4 = uint8* patternTable;
// r5 = int chr; (spriteIndex * !6)
// r6 = int palette;
// r7 = x;
// symbol = unsigned char* ppu_scanlineBuffer <-- aligned as 1024
// 
// r0 - r3 = scratch


    .global _RenderToScanlineOld
    .text; .align 4
_RenderToScanlineOld:
	// r0 = patternTable
	mov r4, r0
	// set r4 up to be patternTable[chr] (the bottom plane)
	mov.b @(r0, r5), r4
	// set r5 up to be patternTable[chr+8] (the top plane)
	add #8,r5
	mov.b @(r0, r5), r5
	
	// r0 = ppu_scanlineBuffer
	mov.l .PPUBuffer, r0
	mov.l @r0, r0

	// r0 += (x + 7) (byte offset)
	add r7, r0
	add #7, r0
	
	// for 8 pixels: palette (r6) = P, topPlane (r5) = T, bottomPlane (r4) = B:
	//  buffer[x--] = PPPPPPTB, topPlane >>= 1, bottomPlane >> 1
.rept 8
	shlr2 r6   					// palette >>= 2
	rotcr r5   					// topplane >> T
	rotcl r6   					// palette = (palette << 1) | T
	rotcr r4   					// bottomPlane >> T
	rotcl r6   					// palette = (palette << 1) | T
	mov.b r6,@r0				// buffer[x] = palette
	dt r0						// x--
.endr
	rts
	nop
	
    .text; .align 4
.PPUBuffer:
	.long	_ppu_scanlineBuffer

.END