
#include "platform.h"
#include "debug.h"
#include "nes.h"

#include "settings.h"

bool GameGenieCode::set(const char* withValue) {
	strncpy(code, withValue, 8);

	if (update())
		return true;

	clear();
	return false;
}

void GameGenieCode::clear() {
	code[0] = 0;
	cachedAddr = 0;
	cachedSet = 0;
	cachedCmp = 0;
	bDoCompare = false;
}

struct Bits {
	uint8 bit0 : 1;
	uint8 bit1 : 1;
	uint8 bit2 : 1;
	uint8 bit3 : 1;
};

bool SetBitsWithValue(Bits& intoBits, unsigned int withBits) {
	intoBits.bit0 = withBits & 1;
	intoBits.bit1 = (withBits & 2) >> 1;
	intoBits.bit2 = (withBits & 4) >> 2;
	intoBits.bit3 = (withBits & 8) >> 3;
	return true;
}

bool SetBitsWithCode(Bits& intoBits, char withChar) {
	switch (withChar) {
		case 'A': return SetBitsWithValue(intoBits, 0b0000);
		case 'P': return SetBitsWithValue(intoBits, 0b0001);
		case 'Z': return SetBitsWithValue(intoBits, 0b0010);
		case 'L': return SetBitsWithValue(intoBits, 0b0011);
		case 'G': return SetBitsWithValue(intoBits, 0b0100);
		case 'I': return SetBitsWithValue(intoBits, 0b0101);
		case 'T': return SetBitsWithValue(intoBits, 0b0110);
		case 'Y': return SetBitsWithValue(intoBits, 0b0111);
		case 'E': return SetBitsWithValue(intoBits, 0b1000);
		case 'O': return SetBitsWithValue(intoBits, 0b1001);
		case 'X': return SetBitsWithValue(intoBits, 0b1010);
		case 'U': return SetBitsWithValue(intoBits, 0b1011);
		case 'K': return SetBitsWithValue(intoBits, 0b1100);
		case 'S': return SetBitsWithValue(intoBits, 0b1101);
		case 'V': return SetBitsWithValue(intoBits, 0b1110);
		case 'N': return SetBitsWithValue(intoBits, 0b1111);
	}
	return false;
}

unsigned char FromBitScramble(uint8 bit7, uint8 bit6, uint8 bit5, uint8 bit4, uint8 bit3, uint8 bit2, uint8 bit1, uint8 bit0) {
	return
		(bit7 << 7) |
		(bit6 << 6) |
		(bit5 << 5) |
		(bit4 << 4) |
		(bit3 << 3) |
		(bit2 << 2) |
		(bit1 << 1) |
		(bit0 << 0);
}

bool GameGenieCode::update() {
	bDoCompare = false;

	uint8 targetAddrHi = 0;
	uint8 targetAddrLo = 0;
	uint8 targetCmp = 0;
	uint8 targetSet = 0;
	Bits C[8];

	// decode the first 6 characters
	for (int i = 0; i < 6; i++) {
		if (!SetBitsWithCode(C[i], code[i])) {
			return false;
		}
	}

	// decode address
	targetAddrHi = FromBitScramble(1, C[3].bit2, C[3].bit1, C[3].bit0, C[4].bit3, C[5].bit2, C[5].bit1, C[5].bit0);
	targetAddrLo = FromBitScramble(C[1].bit3, C[2].bit2, C[2].bit1, C[2].bit0, C[3].bit3, C[4].bit2, C[4].bit1, C[4].bit0);

	// decode set and compare
	if (C[2].bit3) {
		// 8 character code
		for (int i = 6; i < 8; i++) {
			if (!SetBitsWithCode(C[i], code[i])) {
				return false;
			}
		}

		bDoCompare = true;

		targetSet = FromBitScramble(C[0].bit3, C[1].bit2, C[1].bit1, C[1].bit0, C[7].bit3, C[0].bit2, C[0].bit1, C[0].bit0);
		targetCmp = FromBitScramble(C[6].bit3, C[7].bit2, C[7].bit1, C[7].bit0, C[5].bit3, C[6].bit2, C[6].bit1, C[6].bit0);
	} else {
		// 6 character code
		bDoCompare = false;
		targetSet = FromBitScramble(C[0].bit3, C[1].bit2, C[1].bit1, C[1].bit0, C[5].bit3, C[0].bit2, C[0].bit1, C[0].bit0);
	}

	cachedAddr = (targetAddrHi << 8) | targetAddrLo;
	cachedSet = targetSet;
	cachedCmp = targetCmp;

	return true;
}