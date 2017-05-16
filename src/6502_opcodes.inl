
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

// http://www.emulator101.com/reference/6502-reference.html for opcode order
// 
// OPCODE_   ( 0x11,		"",					*,	*,	{}		)