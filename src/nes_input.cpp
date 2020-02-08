
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
bool isDown[22];

void input_cacheKeys() {
	for (int buttonNo = 0; buttonNo < 22; buttonNo++) {
		isDown[buttonNo] = keyDown_fast(nesSettings.keyMap[buttonNo]);
	}
}

inline unsigned char readButton(int buttonNo) {
	return isDown[buttonNo];
}

void input_writeStrobe(unsigned char value) {
	value &= 1;

	if (value != curStrobe) {
		curStrobe = value;

		if (value == 0) {
			// reset strobing
			buttonMarch1 = 0;
			buttonMarch2 = 0;
		}

		mainCPU.specialMemory[0x16] = readButton(NES_A) | 0x40;
		mainCPU.specialMemory[0x17] = 0x40;
	}
}

void input_readController1() {
	if (!curStrobe) {
		if (buttonMarch1 < 8) {
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
			mainCPU.specialMemory[0x17] = 0x40;
		} else {
			mainCPU.specialMemory[0x17] = 0x41;
		}
	}
}