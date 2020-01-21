
// NES input function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "settings.h"

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

void input_writeStrobe(unsigned char value) {
	value &= 1;

	if (value != curStrobe) {
		curStrobe = value;

		if (value == 0) {
			// reset strobing
			buttonMarch1 = 0;
			buttonMarch2 = 0;
		}
	}
}

inline unsigned char readButton(int buttonNo) {
	return keyDown_fast(nes_settings.keyMap[buttonNo]);
}

unsigned char input_readController1() {
	if (curStrobe) {
		// always return A
		return readButton(NES_A);
	} else {
		if (buttonMarch1 < 8) {
			return readButton(buttonMarch1++);
		} else {
			return 1;
		}
	}
}

unsigned char input_readController2() {
	// not currently supported
	if (!curStrobe) {
		if (buttonMarch2++ < 8) {
			return 0;
		} else {
			return 1;
		}
	} else {
		return 0;
	}
}