
# void RenderScanlineBuffer(unsigned char* scanlineSrc, unsigned int* scanlineDest)
// r4 = unsigned int* scanlineSrc;
// r5 = unsigned int* scanlineDest;

// symbol = int* ppu_workingPalette 

// r0 - r3 = scratch

    .global _RenderScanlineBuffer_ASM
    .text; .align 4
_RenderScanlineBuffer_ASM:
	// r2 = ppu_workingPalette
	mov.l .WorkingPalette, r2
	//mov.l @r2, r2

	// loop
	mov #32,r3

	
	bra _DoRender
    .text; .align 4
.WorkingPalette:
	.long	_ppu_workingPalette

_DoRenderAgain:
	add #16,r5
_DoRender:

	// r0 = ppu_workingPalette[scanlineSrc[x]]
	mov.b @r4,r0		// r0 = *scanlineSrc
	shll2 r0
	mov.l @(r0,r2),r0	// r0 = ppu_workingPalette[*scanlineSrc]
	mov.w r0,@r5		// scanlineDest = r0

	mov.b @(1,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0
	mov.w r0,@(2,r5)
	
	mov.b @(2,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0
	mov.w r0,@(4,r5)
	
	mov.b @(3,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0 
	mov.w r0,@(6,r5)
	
	mov.b @(4,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0
	mov.w r0,@(8,r5)
	
	mov.b @(5,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0
	mov.w r0,@(10,r5)
	
	mov.b @(6,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0
	mov.w r0,@(12,r5)
	
	mov.b @(7,r4),r0
	shll2 r0
	mov.l @(r0,r2),r0
	mov.w r0,@(14,r5)

	dt r3
	bf/s _DoRenderAgain
	add #8,r4

	rts
	nop


.END