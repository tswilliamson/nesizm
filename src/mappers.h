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
#define MMC1_BOARD_TYPE nesCart.registers[0]
#define MMC1_S_ROM 0
#define MMC1_SOROM 1
#define MMC1_SUROM 2
#define MMC1_SXROM 3

// register 1 holds the shift register bit
#define MMC1_SHIFT_BIT nesCart.registers[1]

// register 2 holds the shift register value
#define MMC1_SHIFT_VALUE nesCart.registers[2]

// register 3 holds PRG bank mode
#define MMC1_PRG_BANK_MODE nesCart.registers[3]

// register 4 holds CHR bank mode
#define MMC1_CHR_BANK_MODE nesCart.registers[4]

// register 5 holds the RAM chip enable bit (0 = enabled)
#define MMC1_RAM_DISABLE nesCart.registers[5]

// register 6 holds the bank containing the ram
#define MMC1_RAM_BANK nesCart.registers[6]

// register 7 holds the PRG bank in lower 16 KB
#define MMC1_PRG_BANK_1 nesCart.registers[7]

// register 8 holds the PRG bank in higher 16 KB 
#define MMC1_PRG_BANK_2 nesCart.registers[8]

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MMC2 (Punch Out mapper)

// register 0 is the current low CHR map latch value : 0 = FD, 1 = FE
#define MMC2_LOLATCH nesCart.registers[0]

// register 1 is the current high CHR map latch value : 0 = FD, 1 = FE
#define MMC2_HILATCH nesCart.registers[1]

// register 2 is the current bank copied into the PRG select
#define MMC2_PRG_SELECT nesCart.registers[2]

// register 3 is the current 4K CHR bank selected for low CHR map, latched with FD
#define MMC2_CHR_LOW_FD nesCart.registers[3]

// register 4 is the current 4K CHR bank selected for low CHR map, latched with FE
#define MMC2_CHR_LOW_FE nesCart.registers[4]

// register 5 is the current 4K CHR bank selected for high CHR map, latched with FD
#define MMC2_CHR_HIGH_FD nesCart.registers[5]

// register 6 is the current 4K CHR bank selected for high CHR map, latched with FE
#define MMC2_CHR_HIGH_FE nesCart.registers[6]

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color Dreams Mapper 11

#define Mapper11_PRG_SELECT nesCart.registers[0]

#define Mapper11_CHR_SELECT nesCart.registers[1]

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rambo-1 Mapper 64

#define Mapper64_BANK_SELECT nesCart.registers[0]
#define Mapper64_R0 nesCart.registers[1]
#define Mapper64_R1 nesCart.registers[2]
#define Mapper64_R2 nesCart.registers[3]
#define Mapper64_R3 nesCart.registers[4]
#define Mapper64_R4 nesCart.registers[5]
#define Mapper64_R5 nesCart.registers[6]
#define Mapper64_R6 nesCart.registers[7]
#define Mapper64_R7 nesCart.registers[8]
#define Mapper64_R8 nesCart.registers[9]
#define Mapper64_R9 nesCart.registers[10]
#define Mapper64_RF nesCart.registers[11]

#define Mapper64_IRQ_LATCH nesCart.registers[12]
#define Mapper64_IRQ_MODE nesCart.registers[13]
#define Mapper64_IRQ_ENABLE nesCart.registers[14]
#define Mapper64_IRQ_COUNT nesCart.registers[15]
#define Mapper64_IRQ_CLOCKS nesCart.registers[16]