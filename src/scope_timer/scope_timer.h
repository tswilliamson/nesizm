#pragma once

#if DEBUG

#if TARGET_PRIZM
#include "tmu.h"
#define GetCycles() REG_TMU_TCNT_2
#else
#include <windows.h>
extern LONGLONG ScopeTimer_Start;
inline unsigned int GetCycles() {
	LARGE_INTEGER result;
	if (QueryPerformanceCounter(&result) != 0) {
		return (unsigned int) (ScopeTimer_Start - result.QuadPart);
	}

	return 0;
}
#endif

struct ScopeTimer {
	unsigned int cycleCount;
	unsigned int numCounts;

	const char* funcName;
	int line;
	static int fpsValue;

	ScopeTimer* nextTimer;

	ScopeTimer(const char* withFunctionName, int withLine);
	void Register(const char* withFunctionName);

	inline void AddTime(unsigned short cycles) {
		cycleCount += cycles;
		numCounts++;
	}

	static ScopeTimer* firstTimer;
	static char debugString[128];			// per application debug string (placed on last row), usually FPS or similar
	static void InitSystem();
	static void ReportFrame();
	static void DisplayTimes();
	static void Shutdown();
};

struct TimedInstance {
	unsigned int start;
	ScopeTimer* myTimer;

	inline TimedInstance(ScopeTimer* withTimer) : start(GetCycles()), myTimer(withTimer) {
	}

	inline ~TimedInstance() {
		int elapsed = (int)(start - GetCycles());

		if (elapsed >= 0) {
			myTimer->AddTime(elapsed);
		}
	}
};

#define TIME_SCOPE() static ScopeTimer __timer(__FUNCTION__, __LINE__); TimedInstance __timeMe(&__timer);
#define TIME_SCOPE_NAMED(Name) static ScopeTimer __timer(#Name, __LINE__); TimedInstance __timeMe(&__timer);
#else
struct ScopeTimer {
	static void InitSystem() {}
	static void DisplayTimes() {}
	static void Shutdown() {}
	static void ReportFrame() {}
};
#endif

#ifndef TIME_SCOPE
#define TIME_SCOPE() 
#define TIME_SCOPE_NAMED(Name) 
#endif