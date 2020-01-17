#pragma once

#include "platform.h"

struct PrizmImage {
	uint16 width;
	uint16 height;
	uint8 isCompressed;
	uint8* data;

	// blits this image to VRAM
	void Draw_Blit(int32 x, int32 y) const;

	// blits this image to VRAM w/ a black mask
	void Draw_BlitMasked(int32 x, int32 y) const;

	// overlays this image to VRAM w/ translucency (0-255)
	void Draw_Overlay(int32 x, int32 y, uint8 alpha) const;

	// overlays this image to VRAM w/ translucency (0-255) and a black mask
	void Draw_OverlayMasked(int32 x, int32 y, uint8 alpha) const;

	// draws this image to VRAM additively w/ alpha (0-255)
	void Draw_Additive(int32 x, int32 y, uint8 alpha) const;

#if TARGET_WINSIM
	// loadable BMP images available on winsim can be zx7 compressed, code can be setup to load/save from src media to
	// auto update source as images update
	static PrizmImage* LoadImage(const char* fileName);
	void Compress();
	void ExportZX7(const char* imageName, const char* targetSrcFile) const;
#endif

private:
	void BlitBlock(const uint8* colorData, uint32 size, int32 x, int32& y) const;
	void BlitMaskedBlock(const uint8* colorData, uint32 size, int32 x, int32& y) const;
	void OverlayBlock(const uint8* colorData, uint32 size, int32 x, int32& y, uint8 alpha) const;
	void OverlayMaskedBlock(const uint8* colorData, uint32 size, int32 x, int32& y, uint8 alpha) const;
	void AdditiveBlock(const uint8* colorData, uint32 size, int32 x, int32& y, uint8 alpha) const;
};

