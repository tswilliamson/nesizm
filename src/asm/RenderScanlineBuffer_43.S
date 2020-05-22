# Render 4:3 in assembly functions. Each stretchs a different pixel in the 4 -> 5 pixel sequence for interlacing

# void RenderScanlineBuffer_43_0(unsigned char* scanlineSrc, unsigned int* scanlineDest, uint16* workingPalette)
// r4 = unsigned int* scanlineSrc; (1 bit shifted color palette index)
// r5 = unsigned int* scanlineDest;

// symbol = uint16* ppu_workingPalette 

// r0 - r3 = scratch

    .global _RenderScanlineBuffer_43_0_ASM
    .text; .align 4
_RenderScanlineBuffer_43_0_ASM:
	// r2 = ppu_workingPalette
	mov.l .WorkingPalette, r2
	mov.l @r2, r2

	// loop
	bra _DoRender_0
	mov #20,r3

_DoRenderAgain_0:
	add #30,r5
_DoRender_0:

	// r0 = ppu_workingPalette[scanlineSrc[x]]
	mov.b @r4+,r0		// r0 = *scanlineSrc++
	mov.w @(r0,r2),r0	// r0 = ppu_workingPalette[*scanlineSrc]
	mov.w r0,@r5		// scanlineDest = r0
	mov.w r0,@(2,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(4,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0 
	mov.w r0,@(6,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(8,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(10,r5)
	mov.w r0,@(12,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(14,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(16,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(18,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(20,r5)
	mov.w r0,@(22,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(24,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(26,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0

	dt r3
	bf/s _DoRenderAgain_0
	mov.w r0,@(28,r5)

	rts
	nop

    .global _RenderScanlineBuffer_43_1_ASM
    .text; .align 4
_RenderScanlineBuffer_43_1_ASM:
	// r2 = ppu_workingPalette
	mov.l .WorkingPalette, r2
	mov.l @r2, r2

	// loop
	bra _DoRender_1
	mov #20,r3

_DoRenderAgain_1:
	add #30,r5
_DoRender_1:

	// r0 = ppu_workingPalette[scanlineSrc[x]]
	mov.b @r4+,r0		// r0 = *scanlineSrc++
	mov.w @(r0,r2),r0	// r0 = ppu_workingPalette[*scanlineSrc]
	mov.w r0,@r5		// scanlineDest = r0
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(2,r5)
	mov.w r0,@(4,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0 
	mov.w r0,@(6,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(8,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(10,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(12,r5)
	mov.w r0,@(14,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(16,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(18,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(20,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(22,r5)
	mov.w r0,@(24,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(26,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0

	dt r3
	bf/s _DoRenderAgain_1
	mov.w r0,@(28,r5)

	rts
	nop

    .global _RenderScanlineBuffer_43_2_ASM
    .text; .align 4
_RenderScanlineBuffer_43_2_ASM:
	// r2 = ppu_workingPalette
	mov.l .WorkingPalette, r2
	mov.l @r2, r2

	// loop
	bra _DoRender_2
	mov #20,r3

_DoRenderAgain_2:
	add #30,r5
_DoRender_2:

	// r0 = ppu_workingPalette[scanlineSrc[x]]
	mov.b @r4+,r0		// r0 = *scanlineSrc++
	mov.w @(r0,r2),r0	// r0 = ppu_workingPalette[*scanlineSrc]
	mov.w r0,@r5		// scanlineDest = r0
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(2,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(4,r5)
	mov.w r0,@(6,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(8,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(10,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(12,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(14,r5)
	mov.w r0,@(16,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(18,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(20,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(22,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(24,r5)
	mov.w r0,@(26,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0

	dt r3
	bf/s _DoRenderAgain_2
	mov.w r0,@(28,r5)

	rts
	nop

	
    .global _RenderScanlineBuffer_43_3_ASM
    .text; .align 4
_RenderScanlineBuffer_43_3_ASM:
	// r2 = ppu_workingPalette
	mov.l .WorkingPalette, r2
	mov.l @r2, r2

	// loop
	bra _DoRender_3
	mov #20,r3

_DoRenderAgain_3:
	add #30,r5
_DoRender_3:

	// r0 = ppu_workingPalette[scanlineSrc[x]]
	mov.b @r4+,r0		// r0 = *scanlineSrc++
	mov.w @(r0,r2),r0	// r0 = ppu_workingPalette[*scanlineSrc]
	mov.w r0,@r5		// scanlineDest = r0
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(2,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(4,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0 
	mov.w r0,@(6,r5)
	mov.w r0,@(8,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(10,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(12,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(14,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(16,r5)
	mov.w r0,@(18,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(20,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(22,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(24,r5)
	
	mov.b @r4+,r0
	mov.w @(r0,r2),r0
	mov.w r0,@(26,r5)

	dt r3
	bf/s _DoRenderAgain_3
	mov.w r0,@(28,r5)

	rts
	nop

    .text; .align 4
.WorkingPalette:
	.long	_ppu_workingPalette

.END