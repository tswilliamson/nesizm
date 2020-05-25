
#include "platform.h"
#include "debug.h"
#include "nes.h"
#include "ptune2_simple/Ptune2_direct.h"

#include "frontend.h"
#include "imageDraw.h"
#include "settings.h"

/*
Menu Layout
	Continue - Loads the most recently played game and its corresponding save state / Continue playing game
	If available: View FAQ - View the game.txt file loaded in parallel with this ROM
	Load ROM - Load a new ROM file
	Options - Change various emulator options
		See Settings.cpp
	About - Contact me on Twitter at @TSWilliamson or stalk me on the cemetech forums under the same username
*/

#include "calctype/fonts/commodore/commodore.c"		// For Menus
#include "calctype/fonts/arial_small/arial_small.h"

const bool bRebuildGfx = false;

bool shouldExit = false;

struct foundFile {
	char path[48];
};

typedef struct {
	unsigned short id, type;
	unsigned long fsize, dsize;
	unsigned int property;
	unsigned long address;
} file_type_t;

nes_frontend nesFrontend;

bool isSelectKey(int key) {
	return (key == KEY_CTRL_EXE) || (key == KEY_CTRL_SHIFT);
}

static int32 findKey() {
	for (int32 i = 27; i <= 79; i++) {
		if (keyDown_fast(i)) {
			return i;
		}
	}
	return 0;
}

static int32 waitKey() {
	int32 ret = 0;
	// wait for press
	while (ret == 0) {
		ret = findKey();
	}
	// wait for unpress
	while (findKey() != 0) {}

	return ret;
}

extern MenuOption mainOptions[];
extern MenuOption optionTree[];
extern MenuOption remapOptions[];

static bool Continue_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesFrontend.gotoGame = true;
		nesCart.OnContinue();
		return true;
	}

	return false;
}

static void FindFiles(const char* path, foundFile* toArray, int& numFound, int maxAllowed) {
	unsigned short filter[0x100], found[0x100];
	int ret, handle;
	file_type_t info; // See Bfile_FindFirst for the definition of this struct

	Bfile_StrToName_ncpy(filter, path, 0x50); // Overkill

	ret = Bfile_FindFirst((const char*)filter, &handle, (char*)found, &info);

	while (ret == 0 && numFound < maxAllowed) {
		Bfile_NameToStr_ncpy(toArray[numFound++].path, found, 48);
		ret = Bfile_FindNext(handle, (char*)found, (char*)&info);
	};

	Bfile_FindClose(handle);
}

foundFile* discoverFiles(int& numFound) {
	static foundFile files[32];
	static int lastFound = 0;

	// only search once 
	if (!lastFound) {
		numFound = 0;
		FindFiles("\\\\fls0\\*.nes", files, numFound, 32);
		lastFound = numFound;
	}

	numFound = lastFound;
	return files;
}

static bool ROMFile_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		// unload existing game file if there is one:
		if (nesCart.romFile) {
			nesCart.romFile[0] = 0;
		}

		// set up rom file, tell nes to go to game
		cpu6502_Init();
		nesPPU.init();

		char romFile[128];
		if (forOption->extraData) {
			// use continue file
			sprintf(romFile, "\\\\fls0\\%s", nesSettings.GetContinueFile());
		} else {
			sprintf(romFile, "\\\\fls0\\%s", forOption->name);
		}
		if (nesCart.loadROM(romFile)) {
			if (forOption->extraData == 0 && (!nesSettings.GetContinueFile() || strcmp(forOption->name, nesSettings.GetContinueFile()))) {
				nesSettings.SetContinueFile(forOption->name);
				nesSettings.Save();
			}

			mainCPU.reset();

			nesFrontend.gotoGame = true;
		} else {
			// printf has errors
			int key = 0;
			GetKey(&key);
		}

		if (forOption->extraData == 0) {
			free((void*)nesFrontend.currentOptions);
		}

		nesFrontend.SetMainMenu();

		return true;
	}

	return false;
}

static bool FileBack_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		free((void*)nesFrontend.currentOptions);
		nesFrontend.SetMainMenu();
		return true;
	}

	return false;
}

