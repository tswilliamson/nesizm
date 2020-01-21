#pragma once

#include "platform.h"
#include "nes.h"

struct EmulatorSettings {
	int32 keyMap[NES_MAX + NES_NUM_CONTROLLER_KEYS];

	uint8 autoSave;
	uint8 overClock;
	uint8 frameSkip;
	uint8 speed;
	uint8 twoPlayer;
	uint8 turboSetting;
	uint8 palette;
	uint8 chopSides;
	uint8 background;
	uint8 soundEnabled;
	uint8 soundRate;

	void SetDefaults();
	void Load();
	void Save();
};

extern EmulatorSettings nes_settings;