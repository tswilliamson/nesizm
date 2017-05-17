
// Macro'd opcode table for various expansion behavior as needed

// Opcodes are defined by addressing mode :
//  OPCODE_NON		none or implied
//  OPCODE_IMM		immediate
//  OPCODE_REL		relative
//  OPCODE_ABS		absolute
//  OPCODE_ABX		absolute X
//  OPCODE_ABY		absolute Y
//  OPCODE_IND		indirect
//  OPCODE_INX		indirect X
//  OPCODE_INY		indirect Y
//  OPCODE_ZRO		zero page
//  OPCODE_ZRX		zero page X
//  OPCODE_ZRY		zero page Y

// if the individual macro's aren't defined, then we are looking for standard behavior:
#if !defined(OPCODE_NONE) && defined(OPCODE)
#define OPCODE_NON		OPCODE
#define OPCODE_IMM		OPCODE
#define OPCODE_REL		OPCODE
#define OPCODE_ABS		OPCODE
#define OPCODE_ABX		OPCODE
#define OPCODE_ABY		OPCODE
#define OPCODE_IND		OPCODE
#define OPCODE_INX		OPCODE
#define OPCODE_INY		OPCODE
#define OPCODE_ZRO		OPCODE
#define OPCODE_ZRX		OPCODE
#define OPCODE_ZRY		OPCODE
#endif

// actual table, arguments are
// OPCODE(OpcodeNum, InstructionString, BaseClocks, Size, SpecialCode)

