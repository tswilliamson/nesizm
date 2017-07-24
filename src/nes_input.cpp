
// NES input function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

unsigned char curStrobe = 0;
unsigned int buttonMarch1 = 0;
unsigned int buttonMarch2 = 0;

#define NES_A 0

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

unsigned char readButton(int buttonNo) {
	return 0;
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