static bool LoadROM_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		int numFiles;
		foundFile* files = discoverFiles(numFiles);
		
		if (numFiles) {
			// move menu to file select
			MenuOption* fileOptions = (MenuOption*) malloc(sizeof(MenuOption) * (numFiles + 1));
			memset(fileOptions, 0, sizeof(MenuOption) * (numFiles + 1));
			for (int i = 0; i < numFiles; i++) {
				fileOptions[i].name = files[i].path;
				fileOptions[i].OnKey = ROMFile_Selected;
				fileOptions[i].extraData = 0;
			}
			fileOptions[numFiles].name = "Back";
			fileOptions[numFiles].OnKey = FileBack_Selected;

			nesFrontend.currentOptions = fileOptions;
			nesFrontend.numOptions = numFiles + 1;
			nesFrontend.selectedOption = 0;
			nesFrontend.selectOffset = 0;
		}

		return true;
	}

	return false;
}

static bool ViewFAQ_Selected(MenuOption* forOption, int key) {
	return false;
}

static bool Options_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesFrontend.currentOptions = optionTree;
		nesFrontend.numOptions = 5;
		nesFrontend.selectedOption = 0;
		nesFrontend.selectOffset = 0;
		return true;
	}

	return false;
}

static bool About_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		PrizmImage::Draw_GradientRect(70, 73, 244, 69, 0b0000000011100000, COLOR_BLACK);
		PrizmImage::Draw_BorderRect(71, 74, 242, 67, 2, COLOR_AQUAMARINE);
		const char* text1 = "By @TSWilliamson";
		const char* text2 = "Contact via my Github account";
		const char* text3 = "Source at github.com/TSWilliamson/nesizm";
		int32 w1 = CalcType_Width(&arial_small, text1);
		int32 w2 = CalcType_Width(&arial_small, text2);
		int32 w3 = CalcType_Width(&arial_small, text3);
		CalcType_Draw(&arial_small, text1, 192 - w1 / 2, 83, COLOR_WHITE, 0, 0);
		CalcType_Draw(&arial_small, text2, 192 - w2 / 2, 83 + arial_small.height + 6, COLOR_WHITE, 0, 0);
		CalcType_Draw(&arial_small, text3, 192 - w3 / 2, 83 + arial_small.height * 2 + 12, COLOR_WHITE, 0, 0);
		Bdisp_PutDisp_DD_stripe(73, 73 + 70);

		waitKey();
		return true;
	}

	return false;
}

static bool Option_RemapButtons(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesFrontend.currentOptions = remapOptions;
		nesFrontend.numOptions = NES_MAX_KEYS + 1;
		nesFrontend.selectedOption = 0;
		nesFrontend.selectOffset = 0;
		return true;
	}

	return false;
}

static bool Option_RemapKey(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		PrizmImage::Draw_GradientRect(70, 83, 244, 49, 0b0000000011100000, COLOR_BLACK);
		PrizmImage::Draw_BorderRect(71, 84, 242, 47, 2, COLOR_AQUAMARINE);
		const char* text1 = "Press Any Key";
		const char* text2 = "MENU to Cancel";
		int32 w1 = CalcType_Width(&commodore, text1);
		int32 w2 = CalcType_Width(&commodore, text2);
		CalcType_Draw(&commodore, text1, 192 - w1 / 2, 91, COLOR_WHITE, 0, 0);
		CalcType_Draw(&commodore, text2, 192 - w2 / 2, 91 + commodore.height + 6, COLOR_DARKMAGENTA, 0, 0);
		Bdisp_PutDisp_DD_stripe(83, 83 + 49);

		int selectedKey = waitKey();
		if (selectedKey == 48) { // MENU
			selectedKey = 0;
		}

		nesSettings.keyMap[forOption->extraData] = selectedKey;
		return true;
	}

	return false;
}

static const char* Option_GetKeyDetails(MenuOption* forOption) {
	int32 key = nesSettings.keyMap[forOption->extraData];
	if (key == 0) {
		return "None";
	}
	static char bufferCode[3];
	bufferCode[0] = key / 10 + '0';
	bufferCode[1] = key % 10 + '0';
	bufferCode[2] = 0;
	return bufferCode;
}

