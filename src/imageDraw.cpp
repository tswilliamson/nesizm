
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

// blits this image to VRAM
void PrizmImage::Blit(int32 x, int32 y) const {
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