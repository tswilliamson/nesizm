#include "platform.h"
#include "nes.h"
#include "ptune2_simple/Ptune2_direct.h"
#include "scope_timer/scope_timer.h"

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

	Ptune2_LoadSetting(PT2_DOUBLE);

	reset_printf();
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);
	printf("NESizm Initializing...");
	DrawFrame(0x0000);

	ScopeTimer::InitSystem();

	// allocate nes_carts on stack
	unsigned char stackBanks[NUM_CACHED_ROM_BANKS * 8192] ALIGN(256);
	nesCart.allocateBanks(stackBanks);

	input_Initialize();

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