
// NES memory function implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_mem nesMem;

unsigned char nes_mem::readSpecial(unsigned int addr) {
	DebugAssert(0);
	return 0;
}

void nes_mem::writeSpecial(unsigned int addr, unsigned char value) {
	DebugAssert(0);
}