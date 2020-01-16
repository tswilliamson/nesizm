#pragma once

#include "platform.h"

struct PrizmImage {
	uint16 width;
	uint16 height;
	uint8 isCompressed;
	uint8* data;

	// blits this image to VRAM
	void Blit(int32 x, int32 y) const;

#if TARGET_WINSIM
	// loadable BMP images available on winsim can be zx7 compressed, code can be setup to load/save from src media to
	// auto update source as images update
	static PrizmImage* LoadImage(const char* fileName);
	void Compress();
	void ExportZX7(const char* imageName, const char* targetSrcFile) const;
#endif

private:
	void BlitBlock(const uint8* colorData, uint32 size, int32 x, int32& y) const;
};

