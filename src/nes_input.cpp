
// NES input function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "settings.h"
#include "nes_cpu.h"

#if !TARGET_WINSIM
// returns true if the key is down, false if up
bool keyDown_fast(unsigned char keyCode) {
	static const unsigned short* keyboard_register = (unsigned short*)0xA44B0000;

	int row, col, word, bit;
	row = keyCode % 10;
	col = keyCode / 10 - 1;
	word = row >> 1;
	bit = col + 8 * (row & 1);
	return (keyboard_register[word] & (1 << bit));
}
#endif

unsigned char curStrobe = 0;
unsigned int buttonMarch1 = 0;
unsigned int buttonMarch2 = 0;
bool isDown[NES_MAX_KEYS];

void input_cacheKeys() {
	for (int buttonNo = 0; buttonNo < NES_MAX_KEYS; buttonNo++) {
		int keyCode = nesSettings.keyMap[buttonNo];
		if (keyCode) {
			isDown[buttonNo] = keyDown_fast(keyCode);
		}
	}

	int turboBit = 1 << nesSettings.GetSetting(ST_TurboSetting);
	if (nesPPU.frameCounter & turboBit) {
		if (isDown[NES_P1_TURBO_A]) isDown[NES_P1_A] = true;
		if (isDown[NES_P1_TURBO_B]) isDown[NES_P1_B] = true;
	}
}

inline unsigned char readButton(int buttonNo) {
	return isDown[buttonNo];
}

bool EmulatorSettings::CheckCachedKey(NesKeys key) {
	return isDown[(int)key];
}

void input_writeStrobe(unsigned char value) {
	value &= 1;

	{
		curStrobe = value;

		if (value == 0) {
			// reset strobing
			buttonMarch1 = 0;
			buttonMarch2 = 0;
		}

		mainCPU.specialMemory[0x16] = readButton(NES_P1_A) | 0x40;
		mainCPU.specialMemory[0x17] = readButton(NES_P2_A) | 0x40;
	}
}

void input_readController1() {
	if (!curStrobe) {
		if (buttonMarch1 < 8) {
			// the first induced latch will be due to a latched write to $4016, which is why buttonMatch is preincremented for controller 1
			mainCPU.specialMemory[0x16] = readButton(buttonMarch1++) | 0x40;
		} else {
			mainCPU.specialMemory[0x16] = 0x41;
		}
	}
}

void input_readController2() {
	// not currently supported
	if (!curStrobe) {
		if (buttonMarch2 < 8) {
			buttonMarch2++;
			mainCPU.specialMemory[0x17] = readButton(buttonMarch2 + NES_P2_A) | 0x40;
		} else {
			mainCPU.specialMemory[0x17] = 0x41;
		}
	}
}