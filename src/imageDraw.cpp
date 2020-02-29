
#include "platform.h"
#include "debug.h"

#include "imageDraw.h"
#include "zx7/zx7.h"

// src image data is always big endian to match calculator specs,
// so flip bytes for windows
#if TARGET_WINSIM
inline uint16 color_correct(uint16 color) {
	return ((color & 0xFF) << 8) | ((color & 0xFF00) >> 8);
}
inline void colorcopy(uint8* dst, const uint8* src, uint32 size) {
	uint16* dstColor = (uint16*) dst;
	const uint16* srcColor = (const uint16*) src;
	size >>= 1;

	for (int c = 0; c < size; ++c, ++dstColor, ++srcColor) {
		*dstColor = color_correct(*srcColor);
	}
}
#else
#define color_correct(color) (color)
#define colorcopy memcpy
#endif

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

void DecompressBlock(const uint8*& srcData, uint8* intoBlock, uint32 decompressedSize) {
	uint32 compressedSize = srcData[0] * 256 + srcData[1];

	// if compressed size is 0 then the compression wasn't efficient for this block and was stored decompressed
	if (compressedSize != 0) {
		ZX7Decompress(srcData + 2, intoBlock, decompressedSize);
		// compressed size includes 2 bytes for size, see PrizmImage::Compress
		srcData += compressedSize;
	} else {
		memcpy(intoBlock, srcData + 2, decompressedSize);
		srcData += decompressedSize + 2;
	}
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
			DecompressBlock(curData, block, curSize);
			BlitBlock(block, curSize, x, y);
		} else {
			BlitBlock(curData, curSize, x, y);
			curData += curSize;
		}

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
		colorcopy(vram, colorData, width * 2);
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
			DecompressBlock(curData, block, curSize);
			BlitMaskedBlock(block, curSize, x, y);
		} else {
			BlitMaskedBlock(curData, curSize, x, y);
			curData += curSize;
		}

		sizeLeft -= curSize;
	}
}

// draws an uncompressed block to vram: could be moved to assembly for speed later!
void PrizmImage::BlitMaskedBlock(const uint8* colorData, uint32 size, int32 x, int32& y) const {
	DebugAssert(x >= 0 && x + width <= LCD_WIDTH_PX);
	DebugAssert(y >= 0);

	uint16* vram = (uint16*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x);
	const uint16* curColor = (const uint16*) colorData;
	while (size > 0) {
		DebugAssert(y < LCD_HEIGHT_PX);
		for (uint32 x = 0; x < width; ++x, ++curColor) {
			if (*curColor) {
				vram[x] = color_correct(curColor[0]);
			}
		}
		vram += LCD_WIDTH_PX;
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
			DecompressBlock(curData, block, curSize);
			OverlayBlock(block, curSize, x, y, alpha);
		} else {
			OverlayBlock(curData, curSize, x, y, alpha);
			curData += curSize;
		}

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
			*curVRAM = alphaBlendRGB565(color_correct(*curColor++), *curVRAM, alpha);
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
		Draw_BlitMasked(x, y);
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
			DecompressBlock(curData, block, curSize);
			OverlayMaskedBlock(block, curSize, x, y, alpha);
		} else {
			OverlayMaskedBlock(curData, curSize, x, y, alpha);
			curData += curSize;
		}

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
				*curVRAM = alphaBlendRGB565(color_correct(curColor[x]), *curVRAM, alpha);
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
			DecompressBlock(curData, block, curSize);
			AdditiveBlock(block, curSize, x, y, alpha);
		} else {
			AdditiveBlock(curData, curSize, x, y, alpha);
			curData += curSize;
		}

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
			*curVRAM = additiveBlendRGB565(color_correct(*curColor++), *curVRAM, alpha);
			++curVRAM;
		}
		vram += LCD_WIDTH_PX * 2;
		size -= width * 2;
		y++;
	}

	DebugAssert(size == 0);
}

// draws a filled solid color rect to VRAM 
void PrizmImage::Draw_FilledRect(int32 x, int32 y, int32 w, int32 h, uint16 color) {
	uint16* vram = (uint16*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x);
	int32 nextLine = LCD_WIDTH_PX - w;
	while (h--) {
		int32 fX = w;
		while (fX--) {
			*(vram++) = color;
		}
		vram += nextLine;
	}
}

// draws a border solid color rect to VRAM 
void PrizmImage::Draw_BorderRect(int32 x, int32 y, int32 w, int32 h, int32 thickness, uint16 color) {
	Draw_FilledRect(x, y, w, thickness, color);
	Draw_FilledRect(x, y + h - thickness, w, thickness, color);
	h -= thickness * 2;
	y += thickness;
	Draw_FilledRect(x, y, thickness, h, color);
	Draw_FilledRect(x + w - thickness, y, thickness, h, color);
}

// draws a up-down gradient color rect to VRAM 
void PrizmImage::Draw_GradientRect(int32 x, int32 y, int32 w, int32 h, uint16 color1, uint16 color2) {
	int32 baseR = (color1 & 0b1111100000000000) >> 11;
	int32 stepR = (color2 & 0b1111100000000000) >> 11;
	stepR -= baseR;
	int32 baseG = (color1 & 0b0000011111100000) >> 5;
	int32 stepG = (color2 & 0b0000011111100000) >> 5;
	stepG -= baseG;
	int32 baseB = (color1 & 0b0000000000011111);
	int32 stepB = (color2 & 0b0000000000011111);
	stepB -= baseB;

	uint16* vram = (uint16*)GetVRAMAddress() + (y * LCD_WIDTH_PX + x);
	int32 nextLine = LCD_WIDTH_PX - w;
	int32 lineNum = 0;
	int32 fH = h;
	while (fH--) {
		uint16 color =
			((baseR + stepR * lineNum / h) << 11) |
			((baseG + stepG * lineNum / h) << 5) |
			((baseB + stepB * lineNum / h));
		lineNum++;

		int32 fX = w;
		while (fX--) {
			*(vram++) = color;
		}
		vram += nextLine;
	}
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
			dstPix[0] = ((srcPix[2] & 0xF8) >> 0) | ((srcPix[1] & 0xE0) >> 5);
			dstPix[1] = ((srcPix[1] & 0x1C) << 3) | ((srcPix[0] & 0xF8) >> 3);
			dstPix += 2;
			srcPix += 3;
		}
	}

	free(rgbPixels);

	Bfile_CloseFile_OS(file);

	return img;
}

