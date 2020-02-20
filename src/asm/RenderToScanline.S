
# void RenderToScanline(unsigned char*patternTable, int chr, int palette, uint8* buffer);
# Using C code for blitting method.. currently crashes on device, not sure why 
# r4 = uint8* patternTable;
# r5 = int chr; (spriteIndex * !6)
# r6 = int palette;
# r7 = uint8* buffer; (4-byte aligned)
# symbol = uint8* OverlayTable
# 
# r0 - r3 = scratch

    .global _RenderToScanline
    .text; .align 4
_RenderToScanline:
	// r5 = &patternTable[chr] (the bottom plane)
	add r4, r5

	// r1 = OverlayTable
	mov.l .OverlayTable, r1
	// mov.l @r1, r1

	// r2 = 3
	mov #3, r2

	// r0 = patternTable[chr+8]
	mov.b @(8,r5),r0
	extu.b r0,r0
	// << 3
	shld r2,r0
	// r3 = OverlayTable[r0] (bitPlane2[0]) << 1
	mov.l @(r0,r1),r3
	shll r3
	// incorporate the palette
	or r6, r3	
	
	//  r4 = OverlayTable[r0+4] (bitPlane2[1]) << 1
	add #4,r0
	mov.l @(r0,r1),r4
	shll r4
	// incorporate the palette
	or r6, r4	

	// r0 = patternTable[chr]
	mov.b @r5,r0
	extu.b r0,r0
	// << 3
	shld r2,r0
	// r2 = OverlayTable[r0] (bitPlane1[0])
	mov.l @(r0,r1),r2
	// add into our plane1
	or r2, r3

	//  r2 = OverlayTable[r0+4] (bitPlane1[1]) << 1
	add #4,r0
	mov.l @(r0,r1),r2
	// add into our plane2
	or r2, r4

	// now our scanline contents are in r3 and r4, assign to buffer:
	mov.l r3,@r7
	rts
	mov.l r4,@(4,r7)
	
    .text; .align 4
.OverlayTable:
	.long	_OverlayTable

.END