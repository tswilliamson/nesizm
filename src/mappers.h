#pragma once

// NES mapper specifics

#include "nes.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC1

// register 0 holds board disambiguation
// 0 : S*ROM (most common formats with same interface. This includes SNROM whose RAM enable bit won't be implemented)
// 1 : SOROM (extra RAM bank, bit 3 of chr bank select selects RAM bank)
// 2 : SUROM (512 KB PRG ROM, high chr bank bit selects bank)
// 3 : SXROM (32 KB RAM, bits 3-4 of chr bank select selects RAM bank high bits)
#define S_ROM 0
#define SOROM 1
#define SUROM 2
#define SXROM 3

// register 1 holds the shift register bit
// register 2 holds the shift register value

// register 3 holds PRG bank mode
// register 4 holds CHR bank mode
// register 5 holds the RAM chip enable bit (0 = enabled)
// register 6 holds the bank containing the ram

// register 7 holds the PRG bank in lower 16 KB
// register 8 holds the PRG bank in higher 16 KB 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNROM (switchable CHR banks)

// registers[0] = current mapped CHR bank
#define CNROM_CUR_CHR_BANK nesCart.registers[0]

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC3 (most popular mapper with IRQ)

// register 0-7 holds the 8 bank registers
#define MMC3_BANK_REG nesCart.registers

// register 8 is the bank select register (control values)
#define MMC3_BANK_SELECT nesCart.registers[8]

// register 9 is the IRQ counter latch value (the set value on reload)
#define MMC3_IRQ_SET nesCart.registers[9]

// register 10 is the IRQ counter reload flag
#define MMC3_IRQ_RELOAD nesCart.registers[10]

// register 11 is the actual IRQ counter value
#define MMC3_IRQ_COUNTER nesCart.registers[11]

// register 12 is the IRQ interrupt triggle enable/disable flag
#define MMC3_IRQ_ENABLE nesCart.registers[12]

// register 13 is the IRQ latch (when set, IRQ is dispatched with each A12 jump)
#define MMC3_IRQ_LATCH nesCart.registers[13]

// registers 16-19 contain an integer of the last time the IRQ counter was reset, used to fix IRQ timing since we are cheating by performing logic at beginning ot scanline
#define MMC3_IRQ_LASTSET *((unsigned int*) &nesCart.registers[16])

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AOROM (switches nametables for single screen mirroring)

// registers[0] = current nametable page (1 or 0)
#define AOROM_CUR_NAMETABLE nesCart.registers[0]

#define AOROM_NAMEBANK nesCart.availableROMBanks - 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC2 (Punch Out mapper)

// register 0 is the current low CHR map latch value : 0 = FD, 1 = FE
// register 1 is the current high CHR map latch value : 0 = FD, 1 = FE

// register 2 is the current bank copied into the PRG select

// register 3 is the current 4K CHR bank selected for low CHR map, latched with FD
// register 4 is the current 4K CHR bank selected for low CHR map, latched with FE
// register 5 is the current 4K CHR bank selected for high CHR map, latched with FD
// register 6 is the current 4K CHR bank selected for high CHR map, latched with FE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color Dreams Mapper 11

#define Mapper11_PRG_SELECT nesCart.registers[8]
#define Mapper11_CHR_SELECT nesCart.registers[8]
