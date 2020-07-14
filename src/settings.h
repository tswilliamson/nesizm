#pragma once

#include "platform.h"
#include "nes.h"

enum SettingType {
	ST_AutoSave,
	ST_OverClock,
	ST_FrameSkip,
	ST_Speed,
	ST_TwoPlayer,
	ST_TurboSetting,
	ST_Palette,
	ST_Background,
	ST_SoundEnabled,
	ST_SoundQuality,
	ST_DimScreen,
	ST_StretchScreen,
	ST_ShowClock,

	MAX_SETTINGS
};

enum SettingGroup {
	SG_None,
	SG_System,
	SG_Controls,
	SG_Video,
	SG_Sound,
	SG_Deprecated
};

struct EmulatorSettings {
	int32 keyMap[NES_MAX_KEYS];

	void SetDefaults();
	void Load();
	void Save();

	int GetSetting(SettingType type) const {
		return values[type];
	}

	void IncSetting(SettingType type);

	const char* GetContinueFile();
	void SetContinueFile(const char* romFile);

	static const char* GetSettingName(SettingType setting);
	static const char* GetSettingValueName(SettingType setting, uint8 value);
	static int GetNumValues(SettingType setting);
	static SettingGroup GetSettingGroup(SettingType setting);
	static bool GetSettingAvailable(SettingType setting);

	static bool CheckCachedKey(NesKeys key);
private:
	uint8 values[MAX_SETTINGS];
	char continueFile[48];
};

extern EmulatorSettings nesSettings;