static bool OptionsBack(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		free((void*)nesFrontend.currentOptions);
		Options_Selected(nullptr, key);
		return true;
	}

	return false;
}

static bool Option_Any(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesSettings.IncSetting((SettingType)forOption->extraData);
		return true;
	}

	return false;
}

static const char* Option_Detail(MenuOption* forOption) {
	SettingType type = (SettingType) forOption->extraData;
	return EmulatorSettings::GetSettingValueName(type, nesSettings.GetSetting(type));
}

static bool OptionMenu(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		SettingGroup group = (SettingGroup)forOption->extraData;
		int32 countOptions = 0;
		for (int32 i = 0; i < MAX_SETTINGS; i++) {
			if (EmulatorSettings::GetSettingGroup((SettingType)i) == group) {
				countOptions++;
			}
		}

		MenuOption* options = (MenuOption*)malloc(sizeof(MenuOption) * (countOptions + 2));
		memset(options, 0, sizeof(MenuOption) * (countOptions + 2));

		int32 curOption = 0;
		if (group == SG_Controls) {
			options[curOption].name = "Remap Buttons";
			options[curOption].OnKey = Option_RemapButtons;
			options[curOption].help = "Map which keys are used for each NES button";
			curOption++;
		}
		for (int32 i = 0; i < MAX_SETTINGS; i++) {
			if (EmulatorSettings::GetSettingGroup((SettingType)i) == group) {
				options[curOption].name = EmulatorSettings::GetSettingName((SettingType)i);
				options[curOption].OnKey = Option_Any;
				options[curOption].GetDetail = Option_Detail;
				options[curOption].extraData = i;
				options[curOption].disabled = EmulatorSettings::GetSettingAvailable((SettingType)i) == false;
				curOption++;
			}
		}
		options[curOption].name = "Back";
		options[curOption].OnKey = OptionsBack;
		curOption++;

		nesFrontend.currentOptions = options;
		nesFrontend.numOptions = curOption;
		nesFrontend.selectedOption = 0;
		nesFrontend.selectOffset = 0;

		while (options[nesFrontend.selectedOption].disabled == true) {
			nesFrontend.selectedOption++;
		}

		return true;
	}

	return false;
}

static bool LeaveOptions(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesSettings.Save();
		nesFrontend.SetMainMenu();
		return true;
	}

	return false;
}

void MenuOption::Draw(int x, int y, bool bSelected) {
	int color = bSelected ? COLOR_WHITE : COLOR_AQUAMARINE;
	if (disabled) {
		color = COLOR_GRAY;
	}

	char buffer[128];
	sprintf(buffer, "%s%s", bSelected ? "=> " : "   ", name);

	CalcType_Draw(&commodore, buffer, x, y, color, 0, 0);

	if (GetDetail && disabled == false) {
		CalcType_Draw(&commodore, GetDetail(this), x + 150, y, color, 0, 0);
	}
}

nes_frontend::nes_frontend() {
	MenuBGHash = 0;
	currentOptions = nullptr;
	numOptions = 0;
	selectedOption = 0;
	selectOffset = 0;
}

// returns a dirty hash of the vram contents
uint32 GetVRAMHash() {
	const uint16* colors = (uint16*) GetVRAMAddress();
	uint32 numColors = LCD_WIDTH_PX * LCD_HEIGHT_PX;
	// sample 256 colors, hash them
	uint32 hash = 0x13371337;
	for (int i = 0; i < 256; i++) {
		hash = hash * 31 + colors[i*numColors / 256];
	}
	return hash;
}

