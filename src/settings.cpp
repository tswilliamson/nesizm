
#include "settings.h"

EmulatorSettings nes_settings;

void EmulatorSettings::SetDefaults() {
	memset(&keyMap, 0, sizeof(keyMap));
	keyMap[NES_A] = 78;			// SHIFT
	keyMap[NES_B] = 68;			// OPTN
	keyMap[NES_SELECT] = 39;		// F5
	keyMap[NES_START] = 29;		// F6
	keyMap[NES_RIGHT] = 27;
	keyMap[NES_LEFT] = 38;
	keyMap[NES_UP] = 28;
	keyMap[NES_DOWN] = 37;
	keyMap[NES_SAVESTATE] = 43;	// 'S'
	keyMap[NES_LOADSTATE] = 25;   // 'L'

	// simulator only defaults
#if TARGET_WINSIM
	keyMap[NES_SAVESTATE] = 59;	// maps to F3
	keyMap[NES_LOADSTATE] = 49;	// maps to F4
#endif
}