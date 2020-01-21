#include "platform.h"
#include "nes.h"

#include "ptune2_simple/Ptune2_direct.h"
#include "scope_timer/scope_timer.h"

#include "calctype/fonts/commodore/commodore.c"		// For Menus
#include "calctype/fonts/arial_small/arial_small.c"		// For Debug Output
// #include "../../calctype/fonts/consolas_intl/consolas_intl.c"	// for FAQS

#include "imageDraw.h"
#include "settings.h"

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

	Ptune2_LoadSetting(PT2_DOUBLE);

	reset_printf();
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);
	printf("NESizm Initializing...");
	DrawFrame(0x0000);

	ScopeTimer::InitSystem();

	nes_settings.SetDefaults();

	// allocate nes_carts on stack
	unsigned char stackBanks[NUM_CACHED_ROM_BANKS * 8192] ALIGN(256);
	nesCart.allocateBanks(stackBanks);


#if TARGET_WINSIM
	{
		Bdisp_Fill_VRAM(0, 3);
		PrizmImage* logo = PrizmImage::LoadImage("\\\\dev0\\gfx\\logo.bmp");
		logo->Compress();
		logo->Draw_Blit(5,5);
		PrizmImage* bg = PrizmImage::LoadImage("\\\\dev0\\gfx\\rays.bmp");
		bg->Compress();
		bg->Draw_Blit(0, 71);
		PrizmImage* nes = PrizmImage::LoadImage("\\\\dev0\\gfx\\nes.bmp");
		nes->Compress();
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

	Ptune2_LoadSetting(PT2_DEFAULT);

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