void nes_frontend::RenderMenuBackground(bool bForceRedraw) {
	if (!bForceRedraw || MenuBGHash == 0) {
		LoadVRAM_1();
		if (GetVRAMHash() == MenuBGHash)
			return;
	}

	Bdisp_Fill_VRAM(0, 3);
	DrawFrame(0);

	extern PrizmImage gfx_logo;
	extern PrizmImage gfx_bg_warp;
	extern PrizmImage gfx_nes;

	PrizmImage* logo = &gfx_logo;
	PrizmImage* bg = &gfx_bg_warp;
	PrizmImage* nes = &gfx_nes;

#if TARGET_WINSIM
	if (bRebuildGfx) {
		logo = PrizmImage::LoadImage("\\\\dev0\\gfx\\logo.bmp");
		bg = PrizmImage::LoadImage("\\\\dev0\\gfx\\rays.bmp");
		nes = PrizmImage::LoadImage("\\\\dev0\\gfx\\nes.bmp");
		logo->Compress();
		bg->Compress();
		nes->Compress();
		logo->ExportZX7("gfx_logo", "src\\gfx\\logo.cpp");
		bg->ExportZX7("gfx_bg_warp", "src\\gfx\\bg_warp.cpp");
		nes->ExportZX7("gfx_nes", "src\\gfx\\nes_gfx.cpp");
	}
#endif

	logo->Draw_Blit(5, 5);
	bg->Draw_Blit(0, 71);
	nes->Draw_OverlayMasked(195, 38, 192);

	CalcType_Draw(&arial_small, "v0.95", 352, 203, COLOR_DARKGRAY, 0, 0);

	MenuBGHash = GetVRAMHash();
	SaveVRAM_1();
}

void nes_frontend::RenderGameBackground() {
	// note: this function cannot effect file system! Game should be considered active!
	Bdisp_Fill_VRAM(0, 3);
	DrawFrame(0);

	extern PrizmImage gfx_bg_warp;
	gfx_bg_warp.Draw_Blit(0, 71);
	Bdisp_PutDisp_DD();
}

void nes_frontend::Render() {
	RenderMenuBackground();

	int startY = 84;
	if (numOptions < 8) {
		startY += 16 * (8 - numOptions);
	}

	int curY = startY;
	for (int i = 0; i < 8 && i + selectOffset < numOptions; i++) {
		currentOptions[i+selectOffset].Draw(7, curY, i + selectOffset == selectedOption);
		curY += 16;
	}
}

void nes_frontend::SetMainMenu() {
	currentOptions = mainOptions;
	numOptions = 5;
	selectedOption = 0;

	if (nesCart.romFile[0] == 0) {
		// no ROM loaded, check for continue in settings
		if (nesSettings.GetContinueFile()) {
			mainOptions[0].disabled = false;
			mainOptions[0].OnKey = ROMFile_Selected;
			mainOptions[0].extraData = 1; // denotes continue file

			static char continueText[64];
			sprintf(continueText, "Load %s", nesSettings.GetContinueFile());
			mainOptions[0].name = continueText;
		} else {
			mainOptions[0].disabled = true;
			selectedOption = 1;
		}
	} else {
		mainOptions[0].disabled = false;
		mainOptions[0].OnKey = Continue_Selected;
		mainOptions[0].name = "Continue";
	}
	selectOffset = 0;
}

void shutdown() {}

void nes_frontend::Run() {
	SetQuitHandler(shutdown);

	do {
		Render();

#if TARGET_PRIZM
		extern void FillBlackBorder();
		FillBlackBorder();
#endif

		int key = 0;
		GetKey(&key);
		EnableStatusArea(3);

		// wait for unpress
		while (findKey() != 0) {}

		if (currentOptions[selectedOption].disabled ||
			currentOptions[selectedOption].OnKey((MenuOption*) &currentOptions[selectedOption], key) == false) {
			int keyOffset = 0;

			switch (key) {
				case KEY_CTRL_UP:
					keyOffset = numOptions - 1;
					break;
				case KEY_CTRL_DOWN:
					keyOffset = 1;
					break;
#if DEBUG
				case KEY_CTRL_F2:
				{
					ScopeTimer::DisplayTimes();
					break;
				}
#endif
				case KEY_CTRL_OPTN:
				{
					// look for "Back" option
					MenuOption* lastOption = &currentOptions[numOptions - 1];
					if (!strcmp(lastOption->name, "Back")) {
						lastOption->OnKey(lastOption, KEY_CTRL_SHIFT);
					}
					break;
				}
			}

			if (keyOffset) {
				do {
					selectedOption = (selectedOption + keyOffset) % numOptions;
				} while (currentOptions[selectedOption].disabled);

				if (numOptions > 7) {
					// keep selection on screen
					while (selectedOption - 2 < selectOffset && selectOffset > 0)
						selectOffset--;
					while (selectedOption + 3 > selectOffset + 8 && selectOffset < numOptions - 7)
						selectOffset++;
				}
			}
		}

		if (gotoGame) {
			if (getDeviceType() == DT_CG20 && nesSettings.GetSetting(ST_OverClock) != 0) {
				Ptune2_SaveBackup();

				switch (nesSettings.GetSetting(ST_OverClock)) {
					case 1:
						Ptune2_LoadSetting(PT2_HALFINC);
						break;
					case 2:
						Ptune2_LoadSetting(PT2_DOUBLE);
						break;
				}
			}

			nesFrontend.RenderGameBackground();

			nesAPU.startup();
			RunGameLoop();
			nesAPU.shutdown();

			if (getDeviceType() == DT_CG20 && nesSettings.GetSetting(ST_OverClock) != 0) {
				Ptune2_LoadBackup();
			}

			gotoGame = false;
		}
	} while (true);
}

