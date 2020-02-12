#include "platform.h"
#include "debug.h"
#include "nes.h"

#include "ptune2_simple/Ptune2_direct.h"
#include "scope_timer/scope_timer.h"

#include "calctype/fonts/arial_small/arial_small.c"		// For Debug Output
// #include "../../calctype/fonts/consolas_intl/consolas_intl.c"	// for FAQS

#include "imageDraw.h"
#include "settings.h"
#include "frontend.h"

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

	nesSettings.Load();

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

	nesFrontend.SetMainMenu();
	nesFrontend.Run();

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