void PrizmImage::Compress() {
	DebugAssert(isCompressed == false);

	const uint32 blockSize = GetBlockSize(*this);
	const uint8* curData = data;
	uint32 sizeLeft = width * height * 2;

	// temporary compressed memory, moved back into image data
	uint8* compressedMem = (uint8*) malloc(width * height * 4);
	uint8* compressedTarget = compressedMem;
	while (sizeLeft > 0) {
		uint32 curSize = sizeLeft > blockSize ? blockSize : sizeLeft;

		uint8* outData = nullptr;
		uint32 compressedSize = ZX7Compress(curData, curSize, &outData);
		if (compressedSize > 0) {
			DebugAssert(compressedSize <= 0xFFFD);
			compressedSize += 2;
			compressedTarget[0] = (compressedSize & 0xFF00) >> 8;
			compressedTarget[1] = (compressedSize & 0x00FF);
			memcpy(compressedTarget + 2, outData, compressedSize - 2);
			compressedTarget += compressedSize;
			free(outData);
		} else {
			compressedTarget[0] = 0;
			compressedTarget[1] = 0;
			memcpy(compressedTarget + 2, curData, curSize);
			compressedTarget += curSize + 2;
		}

		curData += curSize;
		sizeLeft -= curSize;
	}

	// copy only the compressed data to our memory and free the buffer
	uint32 compressedTotalSize = compressedTarget - compressedMem;
	free(data);
	data = (uint8*)malloc(compressedTotalSize);
	memcpy(data, compressedMem, compressedTotalSize);
	free(compressedMem);

	isCompressed = true;
}

void WriteFileBytes(int file, const uint8* data, int numBytes, bool end) {
	char dataDefinition[512];
	strcpy(dataDefinition, "\t");
	for (uint32 x = 0; x < numBytes; x++) {
		char num[8];
		sprintf(num, "0x%02X,", *data++);
		strcat(dataDefinition, num);
	}
	if (!end) {
		strcat(dataDefinition, "\n");
	} else {
		dataDefinition[strlen(dataDefinition) - 1] = 0;
		strcat(dataDefinition, "\n};\n\n");
	}

	Bfile_WriteFile_OS(file, dataDefinition, strlen(dataDefinition));
}

void WriteDataBlock(int file, const uint8* data, uint32 dataSize, bool hasMore) {
	uint32 sizeLeft = dataSize;
	while (sizeLeft) {
		uint32 curSize = sizeLeft > 32 ? 32 : sizeLeft;
		sizeLeft -= curSize;
		WriteFileBytes(file, data, curSize, sizeLeft == 0 && !hasMore);
		data += curSize;
	}

	if (hasMore) {
		Bfile_WriteFile_OS(file, "\n", 1);
	}
}

void PrizmImage::ExportZX7(const char* imageName, const char* targetSrcFile) const {
	char fileTarget[512];
	strcpy(fileTarget, "\\\\dev0\\");
	strcat(fileTarget, targetSrcFile);

	unsigned short fileAsName[512];
	Bfile_StrToName_ncpy(fileAsName, fileTarget, strlen(fileTarget) + 1);

	size_t size = 0;
	Bfile_CreateEntry_OS(fileAsName, CREATEMODE_FILE, &size);
	int file = Bfile_OpenFile_OS(fileAsName, WRITE, 0);
	if (file != -1) {
		const uint32 dataSize = width * height * 2;

		{
			const char* headerString =
				"// Autogenerated PrizmImage file\n"
				"#include \"platform.h\"\n"
				"#include \"imageDraw.h\"\n"
				"\n";
			Bfile_WriteFile_OS(file, headerString, strlen(headerString));
		}

		{
			char dataDeclaration[256];
			sprintf(dataDeclaration, "static uint8 %s_data[]={\n", imageName);
			Bfile_WriteFile_OS(file, dataDeclaration, strlen(dataDeclaration));
		}

		{
			const uint8* curData = data;
			if (isCompressed == false) {
				WriteDataBlock(file, curData, dataSize, false);
			} else {
				uint32 blockSize = GetBlockSize(*this);

				int32 totalSizeLeft = dataSize;
				while (totalSizeLeft > 0) {
					totalSizeLeft -= blockSize;

					uint16 compressedSize = curData[0] * 256 + curData[1];
					WriteDataBlock(file, curData, compressedSize, totalSizeLeft > 0);
					curData += compressedSize;
				}
			}
		}

		{
			char imageDefinition[512];
			sprintf(imageDefinition,
				"PrizmImage %s={\n"
				"\t%d,\n" // width
				"\t%d,\n" // height
				"\t%d,\n" // isCompressed
				"\t%s_data\n" // data
				"};\n"
				, imageName, width, height, isCompressed, imageName);
			Bfile_WriteFile_OS(file, imageDefinition, strlen(imageDefinition));
		}

		Bfile_CloseFile_OS(file);
	}
}
#endif