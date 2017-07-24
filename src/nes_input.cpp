
// NES input function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

bool keyDown_fast(unsigned char keyCode);

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

// in report order
#define NES_A 0
#define NES_B 1
#define NES_SELECT 2
#define NES_START 3
#define NES_UP 4
#define NES_DOWN 5
#define NES_LEFT 6
#define NES_RIGHT 7

// emulator keys
#define NES_SAVESTATE 8
#define NES_LOADSTATE 9

static int keyMap[10];

unsigned char curStrobe = 0;
unsigned int buttonMarch1 = 0;
unsigned int buttonMarch2 = 0;

void input_Initialize() {
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
	return keyDown_fast(keyMap[buttonNo]);
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