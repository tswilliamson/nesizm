// uses scope timer to time all individual instructions for performance profiling
#define INSTRUCTION_TIMING 0

#if INSTRUCTION_TIMING
#define OPCODE_TIMER(Opcode, Prefix,Name) \
	static ScopeTimer _time_Instr_##Opcode(#Name"_"#Prefix"_"#Opcode, 0); 

#define OPCODE(Prefix,Opcode,String,Clk,Sz,Page,Name,Code) OPCODE_TIMER(Opcode, Prefix, Name)
#include "6502_opcodes.inl"

void RegisterInstructionTimers() {
#define OPCODE(Prefix,Opcode,String,Clk,Sz,Page,Name,Code) _time_Instr_##Opcode.Register(#Name"_"#Prefix"_"#Opcode);
#include "6502_opcodes.inl"
}

#define INSTR_TIMING(Opcode) TimedInstance __timeMe(&_time_Instr_##Opcode);
#else
#define INSTR_TIMING(op)
#define RegisterInstructionTimers()
#endif

