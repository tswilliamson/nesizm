#include "platform.h"
#include "nes.h"

#include "../../calctype/calctype.inl"
#include "../../calctype/fonts/arial_small/arial_small.c"		// For Menus
// #include "../../calctype/fonts/consolas_intl/consolas_intl.c"	// for FAQS

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

	reset_printf();
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);
	printf("NESizm Initializing...");
	DrawFrame(0x0000);

	// allocate nes_carts on stack
	unsigned char stackBanks[NUM_CACHED_ROM_BANKS * 8192] ALIGN(256);
	nesCart.allocateROMBanks(stackBanks);


	const char* romFile = "\\\\fls0\\DonkeyKong.nes";
	if (nesCart.loadROM(romFile)) {
		Bdisp_PutDisp_DD();

		SetQuitHandler(shutdown);

		mainCPU.reset();

		while (1) {
			cpu6502_Step();
		}
	} else {
		int key = 0; 
		GetKey(&key);
	}

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