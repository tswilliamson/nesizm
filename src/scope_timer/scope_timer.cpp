
// ScopeTimer not supported on windows simulator or non-Debug
#if DEBUG

#include "../platform.h"
#include "scope_timer.h"

#include "../../calctype/calctype.h"
#include "../../calctype/fonts/arial_small/arial_small.h"	

#if TARGET_WINSIM
unsigned int ScopeTimer_Frequency = 0;
LONGLONG ScopeTimer_Start = 0;
#endif

ScopeTimer* ScopeTimer::firstTimer = NULL;
char ScopeTimer::debugString[128] = { 0 };

ScopeTimer::ScopeTimer(const char* withFunctionName, int withLine) : cycleCount(0), numCounts(0), funcName(withFunctionName), line(withLine) {
	// put me in the linked list
	nextTimer = firstTimer;
	firstTimer = this;
}

void ScopeTimer::InitSystem() {
	// initialize TMU2 at the fastest clock rate possible

#if TARGET_PRIZM
	// disable so we can set up
	REG_TMU_TSTR &= ~(1 << 2);

	REG_TMU_TCOR_2 = 0x7FFFFFFF; // max int
	REG_TMU_TCNT_2 = 0x7FFFFFFF;
	REG_TMU_TCR_2 = 0x0000; // no interrupt needed

	// enable TMU2
	REG_TMU_TSTR |= (1 << 2);
#endif

#if TARGET_WINSIM
	// 
	LARGE_INTEGER result;
	QueryPerformanceFrequency(&result);
	ScopeTimer_Frequency = (result.QuadPart >> 23);
	if (ScopeTimer_Frequency == 0) {
		ScopeTimer_Frequency = 1;
	}
	QueryPerformanceCounter(&result);
	ScopeTimer_Start = result.QuadPart;
#endif

	memset(debugString, 0, sizeof(debugString));

	// clear timers
	ScopeTimer* curTimer = firstTimer;
	while (curTimer) {
		curTimer->cycleCount = 0;
		curTimer->numCounts = 0;
		curTimer = curTimer->nextTimer;
	}
}

void ScopeTimer::Shutdown() {
#if TARGET_PRIZM
	// disable TMU 2
	REG_TMU_TSTR &= ~(1 << 2);
#endif
}

static void PrintInfo(int row, const char* label, const char* info, unsigned short color) {
	// location of columns
	const int col1 = 0;
	const int col2 = 200;

	int x = col1;
	int y = row * 15 + 2;
	CalcType_Draw(&arial_small, label, x, y, color, 0, 0);

	x = col2;
	y = row * 15 + 2;
	CalcType_Draw(&arial_small, info, x, y, color, 0, 0);
}

void ScopeTimer::DisplayTimes() {
	// display debug view with get key
	int toKey;
	unsigned int maxCycles = 0;
	unsigned int maxCount = 0;

	// mode descriptions
	const int numModes = 3;
	const char* modeNames[numModes] = {
		"% Max",
		"Num Cycles",
		"Num Hits"
	};

	// reset display area just in case
	Bdisp_PutDisp_DD();

	// allocate number of timers in use, support up to 128
	int numTimers = 0;
	ScopeTimer* timers[128];
	{
		ScopeTimer* curTimer = firstTimer;
		while (curTimer && numTimers < 128) {
			timers[numTimers] = curTimer;
			if (curTimer->cycleCount > maxCycles) {
				maxCycles = curTimer->cycleCount;
			}
			if (curTimer->numCounts > maxCount) {
				maxCount = curTimer->numCounts;
			}

			numTimers++;
			curTimer = curTimer->nextTimer;
		}
	}

	int startTimer = 0;
	int mode = 0;

	do {
		// create stat display
		Bdisp_AllClr_VRAM();

		// header
		PrintInfo(0, "Function(line)", modeNames[mode], TEXT_COLOR_BLACK);

		// notify if there are no timers
		if (numTimers == 0) {
			PrintInfo(1, "No timers found!", "ERROR", TEXT_COLOR_RED);
		}

		for (int timer = startTimer, row = 1; timer < numTimers && row < 12; timer++, row++) {
			ScopeTimer* curTimer = timers[timer];

			char name[256];
			memset(name, 0, sizeof(name));
			sprintf(name, "%s(%d)", curTimer->funcName, curTimer->line);

			bool isMax;
			char info[256];
			memset(info, 0, sizeof(info));
			switch (mode) {
				case 0:
				{
					int percent = curTimer->cycleCount / (maxCycles / 1000);
					sprintf(info, "%d.%d", percent / 10, percent % 10);
					isMax = curTimer->cycleCount == maxCycles;
					break;
				}
				case 1:
					sprintf(info, "%dk", curTimer->cycleCount / 1024);
					isMax = curTimer->cycleCount == maxCycles;
					break;
				case 2:
					sprintf(info, "%dk", curTimer->numCounts / 1024);
					isMax = curTimer->numCounts == maxCount;
					break;
			}
			PrintInfo(row, name, info, isMax ? TEXT_COLOR_RED : TEXT_COLOR_BLUE);
		}

		if (debugString[0]) {
			PrintInfo(12, debugString, "Nav: Arrows, Leave: EXIT", TEXT_COLOR_BLACK);
		} else {
			PrintInfo(12, "Nav: Arrows", "Leave: EXIT", TEXT_COLOR_BLACK);
		}

		GetKey(&toKey);
		
		switch (toKey) {
			case KEY_CTRL_UP:
				if (startTimer > 0) {
					startTimer--;
				}
				break;
			case KEY_CTRL_DOWN:
				if (startTimer < numTimers - 1) {
					startTimer++;
				}
				break;
			case KEY_CTRL_RIGHT:
				mode = (mode + 1) % numModes;
				break;
			case KEY_CTRL_LEFT:
				mode = (mode + numModes - 1) % numModes;
				break;
		}
	// wait for exit key
	} while (toKey != KEY_CTRL_EXIT);
}

#endif