OPCODE_NON(	0x00,		"BRK",				7,	1,	{}		)
OPCODE_REL(	0x01,		"ORA ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO(	0x05,		"ORA $%02x",		*,	*,	{}		)
OPCODE_ZRO(	0x06,		"ASL $%02x",		*,	*,	{}		)
OPCODE_NON(	0x08,		"PHP",				*,	*,	{}		)
OPCODE_IMM( 0x09,		"ORA #%d",			*,	*,	{}		)
OPCODE_NON( 0x0a,		"ASL A",			*,	*,	{}		)
OPCODE_ABS( 0x0d,		"ORA $%04x",		*,	*,  {}		)
OPCODE_ABS( 0x0e,		"ASL $%04x",		*,	*,	{}		)

OPCODE_REL( 0x10,		"BPL %d",			*,	*,	{}		)
OPCODE_INY( 0x11,		"ORA ($%02x,Y)",    *,	*,	{}		)
OPCODE_ZRX( 0x15,		"ORA $%02x, X",		*,	*,	{}		)
OPCODE_ZRX( 0x16,		"ASL $%02x, X",		*,	*,	{}		)
OPCODE_NON( 0x18,		"CLC",				*,	*,	{}		)
OPCODE_ABY( 0x19,		"ORA $%04x, Y",		*,	*,	{}		)
OPCODE_ABX( 0x1d,		"ORA $%04x, X",		*,	*,	{}		)
OPCODE_ABX( 0x1e,		"ASL $%04x, X",		*,	*,	{}		)

OPCODE_ABS( 0x20,		"JSR $%04x",		*,	*,	{}		)
OPCODE_INX( 0x21,		"AND ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO( 0x24,		"BIT $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0x25,		"AND $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0x26,		"ROL $%02x",		*,	*,	{}		)
OPCODE_NON( 0x28,		"PLP",				*,	*,	{}		)
OPCODE_IMM( 0x29,		"AND #%d",			*,	*,	{}		)
OPCODE_NON( 0x2a,		"ROL A",			*,	*,	{}		)
OPCODE_ABS( 0x2c,		"BIT $%04x",		*,	*,	{}		)
OPCODE_ABS( 0x2d,		"AND $%04x",		*,	*,	{}		)
OPCODE_ABS( 0x2e,		"ROL $%04x",		*,	*,	{}		)

OPCODE_REL( 0x30,		"BMI %d",			*,	*,	{}		)
OPCODE_INY( 0x31,		"AND ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0x35,		"AND $%02x,X",		*,	*,	{}		)
OPCODE_ZRX( 0x36,		"ROL $%02x,X",		*,	*,	{}		)
OPCODE_NON( 0x38,		"SEC",				*,	*,	{}		)
OPCODE_ABY( 0x39,		"AND $%04x,Y",		*,	*,	{}		)
OPCODE_ABX( 0x3d,		"AND $%04x,X",		*,	*,	{}		)
OPCODE_ABX( 0x3e,		"ROL $%04x,X",		*,	*,	{}		)

OPCODE_NON( 0x40,		"RTI",				*,	*,	{}		)
OPCODE_INX( 0x41,		"EOR ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO( 0x45,		"EOR $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0x46,		"LSR $%02x",		*,	*,	{}		)
OPCODE_NON( 0x48,		"PHA",				*,	*,	{}		)
OPCODE_IMM( 0x49,		"EOR #%d",			*,	*,	{}		)
OPCODE_NON( 0x4a,		"LSR",				*,	*,	{}		)
OPCODE_ABS( 0x4c,		"JMP $%04x",		*,	*,	{}		)
OPCODE_ABS( 0x4d,		"EOR $%04x",		*,	*,	{}		)
OPCODE_ABS( 0x4e,		"LSR $%04x",		*,	*,	{}		)

OPCODE_REL( 0x50,		"BVC %d",			*,	*,	{}		)
OPCODE_INY( 0x51,		"EOR ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0x55,		"EOR $%02x,X",		*,	*,	{}		)
OPCODE_ZRX( 0x56,		"LSR $%02x,X",		*,	*,	{}		)
OPCODE_NON( 0x58,		"CLI",				*,	*,	{}		)
OPCODE_ABY( 0x59,		"EOR $%04x,Y",		*,	*,	{}		)
OPCODE_ABX( 0x5d,		"EOR $%04x,X",		*,	*,	{}		)
OPCODE_ABX( 0x5e,		"LSR $%04x,X",		*,	*,	{}		)

OPCODE_NON( 0x60,		"RTS",				*,	*,	{}		)
OPCODE_INX( 0x61,		"ADC ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO( 0x65,		"ADC $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0x66,		"ROR $%02x",		*,	*,	{}		)
OPCODE_NON( 0x68,		"PLA",				*,	*,	{}		)
OPCODE_IMM( 0x69,		"ADC #%d",			*,	*,	{}		)
OPCODE_NON( 0x6a,		"ROR A",			*,	*,	{}		)
OPCODE_IND( 0x6c,		"JMP ($%04x)",		*,	*,	{}		)
OPCODE_ABS( 0x6d,		"ADC $%04x",		*,	*,	{}		)
OPCODE_ABX( 0x6e,		"ROR $%04x,X",		*,	*,	{}		)

OPCODE_REL( 0x70,		"BVS %d",			*,	*,	{}		)
OPCODE_INY( 0x71,		"ADC ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0x75,		"ADC $%02x,X",		*,	*,	{}		)
OPCODE_ZRX( 0x76,		"ROR $%02x,X",		*,	*,	{}		)
OPCODE_NON( 0x78,		"SEI",				*,	*,	{}		)
OPCODE_ABY( 0x79,		"ADC $%04x,Y",		*,	*,	{}		)
OPCODE_ABX( 0x7d,		"ADC $%04x,X",		*,	*,	{}		)
OPCODE_ABS( 0x7e,		"ROR $%04x",		*,	*,	{}		)

OPCODE_INX( 0x81,		"STA ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO( 0x84,		"STY $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0x85,		"STA $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0x86,		"STX $%02x",		*,	*,	{}		)
OPCODE_NON( 0x88,		"DEY",				*,	*,	{}		)
OPCODE_NON( 0x8a,		"TXA",				*,	*,	{}		)
OPCODE_ABS( 0x8c,		"STY $%04x",		*,	*,	{}		)
OPCODE_ABS( 0x8d,		"STA $%04x",		*,	*,	{}		)
OPCODE_ABS( 0x8e,		"STX $%04x",		*,	*,	{}		)

OPCODE_REL( 0x90,		"BCC %d",			*,	*,	{}		)
OPCODE_INY( 0x91,		"STA ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0x94,		"STY $%02x,X",		*,	*,	{}		)
OPCODE_ZRX( 0x95,		"STA $%02x,X",		*,	*,	{}		)
OPCODE_ZRY( 0x96,		"STX $%02x,Y",		*,	*,	{}		)
OPCODE_NON( 0x98,		"TYA",				*,	*,	{}		)
OPCODE_ABY( 0x99,		"STA $%04x,Y",		*,	*,	{}		)
OPCODE_NON( 0x9a,		"TXS",				*,	*,	{}		)
OPCODE_ABX( 0x9d,		"STA $%04x,X",		*,	*,	{}		)

OPCODE_IMM( 0xa0,		"LDY #%d",			*,	*,	{}		)
OPCODE_INX( 0xa1,		"LDA ($%02x,X)",	*,	*,	{}		)
OPCODE_IMM( 0xa2,		"LDX #%d",			*,	*,	{}		)
OPCODE_ZRO( 0xa4,		"LDY $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0xa5,		"LDA $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0xa6,		"LDX $%02x",		*,	*,	{}		)
OPCODE_NON( 0xa8,		"TAY",				*,	*,	{}		)
OPCODE_IMM( 0xa9,		"LDA #%d",			*,	*,	{}		)
OPCODE_NON( 0xaa,		"TAX",				*,	*,	{}		)
OPCODE_ABS( 0xac,		"LDY $%04x",		*,	*,	{}		)
OPCODE_ABS( 0xad,		"LDA $%04x",		*,	*,	{}		)
OPCODE_ABS( 0xae,		"LDX $%04x",		*,	*,	{}		)

OPCODE_REL( 0xb0,		"BCS %d",			*,	*,	{}		)
OPCODE_INY( 0xb1,		"LDA ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0xb4,		"LDY $%02x, X",		*,	*,	{}		)
OPCODE_ZRX( 0xb5,		"LDA $%02x, X",		*,	*,	{}		)
OPCODE_ZRY( 0xb6,		"LDX $%02x, Y",		*,	*,	{}		)
OPCODE_NON( 0xb8,		"CLV",				*,	*,	{}		)
OPCODE_ABY( 0xb9,		"LDA $%04x,Y",		*,	*,	{}		)
OPCODE_NON( 0xba,		"TSX",				*,	*,	{}		)
OPCODE_ABX( 0xbc,		"LDY $%04x,X",		*,	*,	{}		)
OPCODE_ABX( 0xbd,		"LDA $%04x,X",		*,	*,	{}		)
OPCODE_ABY( 0xbe,		"LDX $%04x,Y",		*,	*,	{}		)

OPCODE_IMM( 0xc0,		"CPY #%d",			*,	*,	{}		)
OPCODE_INX( 0xc1,		"CMP ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO( 0xc4,		"CPY $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0xc5,		"CMP $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0xc6,		"DEC $%02x",		*,	*,	{}		)
OPCODE_NON( 0xc8,		"INY",				*,	*,	{}		)
OPCODE_IMM( 0xc9,		"CMP #%d",			*,	*,	{}		)
OPCODE_NON( 0xca,		"DEX",				*,	*,	{}		)
OPCODE_ABS( 0xcc,		"CPY $%04x",		*,	*,	{}		)
OPCODE_ABS( 0xcd,		"CMP $%04x",		*,	*,	{}		)
OPCODE_ABS( 0xce,		"DEC $%04x",		*,	*,	{}		)

OPCODE_REL( 0xd0,		"BNE %d",			*,	*,	{}		)
OPCODE_INY( 0xd1,		"CMP ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0xd5,		"CMP $%02x, X",		*,	*,	{}		)
OPCODE_ZRX( 0xd6,		"DEC $%02x, X",		*,	*,	{}		)
OPCODE_NON( 0xd8,		"CLD",				*,	*,	{}		)
OPCODE_ABY( 0xd9,		"CMP $%04x,Y",		*,	*,	{}		)
OPCODE_ABX( 0xdd,		"CMP $%04x,X",		*,	*,	{}		)
OPCODE_ABX( 0xde,		"DEC $%04x,X",		*,	*,	{}		)

OPCODE_IMM( 0xe0,		"CPX #%d",			*,	*,	{}		)
OPCODE_INX( 0xe1,		"SBC ($%02x,X)",	*,	*,	{}		)
OPCODE_ZRO( 0xe4,		"CPX $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0xe5,		"SBC $%02x",		*,	*,	{}		)
OPCODE_ZRO( 0xe6,		"INC $%02x",		*,	*,	{}		)
OPCODE_NON( 0xe8,		"INX",				*,	*,	{}		)
OPCODE_IMM( 0xe9,		"SBC #%d",			*,	*,	{}		)
OPCODE_NON( 0xea,		"NOP",				*,	*,	{}		)
OPCODE_ABS( 0xec,		"CPX $%04x",		*,	*,	{}		)
OPCODE_ABS( 0xed,		"SBC $%04x",		*,	*,	{}		)
OPCODE_ABS( 0xee,		"INC $%04x",		*,	*,	{}		)

OPCODE_REL( 0xf0,		"BEQ %d",			*,	*,	{}		)
OPCODE_INY( 0xf1,		"SBC ($%02x),Y",	*,	*,	{}		)
OPCODE_ZRX( 0xf5,		"SBC $%02x, X",		*,	*,	{}		)
OPCODE_ZRX( 0xf6,		"INC $%02x, X",		*,	*,	{}		)
OPCODE_NON( 0xf8,		"SED",				*,	*,	{}		)
OPCODE_ABY( 0xf9,		"SBC $%04x,Y",		*,	*,	{}		)
OPCODE_ABX( 0xfd,		"SBC $%04x,X",		*,	*,	{}		)
OPCODE_ABX( 0xfe,		"INC $%04x,Y",		*,	*,	{}		)
