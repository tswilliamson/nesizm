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

	reset_printf();
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);
	printf("NESizm Initializing...");
	DrawFrame(0x0000);

	ScopeTimer::InitSystem();

	nesSettings.Load();

	// allocate nes_carts on stack
	unsigned char stackBanks[STATIC_CACHED_ROM_BANKS * 8192] ALIGN(256);
	nesCart.allocateBanks(stackBanks);

	nesFrontend.SetMainMenu();
	nesFrontend.Run();

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
#endif