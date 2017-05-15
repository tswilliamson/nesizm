
// Main NES ppu implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"

nes_ppu nesPPU;

nes_ppu::nes_ppu() {
	memset(this, 0, sizeof(nes_ppu));
}
