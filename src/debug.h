#pragma once

#if DEBUG

#include "scope_timer\scope_timer.h"

#if TARGET_WINSIM
#include <Windows.h>

// debug mem writes to this address (Slow. in WinSim will spit out the last 50 instructions, their address, the contents of the registers and the 8 bytes before and after the given address)
#define DEBUG_MEMWRITE 0
#define DEBUG_BREAKPOINT 0

void failedAssert(const char* assertion);
#define DebugAssert(x) { if (!(x)) failedAssert(#x); }

#if DEBUG_MEMWRITE
extern unsigned short debugWriteAddress;
extern void HitMemAccess();
#define DEBUG_TRACKINSTRUCTIONS 32
#define DebugWrite(address) { if (debugWriteAddress && address == debugWriteAddress) { HitMemAccess(); } }
#endif

#if DEBUG_BREAKPOINT
extern unsigned short debugBreakpoint;
extern void HitBreakpoint();
#ifndef DEBUG_TRACKINSTRUCTIONS
#define DEBUG_TRACKINSTRUCTIONS 32
#endif
#define DebugPC(address) { if (debugBreakpoint && address == debugBreakpoint) { HitBreakpoint(); } }
#endif

#if DEBUG_TRACKINSTRUCTIONS
struct InstructionsHistory {
	char instr[64];
	registers_type regs;
};
extern int instr_slot;
extern InstructionsHistory instr_hist[DEBUG_TRACKINSTRUCTIONS];
#define DebugInstruction(...) { sprintf(instr_hist[instr_slot % DEBUG_TRACKINSTRUCTIONS].instr, __VA_ARGS__); instr_hist[(instr_slot++) % DEBUG_TRACKINSTRUCTIONS].regs = cpu.registers; }
#define DebugInstructionMapped(...) { sprintf(instr_hist[instr_slot % DEBUG_TRACKINSTRUCTIONS].instr, __VA_ARGS__); instr_hist[(instr_slot++) % DEBUG_TRACKINSTRUCTIONS].regs = cpu.registers; }
#endif

#define OutputLog(...) { char buffer[1024]; sprintf_s(buffer, 1024, __VA_ARGS__); OutputDebugString(buffer); }

#else

///////////////////////////////////////////////////////////////////////////////////////////////////
// For "cowboy debugging" Prizm crashes

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)
#define STEP_LINE { int _x = 0; int _y = 0; PrintMini(&_x, &_y, "Step: " S__LINE__, 0, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0); GetKey(&_x); }
#define STEP_LINE_INFO(...) { int _x = 0; int _y = 0; PrintMini(&_x, &_y, "Step: " S__LINE__, 0, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, 1, 0); printf(__VA_ARGS__);  GetKey(&_x); }

// Output log only available on WinSim for the moment (add write to file option?)
#define OutputLog(...)

#endif

#endif

#ifndef DebugInstruction
#define DebugInstruction(...) 
#define DebugInstructionMapped(...) 
#endif

#ifndef DebugWrite
#define DebugWrite(...) 
#endif

#ifndef DebugPC
#define DebugPC(...) 
#endif

#ifndef DebugAssert
#define DebugAssert(...)
#endif

#ifndef OutputLog
#define OutputLog(...)
#endif