#include "platform.h"
#include "debug.h"

#include "settings.h"

EmulatorSettings nesSettings;

struct SettingInfo {
	SettingType type;
	SettingGroup group;
	bool available;
	int defaultValue;
	int numValues;
	const char* name;
	const char** values;
};

/*
	System
		Autosave - Whether the game auto saves the state of the game when exiting with menu key
			[On, Off]
		Overclock - Whether to enable calculator overclocking for faster emulation (will drain battery faster!)
			[On, Off]
		Frame Skip - How many frames are skipped for each rendered frame
			[Auto, 0, 1, 2, 3, 4]
		Speed - What target speed to emulate at. Can be useful to make some games easier to play
			[Unclamped, 100%, 90%, 80%, 50%]
		Back - Return to Options
	Controls
		Remap Keys - Map the controller buttons to new keys
		Two Players - Whether to enable two players (map keys again if enabled)
			[On, Off]
		Turbo Speed - What frequency turbo buttons should toggle (games can struggle with very fast speed)
			[Disabled, 5 hz, 10 hz, 15 hz, 20 hz]
		Back - Return to Options
	Video
		Palette - Select from a prebuilt color palette, or a
			[FCEUX, Blargg, Custom]
		Background - Select a background image when playing (see readme for custom background support)
			[Game BG Color, Warp Field, Old TV, Black, Custom]
		Back - Return to Options
	Sound
		Enable - Whether sounds are enabled or not
			[On, Off]
		Sample Rate - Select sample rate quality (High = slower speed)
			[Low, High]
		Back - Return to Options
*/

static const char* OffOn[] = {
	"Off",
	"On"
};

static const char* FrameSkipOptions[] = {
	"Auto",
	"None",
	"1",
	"2",
	"3",
	"4"
};

static const char* SpeedOptions[] = {
	"125%",
	"100%",
	"90%",
	"75%",
	"Unclamped"
};

static const char* TurboKeyOptions[] = {
	"30 Hz",
	"15 Hz",
	"8 Hz",
	"4 Hz",
	"2 Hz"
};

static const char* PaletteOptions[] = {
	"FCEUX",
	"Blargg",
	"Custom"
};

static const char* BackgroundOptions[] = {
	"Warp",
	"TV",
	"Game Color",
	"Black",
	"Custom",
};

static const char* SoundQualityOptions[] = {
	"Low",
	"High"
};

static const char* OverclockOptions[] = {
	"Off",
	"150%",
	"200%",
};

static SettingInfo infos[] = {
	{ ST_AutoSave,			SG_System,		false,	0,	2,	"Auto Save",		OffOn},
	{ ST_OverClock,			SG_System,		true,   0,  3,  "Overclock",		OverclockOptions},
	{ ST_FrameSkip,			SG_System,		true,	0,  6,	"Frame Skip",		FrameSkipOptions},
	{ ST_Speed,				SG_System,		true,	1,	5,	"Speed",			SpeedOptions},
	{ ST_TwoPlayer,			SG_Deprecated,	false,	0,	2,	"2 Player Mode",	OffOn},
	{ ST_TurboSetting,		SG_Controls,	true,	2,	5,	"Turbo Key",		TurboKeyOptions},
	{ ST_Palette,			SG_Video,		false,	0,	3,	"Palette",			PaletteOptions},
	{ ST_Background,		SG_Video,		false,	0,	5,	"Background",		BackgroundOptions},
	{ ST_SoundEnabled,		SG_Sound,		true,	0,	2,	"Enable Sound",		OffOn},
	{ ST_SoundRate,			SG_Sound,		false,	0,  2,  "Quality",			SoundQualityOptions},
	{ ST_DimScreen,			SG_Video,		true,	0,  2,  "Dim Screen",		OffOn }
};

const char* EmulatorSettings::GetSettingName(SettingType setting) {
	return infos[setting].name;
}