void nes_frontend::RunGameLoop() {
	while (!shouldExit) {
		cpu6502_Step();
		if (mainCPU.clocks >= mainCPU.ppuClocks) nesPPU.step();
		if (mainCPU.irqClocks && mainCPU.clocks >= mainCPU.irqClocks) cpu6502_IRQ();
		if (mainCPU.clocks >= mainCPU.apuClocks) nesAPU.step();
	}

	nesCart.OnPause();
	shouldExit = false;
}

MenuOption mainOptions[] =
{
	{"Continue", "Continue the current loaded game", false, Continue_Selected, nullptr, 0 },
	{"Load ROM", "Select a ROM to load from your Root folder", false, LoadROM_Selected, nullptr, 0 },
	{"View FAQ", "View .txt file of the same name as your ROM", true, ViewFAQ_Selected, nullptr, 0 },
	{"Options", "Select a ROM to load from your Root folder", false, Options_Selected, nullptr, 0 },
	{"About", "Where did this emulator come from?", false, About_Selected, nullptr, 0 },
};

MenuOption optionTree[] = 
{
	{ "System", "System clock and main options", false, OptionMenu, nullptr, (int) SG_System },
	{ "Controls", "Controls options and keymappings", false, OptionMenu, nullptr, (int) SG_Controls },
	{ "Display", "Misc video options", false, OptionMenu, nullptr, (int) SG_Video },
	{ "Sound", "Sound options", false, OptionMenu, nullptr, (int) SG_Sound },
	{ "Back", "Return to main menu", false, LeaveOptions, nullptr, 0 }
};

MenuOption remapOptions[] = 
{
	{ "P1 A", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_A},
	{ "P1 B", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_B},
	{ "P1 Select", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_SELECT},
	{ "P1 Start", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_START},
	{ "P1 Up", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_UP},
	{ "P1 Down", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_DOWN},
	{ "P1 Left", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_LEFT},
	{ "P1 Right", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_RIGHT},
	{ "P1 Turbo A", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_TURBO_A},
	{ "P1 Turbo B", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P1_TURBO_B},
	{ "Save State", "", false, Option_RemapKey, Option_GetKeyDetails, NES_SAVESTATE},
	{ "Load State", "", false, Option_RemapKey, Option_GetKeyDetails, NES_LOADSTATE},
	{ "Fast Fwd", "", false, Option_RemapKey, Option_GetKeyDetails, NES_FASTFORWARD},
	{ "Volume Up", "", false, Option_RemapKey, Option_GetKeyDetails, NES_VOL_UP},
	{ "Volume Down", "", false, Option_RemapKey, Option_GetKeyDetails, NES_VOL_DOWN},
	{ "P2 A", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_A},
	{ "P2 B", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_B},
	{ "P2 Select", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_SELECT},
	{ "P2 Start", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_START},
	{ "P2 Up", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_UP},
	{ "P2 Down", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_DOWN},
	{ "P2 Left", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_LEFT},
	{ "P2 Right", "", false, Option_RemapKey, Option_GetKeyDetails, NES_P2_RIGHT},
	{ "Back", "Return to controls options", false, OptionMenu, nullptr, (int)SG_Controls },
};
