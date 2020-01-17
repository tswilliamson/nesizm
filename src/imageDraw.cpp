
#include "platform.h"
#include "debug.h"

#include "imageDraw.h"
#include "zx7/zx7.h"

// size of compressed image chunk (also draws in blocks)
#define MAX_BLOCK_SIZE 4096

static uint32 GetBlockSize(const PrizmImage& forImage) {
	uint32 blockSize = forImage.width * forImage.height * 2;
	if (blockSize > MAX_BLOCK_SIZE) {
		uint32 linesPerBlock = MAX_BLOCK_SIZE / (forImage.width * 2);
		blockSize = linesPerBlock * forImage.width * 2;
		DebugAssert(blockSize <= MAX_BLOCK_SIZE);
	}

	return blockSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Blitting

// blits this image to VRAM
void PrizmImage::Draw_Blit(int32 x, int32 y) const {
	uint8 block[MAX_BLOCK_SIZE];
	const uint8* curData = data;
	uint32 sizeLeft = width * height * 2;
	const uint32 blockSize = GetBlockSize(*this);

	while (sizeLeft > 0) {
		uint32 curSize = sizeLeft > blockSize ? blockSize : sizeLeft;
		if (isCompressed) {
			ZX7Decompress(curData, block, curSize);
			BlitBlock(block, curSize, x, y);
		} else {
			BlitBlock(curData, curSize, x, y);
		}

		curData += curSize;
		sizeLeft -= curSize;
	}
}

// draws an uncompressed block to vram: could be moved to assembly for speed later!
void PrizmImage::BlitBlock(const uint8* colorData, uint32 size, int32 x, int32& y) const {
	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0);

	uint8* vram = (uint8*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x) * 2;
	while (size > 0) {
		DebugAssert(y < LCD_HEIGHT_PX);
		memcpy(vram, colorData, width * 2);
		vram += LCD_WIDTH_PX * 2;
		colorData += width * 2;
		size -= width * 2;
		y++;
	}

	DebugAssert(size == 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Masked Blitting

// blits this image to VRAM
void PrizmImage::Draw_BlitMasked(int32 x, int32 y) const {
	uint8 block[MAX_BLOCK_SIZE];
	const uint8* curData = data;
	uint32 sizeLeft = width * height * 2;
	const uint32 blockSize = GetBlockSize(*this);

	while (sizeLeft > 0) {
		uint32 curSize = sizeLeft > blockSize ? blockSize : sizeLeft;
		if (isCompressed) {
			ZX7Decompress(curData, block, curSize);
			BlitMaskedBlock(block, curSize, x, y);
		}
		else {
			BlitMaskedBlock(curData, curSize, x, y);
		}

		curData += curSize;
		sizeLeft -= curSize;
	}
}

// draws an uncompressed block to vram: could be moved to assembly for speed later!
void PrizmImage::BlitMaskedBlock(const uint8* colorData, uint32 size, int32 x, int32& y) const {
	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0);

	uint8* vram = (uint8*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x) * 2;
	while (size > 0) {
		DebugAssert(y < LCD_HEIGHT_PX);
		for (uint32 x = 0; x < width * 2; x++) {
			if (colorData[x] || colorData[x + 1]) {
				vram[x] = colorData[x];
				vram[x + 1] = colorData[x + 1];
			}
		}
		vram += LCD_WIDTH_PX * 2;
		colorData += width * 2;
		size -= width * 2;
		y++;
	}

	DebugAssert(size == 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Overlay

// Fast RGB565 pixel blending
// Found in a pull request for the Adafruit framebuffer library. Clever!
// https://github.com/tricorderproject/arducordermini/pull/1/files#diff-d22a481ade4dbb4e41acc4d7c77f683d
inline uint16 alphaBlendRGB565(uint32 fg, uint32 bg, uint8 alpha) {
	// Converts  0000000000000000rrrrrggggggbbbbb
	//     into  00000gggggg00000rrrrr000000bbbbb
	// with mask 00000111111000001111100000011111
	// This is useful because it makes space for a parallel fixed-point multiply
	bg = (bg | (bg << 16)) & 0b00000111111000001111100000011111;
	fg = (fg | (fg << 16)) & 0b00000111111000001111100000011111;

	// This implements the linear interpolation formula: result = bg * (1.0 - alpha) + fg * alpha
	// This can be factorized into: result = bg + (fg - bg) * alpha
	// alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
	uint32 result = (fg - bg) * alpha; // parallel fixed-point multiply of all components
	result >>= 5;
	result += bg;
	result &= 0b00000111111000001111100000011111; // mask out fractional parts
	return (uint16)((result >> 16) | result); // contract result
}

// overlays this image to VRAM w/ translucency (0-255)
void PrizmImage::Draw_Overlay(int32 x, int32 y, uint8 alpha) const {
	if (alpha >= 252) {
		// alphaBlendRGB565 will be just as good as blit for alpha >= 252
		Draw_Blit(x, y);
		return;
	}

	// Alpha converted from [0..255] to [0..31]
	alpha = (alpha + 4) >> 3;

	if (alpha == 0) {
		// similarly they will just fail to render anything
		return;
	}

	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0 && y + height <= LCD_HEIGHT_PX);

	uint8 block[MAX_BLOCK_SIZE];
	const uint8* curData = data;
	uint32 sizeLeft = width * height * 2;
	const uint32 blockSize = GetBlockSize(*this);

	while (sizeLeft > 0) {
		uint32 curSize = sizeLeft > blockSize ? blockSize : sizeLeft;
		if (isCompressed) {
			ZX7Decompress(curData, block, curSize);
			OverlayBlock(block, curSize, x, y, alpha);
		}
		else {
			OverlayBlock(curData, curSize, x, y, alpha);
		}

		curData += curSize;
		sizeLeft -= curSize;
	}
}

// draws an uncompressed block to vram: could be moved to assembly for speed later!
void PrizmImage::OverlayBlock(const uint8* colorData, uint32 size, int32 x, int32& y, uint8 alpha) const {
	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0);

	uint8* vram = (uint8*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x) * 2;
	const uint16* curColor = (const uint16*) colorData;
	while (size > 0) {
		DebugAssert(y < LCD_HEIGHT_PX);
		uint16* curVRAM = (uint16*) vram;
		for (int x = 0; x < width; x++) {
			*curVRAM = alphaBlendRGB565(*curColor++, *curVRAM, alpha);
			++curVRAM;
		}
		vram += LCD_WIDTH_PX * 2;
		size -= width * 2;
		y++;
	}

	DebugAssert(size == 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Overlay

// overlays this image to VRAM w/ translucency (0-255)
void PrizmImage::Draw_OverlayMasked(int32 x, int32 y, uint8 alpha) const {
	if (alpha >= 252) {
		// alphaBlendRGB565 will be just as good as blit for alpha >= 252
		Draw_Blit(x, y);
		return;
	}

	// Alpha converted from [0..255] to [0..31]
	alpha = (alpha + 4) >> 3;

	if (alpha == 0) {
		// similarly they will just fail to render anything
		return;
	}

	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0 && y + height <= LCD_HEIGHT_PX);

	uint8 block[MAX_BLOCK_SIZE];
	const uint8* curData = data;
	uint32 sizeLeft = width * height * 2;
	const uint32 blockSize = GetBlockSize(*this);

	while (sizeLeft > 0) {
		uint32 curSize = sizeLeft > blockSize ? blockSize : sizeLeft;
		if (isCompressed) {
			ZX7Decompress(curData, block, curSize);
			OverlayMaskedBlock(block, curSize, x, y, alpha);
		}
		else {
			OverlayMaskedBlock(curData, curSize, x, y, alpha);
		}

		curData += curSize;
		sizeLeft -= curSize;
	}
}

// draws an uncompressed block to vram: could be moved to assembly for speed later!
void PrizmImage::OverlayMaskedBlock(const uint8* colorData, uint32 size, int32 x, int32& y, uint8 alpha) const {
	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0);

	uint8* vram = (uint8*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x) * 2;
	const uint16* curColor = (const uint16*)colorData;
	while (size > 0) {
		DebugAssert(y < LCD_HEIGHT_PX);
		uint16* curVRAM = (uint16*)vram;
		for (int x = 0; x < width; x++) {
			if (curColor[x]) {
				*curVRAM = alphaBlendRGB565(curColor[x], *curVRAM, alpha);
			}
			++curVRAM;
		}
		curColor += width;
		vram += LCD_WIDTH_PX * 2;
		size -= width * 2;
		y++;
	}

	DebugAssert(size == 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Additive

// Fast RGB565 additive pixel
inline uint16 additiveBlendRGB565(uint32 fg, uint32 bg, uint8 alpha) {
	const uint32 r_Mask = 0b1111100000000000;
	const uint32 g_Mask = 0b0000011111100000;
	const uint32 b_Mask = 0b0000000000011111;

	uint32 c_r = (((bg & r_Mask) << 5) + (fg & r_Mask) * alpha) >> 5;
	uint32 c_g = (((bg & g_Mask) << 5) + (fg & g_Mask) * alpha) >> 5;
	uint32 c_b = (((bg & b_Mask) << 5) + (fg & b_Mask) * alpha) >> 5;

	uint32 r_overflow = (c_r & 0b10000000000000000);
	uint32 g_overflow = (c_g & 0b100000000000);
	uint32 b_overflow = (c_b & 0b100000);
	
	if (r_overflow) c_r = r_Mask;
	if (g_overflow) c_g = g_Mask;
	if (b_overflow) c_b = b_Mask;

	return (c_r & r_Mask) | (c_g & g_Mask) | (c_b & b_Mask);
}

// overlays this image to VRAM w/ additive (0-255)
void PrizmImage::Draw_Additive(int32 x, int32 y, uint8 alpha) const {
	// Alpha converted from [0..255] to [0..31]
	alpha = (alpha + 4) >> 3;

	if (alpha == 0) {
		// similarly they will just fail to render anything
		return;
	}

	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0 && y + height <= LCD_HEIGHT_PX);

	uint8 block[MAX_BLOCK_SIZE];
	const uint8* curData = data;
	uint32 sizeLeft = width * height * 2;
	const uint32 blockSize = GetBlockSize(*this);

	while (sizeLeft > 0) {
		uint32 curSize = sizeLeft > blockSize ? blockSize : sizeLeft;
		if (isCompressed) {
			ZX7Decompress(curData, block, curSize);
			AdditiveBlock(block, curSize, x, y, alpha);
		}
		else {
			AdditiveBlock(curData, curSize, x, y, alpha);
		}

		curData += curSize;
		sizeLeft -= curSize;
	}
}

// draws an uncompressed block to vram: could be moved to assembly for speed later!
void PrizmImage::AdditiveBlock(const uint8* colorData, uint32 size, int32 x, int32& y, uint8 alpha) const {
	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0);

	uint8* vram = (uint8*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x) * 2;
	const uint16* curColor = (const uint16*)colorData;
	while (size > 0) {
		DebugAssert(y < LCD_HEIGHT_PX);
		uint16* curVRAM = (uint16*)vram;
		for (int x = 0; x < width; x++) {
			*curVRAM = additiveBlendRGB565(*curColor++, *curVRAM, alpha);
			++curVRAM;
		}
		vram += LCD_WIDTH_PX * 2;
		size -= width * 2;
		y++;
	}

	DebugAssert(size == 0);
}

#if TARGET_WINSIM
PrizmImage* PrizmImage::LoadImage(const char* fileName) {
	unsigned short fileAsName[512];
	Bfile_StrToName_ncpy(fileAsName, fileName, strlen(fileName) + 1);
	int file = Bfile_OpenFile_OS(fileAsName, READ, 0);

	// read the 54-byte header
	uint8 header[54];
	Bfile_ReadFile_OS(file, header, 54, -1); 
	
	// extract image height and width from header
	int16 width = header[18] + 256 * header[19];
	int16 height = header[22] + 256 * header[23];
	int16 depth = header[28] + 256 * header[29];
	if (height < 0) height = -height;

	// only support 24 bit depth for now
	DebugAssert(depth == 24);

	PrizmImage* img = new PrizmImage;
	img->width = width;
	img->height = height;
	img->isCompressed = false;
	img->data = (uint8*) malloc(width * height * 2);

	int32 rowSize = ((3 * width + 3) / 4) * 4;
	uint8* rgbPixels = (uint8*)malloc(rowSize * height);
	Bfile_ReadFile_OS(file, rgbPixels, rowSize * height, -1);

	for (int32 y = 0; y <  height; y++) {
		// do image y flip
		uint8* dstPix = img->data + width * (height - 1 - y) * 2;
		uint8* srcPix = rgbPixels + rowSize * y;

		for (int32 x = 0; x < width; x++) {
			dstPix[1] = ((srcPix[2] & 0xF8) >> 0) | ((srcPix[1] & 0xE0) >> 5);
			dstPix[0] = ((srcPix[1] & 0x1C) << 3) | ((srcPix[0] & 0xF8) >> 3);
			dstPix += 2;
			srcPix += 3;
		}
	}

	free(rgbPixels);

	Bfile_CloseFile_OS(file);

	return img;
}
void PrizmImage::Compress() {
	// TODO
}
void PrizmImage::ExportZX7(const char* imageName, const char* targetSrcFile) const {
	// TODO
}
#endif