#include "platform.h"
#include "nes.h"
#include "debug.h"

#include "ptune2_simple/Ptune2_direct.h"
#include "scope_timer/scope_timer.h"

#include "calctype/fonts/commodore/commodore.c"		// For Menus
#include "calctype/fonts/arial_small/arial_small.c"		// For Debug Output
// #include "../../calctype/fonts/consolas_intl/consolas_intl.c"	// for FAQS

#include "imageDraw.h"
#include "settings.h"

const bool bRebuildGfx = false;

bool shouldExit = false;

void shutdown() {
}

#if TARGET_WINSIM
int simmain(void) {
#else
int main(void) {
#endif
	// prepare for full color mode
	Bdisp_EnableColor(1);
	EnableStatusArea(3);
	DrawFrame(0);

	//Ptune2_LoadSetting(PT2_DOUBLE);

	reset_printf();
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);
	printf("NESizm Initializing...");
	DrawFrame(0x0000);

	ScopeTimer::InitSystem();

	nes_settings.SetDefaults();

	// allocate nes_carts on stack
	unsigned char stackBanks[STATIC_CACHED_ROM_BANKS * 8192] ALIGN(256);
	nesCart.allocateBanks(stackBanks);

#if TARGET_WINSIM
	// Dumps the palette overlay table to the log
	const int bitShift = 0;
	OutputLog("uint8 PatternTable[8 * 256] = {\n");
	for (uint32 i = 0; i < 256; i++) {
		uint8 bytes[8];
		for (int32 b = 7; b >= 0; b--) {
			uint32 bit = (i & (1 << b)) ? 1 << bitShift : 0;
			bytes[7-b] = bit;
		}
		if (i % 8 == 0) OutputLog("\t");
		OutputLog("%d,%d,%d,%d,%d,%d,%d,%d,", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
		if (i % 8 == 7) OutputLog("\n");
	}
	OutputLog("};\n\n");
#endif


	{
		Bdisp_Fill_VRAM(0, 3);
		DrawFrame(0);

		extern PrizmImage gfx_logo;
		extern PrizmImage gfx_bg_warp;
		extern PrizmImage gfx_nes;

		PrizmImage* logo = &gfx_logo;
		PrizmImage* bg = &gfx_bg_warp;
		PrizmImage* nes = &gfx_nes;

#if TARGET_WINSIM
		if (bRebuildGfx) {
			logo = PrizmImage::LoadImage("\\\\dev0\\gfx\\logo.bmp");
			bg = PrizmImage::LoadImage("\\\\dev0\\gfx\\rays.bmp");
			nes = PrizmImage::LoadImage("\\\\dev0\\gfx\\nes.bmp");
			logo->Compress();
			bg->Compress();
			nes->Compress();
			logo->ExportZX7("gfx_logo", "src\\gfx\\logo.cpp");
			bg->ExportZX7("gfx_bg_warp", "src\\gfx\\bg_warp.cpp");
			nes->ExportZX7("gfx_nes", "src\\gfx\\nes_gfx.cpp");
		}
#endif
		logo->Draw_Blit(5,5);
		bg->Draw_Blit(0, 71);
		nes->Draw_OverlayMasked(195, 38, 192);
		CalcType_Draw(&commodore, "=> Load ROM", 7, 140, COLOR_WHITE, 0, 0);
		CalcType_Draw(&commodore, "   View FAQ", 7, 156, COLOR_AQUAMARINE, 0, 0);
		CalcType_Draw(&commodore, "   Options", 7, 172, COLOR_AQUAMARINE, 0, 0);
		CalcType_Draw(&commodore, "   About", 7, 188, COLOR_AQUAMARINE, 0, 0);

		CalcType_Draw(&commodore, "@TSWilliamson", 255, 5, COLOR_AQUAMARINE, 0, 0);
		CalcType_Draw(&commodore, "v0.9", 340, 198, 0x302C, 0, 0);
	
		int key = 0;
		GetKey(&key);
	}

#if TARGET_WINSIM
	char romFileSystem[512] = { 0 };
	OPENFILENAME openStruct;
	memset(&openStruct, 0, sizeof(OPENFILENAME));
	openStruct.lStructSize = sizeof(OPENFILENAME);
	openStruct.lpstrFilter = "NES ROMs\0*.nes\0";
	openStruct.lpstrFile = romFileSystem;
	openStruct.nMaxFile = 512;
	openStruct.lpstrInitialDir = "%USERPROFILE%\\Documents\\Prizm\\ROM";
	openStruct.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

	if (!GetOpenFileName(&openStruct)) {
		return -1;
	}

	char* ROMdir = strstr(romFileSystem, "ROM\\");
	char romFile[512];
	strcpy(romFile, "\\\\fls0\\");
	strcat(romFile, ROMdir + 4);
#else
	const char* romFile = "\\\\fls0\\SMB.nes";
#endif

	if (nesCart.loadROM(romFile)) {
		Bdisp_PutDisp_DD();

		SetQuitHandler(shutdown);

		cpu6502_Init();
		mainCPU.reset();

		while (!shouldExit) {
			cpu6502_Step();
			if (mainCPU.clocks >= mainCPU.ppuClocks) ppu_step();
			if (mainCPU.irqClocks && mainCPU.clocks >= mainCPU.irqClocks) cpu6502_IRQ();
		}

		nesCart.unload();
	} else {
		int key = 0; 
		GetKey(&key);
	}

	//Ptune2_LoadSetting(PT2_DEFAULT);

	ScopeTimer::Shutdown();

	return 0;
}

#if !TARGET_WINSIM
void* operator new(unsigned int size){
	return malloc(size);
}
void operator delete(void* addr) {
	free(addr);
}
void* operator new[](unsigned int size) {
	return malloc(size);
}
void operator delete[](void* addr) {
	free(addr);
}
DeviceType getDeviceType() {
	return (size_t) GetVRAMAddress() == 0xAC000000 ? DT_CG50 : DT_CG20;
}

#else
DeviceType getDeviceType() {
	return DT_Winsim;
}
#endif