const char* EmulatorSettings::GetSettingValueName(SettingType setting, uint8 value) {
	return infos[setting].values[value];
}

int EmulatorSettings::GetNumValues(SettingType setting) {
	return infos[setting].numValues;
}

SettingGroup EmulatorSettings::GetSettingGroup(SettingType setting) {
	return infos[setting].group;
}

bool EmulatorSettings::GetSettingAvailable(SettingType setting) {
	return infos[setting].available;
}

void EmulatorSettings::SetDefaults() {
	memset(this, 0, sizeof(EmulatorSettings));

	// by default P2 is unmapped
	keyMap[NES_P1_A] = 78;			// SHIFT
	keyMap[NES_P1_B] = 68;			// OPTN
	keyMap[NES_P1_TURBO_A] = 77;	// Alpha
	keyMap[NES_P1_TURBO_B] = 67;	// X^2
	keyMap[NES_P1_SELECT] = 39;		// F5
	keyMap[NES_P1_START] = 29;		// F6
	keyMap[NES_P1_RIGHT] = 27;
	keyMap[NES_P1_LEFT] = 38;
	keyMap[NES_P1_UP] = 28;
	keyMap[NES_P1_DOWN] = 37;
	keyMap[NES_SAVESTATE] = 43;		// 'S'
	keyMap[NES_LOADSTATE] = 25;		// 'L'
	keyMap[NES_FASTFORWARD] = 57;	// '^'
	keyMap[NES_VOL_UP] = 42;	// '+'
	keyMap[NES_VOL_DOWN] = 32;	// '-'

	// simulator only defaults
#if TARGET_WINSIM
	keyMap[NES_SAVESTATE] = 59;		// maps to F3
	keyMap[NES_LOADSTATE] = 49;		// maps to F4
#endif

	for (int i = 0; i < (int)MAX_SETTINGS; i++) {
		DebugAssert(infos[i].type == i);
		values[i] = infos[i].defaultValue;
	}
}

void EmulatorSettings::IncSetting(SettingType type) {
	values[type]++;
	if (values[type] >= infos[type].numValues) {
		values[type] = 0;
	}
}

static const char* settingsDir = "Nesizm";
static const char* settingsFile = "Settings";

void EmulatorSettings::Load() {
	SetDefaults();

	int len = 0;
	if (!MCSGetDlen2((unsigned char*)settingsDir, (unsigned char*)settingsFile, &len) && len > 0 && len < 256) {
		uint8 contents[256];

		if (!MCSGetData1(0, len, &contents[0])) {
			int cur = 0;
			uint8 numItems = contents[cur++];
			for (int i = 0; i < numItems; i++) {
				uint8 setting = contents[cur++];
				uint8 value = contents[cur++];
				values[setting] = value;
			}
			uint8 numKeys = contents[cur++];
			if (numKeys == NES_MAX_KEYS) {
				for (int i = 0; i < NES_MAX_KEYS; i++) {
					keyMap[i] = contents[cur++];
				}
			}
		}
	}

	if (getDeviceType() == DT_CG50) {
		infos[ST_OverClock].available = false;
	} else {
		infos[ST_OverClock].available = true;
	}
}

void EmulatorSettings::Save() {
	uint8 contents[256];
	int32 size = 1;
	contents[0] = 0;
	for (int32 i = 0; i < (int)MAX_SETTINGS; i++) {
		if (values[i] != infos[i].defaultValue) {
			contents[0]++;
			contents[size++] = i;
			contents[size++] = values[i];
		}
	}
	contents[size++] = NES_MAX_KEYS;
	for (int i = 0; i < NES_MAX_KEYS; i++) {
		contents[size++] = keyMap[i];
	}

	while (size % 4 != 0) size++;

	MCS_CreateDirectory((unsigned char*)settingsDir);
	MCS_WriteItem((unsigned char*)settingsDir, (unsigned char*)settingsFile, 0, size, (int)(&contents[0]));
}