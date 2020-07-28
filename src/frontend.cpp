
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
	while (ret <= 10) {
		ret = findKey();
	}

	// wait for unpress
	while (findKey() > 10) {}

	return ret;
}

static int32 DrawInfoBox(const char* text1, const char* text2, const char* text3) {
	if (text1 == nullptr) text1 = "";
	if (text2 == nullptr) text2 = "";
	if (text3 == nullptr) text3 = "";
	PrizmImage::Draw_GradientRect(70, 73, 244, 69, 0b0000000011100000, COLOR_BLACK);
	PrizmImage::Draw_BorderRect(71, 74, 242, 67, 2, COLOR_AQUAMARINE);
	int32 w1 = CalcType_Width(&arial_small, text1);
	int32 w2 = CalcType_Width(&arial_small, text2);
	int32 w3 = CalcType_Width(&arial_small, text3);
	CalcType_Draw(&arial_small, text1, 192 - w1 / 2, 83, COLOR_WHITE, 0, 0);
	CalcType_Draw(&arial_small, text2, 192 - w2 / 2, 83 + arial_small.height + 6, COLOR_WHITE, 0, 0);
	CalcType_Draw(&arial_small, text3, 192 - w3 / 2, 83 + arial_small.height * 2 + 12, COLOR_WHITE, 0, 0);
	Bdisp_PutDisp_DD_stripe(73, 73 + 70);
	return waitKey();
}

static void PrintOptionDetail(const char* text, int x, int y, int textColor) {
	CalcType_Draw(&commodore, text, x + 150, y, textColor, 0, 0);
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
	static foundFile files[64];
	static int lastFound = 0;

	// only search once 
	if (!lastFound) {
		numFound = 0;
		FindFiles("\\\\fls0\\*.nes", files, numFound, 64);
		lastFound = numFound;
	}

	numFound = lastFound;
	return files;
}

static bool LoadROM(const char* filename, bool bShowCodes = true) {
	// set up rom file, tell nes to go to game
	cpu6502_Init();
	nesPPU.init();

	char romFile[128];
	sprintf(romFile, "\\\\fls0\\%s", filename);
	if (nesCart.loadROM(romFile)) {
		// check for game genie application and display
		const char* code0 = nullptr;
		const char* code1 = nullptr;
		int32 numCodes = 0;
		for (int i = 0; i < 10; i++) {
			if (nesSettings.codes[i].isActive()) {
				if (i == 0) code0 = nesSettings.codes[i].getText();
				if (i == 1) code1 = nesSettings.codes[i].getText();
				numCodes++;
			} else
				break;
		}
		if (numCodes && bShowCodes) {
			char appliedText[32];
			sprintf(appliedText, "Using %d Game Genie Code(s)", numCodes);
			DrawInfoBox(appliedText, code0, code1);
		}

		mainCPU.reset();

		return true;
	}

	return false;
}

static bool ROMFile_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		// unload existing game file if there is one:
		if (nesCart.romFile) {
			nesCart.romFile[0] = 0;
		}

		bool bLoaded = false;
		if (forOption->extraData) {
			// use continue file
			bLoaded = LoadROM(nesSettings.GetContinueFile());
		} else {
			bLoaded = LoadROM(forOption->name);
		}
		if (bLoaded) {
			if (forOption->extraData == 0 && (!nesSettings.GetContinueFile() || strcmp(forOption->name, nesSettings.GetContinueFile()))) {
				nesSettings.SetContinueFile(forOption->name);
				nesSettings.Save();
			}

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
	if (isSelectKey(key)) {
		if (!nesFrontend.loadFAQ()) {
			char faqName[128];
			nesFrontend.getFAQName(faqName);
			DrawInfoBox(
				"FAQ File Missing!",
				faqName,
				"should be in root directory."
			);
		} else {
			nesFrontend.viewFAQ();
			return true;
		}
	}

	return false;
}

static bool Options_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesFrontend.currentOptions = optionTree;
		nesFrontend.numOptions = 6;
		nesFrontend.selectedOption = 0;
		nesFrontend.selectOffset = 0;
		return true;
	}

	return false;
}

static bool About_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		DrawInfoBox(
			"By @TSWilliamson",
			"Contact via my Github account",
			"Source at github.com/TSWilliamson/nesizm"
		);

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

static void Option_GetKeyDetails(MenuOption* forOption, int x, int y, int textColor, bool selected) {
	int32 key = nesSettings.keyMap[forOption->extraData];
	if (key == 0) {
		PrintOptionDetail("None", x, y, textColor);
		return;
	}
	static char bufferCode[3];
	bufferCode[0] = key / 10 + '0';
	bufferCode[1] = key % 10 + '0';
	bufferCode[2] = 0;
	PrintOptionDetail(bufferCode, x, y, textColor);
	return;
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

static void renderPalette() {
	nesPPU.initPalette();

	const int gridSize = 9;
	const int gridSpacing = 1;
	const int totalGrid = gridSize + gridSpacing;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 16; x++) {
			int gridX = 384 - (16 - x) * totalGrid;
			int gridY = 4 + y * totalGrid;

			PrizmImage::Draw_FilledRect(gridX+1, gridY+1, gridSize-2, gridSize-2, nesPPU.rgbPalette[x + y * 16]);
			PrizmImage::Draw_BorderRect(gridX, gridY, gridSize, gridSize, 1, 0b0011100111000111);
		}
	}

	// check for valid custom palette
	extern const uint8* GetCustomPalette();
	if (nesSettings.GetSetting(ST_Palette) == 3 && GetCustomPalette() == nullptr) {
		CalcType_Draw(&arial_small, "No Custom.pal file found!", 384 - 16 * totalGrid, 4 * totalGrid + 5, COLOR_RED, 0, 0);
	}
}

static void Option_Detail(MenuOption* forOption, int x, int y, int textColor, bool bSelected) {
	SettingType type = (SettingType) forOption->extraData;
	const char* settingValue = EmulatorSettings::GetSettingValueName(type, nesSettings.GetSetting(type));
	DebugAssert(settingValue);
	PrintOptionDetail(settingValue, x, y, textColor);

	if (bSelected) {
		if (type == ST_Palette) {
			renderPalette();
		} else {
			forOption->DrawHelp();
		}
	}
}

static void Option_ColorPercentage(MenuOption* forOption, int x, int y, int textColor, bool bSelected) {
	SettingType type = (SettingType)forOption->extraData;
	int value = nesSettings.GetSetting(type);

	PrizmImage::Draw_GradientRect(x+146, y-1, value*10, 12, COLOR_BLUE, COLOR_DARKBLUE);
	PrizmImage::Draw_BorderRect(x+146, y-1, 100, 12, 1, textColor);

	char percentText[32];
	sprintf(percentText, "%d%%", value * 20);
	CalcType_Draw(&arial_small, percentText, x + 150, y, textColor, 0, 0);

	// calculate the actual palette and render it to the top right when selected
	if (bSelected) {
		renderPalette();
	}
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
			options[curOption].help = "Map which keys are used for each\ncontroller button";
			curOption++;
		}
		for (int32 i = 0; i < MAX_SETTINGS; i++) {
			if (EmulatorSettings::GetSettingGroup((SettingType)i) == group) {
				options[curOption].name = EmulatorSettings::GetSettingName((SettingType)i);
				options[curOption].help = EmulatorSettings::GetSettingHelp((SettingType)i);
				options[curOption].OnKey = Option_Any;
				if (EmulatorSettings::GetSettingValueName((SettingType)i, 0))
					options[curOption].DrawDetail = Option_Detail;
				else
					options[curOption].DrawDetail = Option_ColorPercentage;
				options[curOption].extraData = i;
				options[curOption].disabled = EmulatorSettings::GetSettingAvailable((SettingType)i) == false;
				curOption++;
			}
		}
		options[curOption].name = "Back";
		options[curOption].help = "Return to options menu";
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

static bool ResetOptions(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		int key = DrawInfoBox(
			"Are you sure?",
			"To reset all options",
			"Confirm with the EXE key"
		);

		if (key == 31) {
			nesSettings.SetDefaults();
			DrawInfoBox(nullptr, "Options reset.", nullptr);
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

	if (bSelected) {
		extern PrizmImage controls_select;
		controls_select.Draw_Blit(x, y-1);
	}

	CalcType_Draw(&commodore, name, x+29, y, color, 0, 0);

	if (DrawDetail && disabled == false) {
		DrawDetail(this, x, y, color, bSelected);
	} else if (bSelected) {
		DrawHelp();
	}
}

void MenuOption::DrawHelp() {
	if (help) {
		CalcType_Draw(&arial_small, help, 192, 10, COLOR_CADETBLUE, 0, 0);
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
uint32 nes_frontend::GetVRAMHash() {
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

#if TARGET_WINSIM
	if (bRebuildGfx) {
		PrizmImage* logo = PrizmImage::LoadImage("\\\\dev0\\gfx\\logo.bmp");
		PrizmImage* bg0 = PrizmImage::LoadImage("\\\\dev0\\gfx\\rays.bmp");
		PrizmImage* bg1 = PrizmImage::LoadImage("\\\\dev0\\gfx\\oldtv.bmp");
		PrizmImage* nes = PrizmImage::LoadImage("\\\\dev0\\gfx\\nes.bmp");
		PrizmImage* controls = PrizmImage::LoadImage("\\\\dev0\\gfx\\controls_select.bmp");
		logo->Compress();
		bg0->Compress();
		bg1->Compress();
		nes->Compress();
		logo->ExportZX7("gfx_logo", "src\\gfx\\logo.cpp");
		bg0->ExportZX7("gfx_bg_warp", "src\\gfx\\bg_warp.cpp");
		bg1->ExportZX7("gfx_bg_oldtv", "src\\gfx\\bg_oldtv.cpp");
		nes->ExportZX7("gfx_nes", "src\\gfx\\nes_gfx.cpp");
		controls->ExportZX7("controls_select", "src\\gfx\\controls_select.cpp");
	}
#endif

	extern PrizmImage gfx_logo;
	extern PrizmImage gfx_bg_warp;
	extern PrizmImage gfx_nes;

	gfx_logo.Draw_Blit(5, 5);
	gfx_bg_warp.Draw_Blit(0, 71);
	gfx_nes.Draw_OverlayMasked(195, 38, 192);

	CalcType_Draw(&arial_small, "v1.00", 352, 203, COLOR_DARKGRAY, 0, 0);

	MenuBGHash = GetVRAMHash();
	SaveVRAM_1();
}

void nes_frontend::RenderGameBackground() {
	// note: this function cannot effect file system! Game should be considered active!
	Bdisp_Fill_VRAM(0, 3);
	DrawFrame(0);
	nesPPU.scanlineOffset = 0;
	nesPPU.currentBGColor = 0;
	nesSettings.cachedTime = -1;

	if (nesSettings.GetSetting(ST_Background) == 0) {
		extern PrizmImage gfx_bg_warp;
		gfx_bg_warp.Draw_Blit(0, 71);
	} else if (nesSettings.GetSetting(ST_Background) == 1) {
		extern PrizmImage gfx_bg_oldtv;
		gfx_bg_oldtv.Draw_Blit(0, 0);
		if (nesSettings.GetSetting(ST_StretchScreen) == 0) {
			nesPPU.scanlineOffset = -44;
		} else if (nesSettings.GetSetting(ST_StretchScreen) == 1) {
			nesPPU.scanlineOffset = -42;
		}
	}

	Bdisp_PutDisp_DD();
}

static uint16 PrepareBuffer(unsigned short* buffer) {
	unsigned short bgColor = 0;
	unsigned short textColor = COLOR_DARKGRAY;
	switch (nesSettings.GetSetting(ST_Background)) {
		case 1:
			// TV color
			bgColor = 0b1000101011000100;
			textColor = COLOR_BLACK;
			break;
		case 2:
			// match game bg color
			bgColor = nesPPU.currentBGColor;
			if (bgColor != 0)
				textColor = COLOR_WHITE;
			break;
	}
	for (int i = 0; i < CLOCK_WIDTH * CLOCK_HEIGHT; i++) {
		buffer[i] = bgColor;
	}

	return textColor;
}

void nes_frontend::RenderTimeToBuffer(unsigned short* buffer) {
	char clockText[10];
	int hours = nesSettings.cachedTime / 256;
	int minutes = nesSettings.cachedTime % 256;
	if (nesSettings.GetSetting(ST_ShowClock) == 2) {
		switch (hours) {
			case 0x13: hours = 0x01; break;
			case 0x14: hours = 0x02; break;
			case 0x15: hours = 0x03; break;
			case 0x16: hours = 0x04; break;
			case 0x17: hours = 0x05; break;
			case 0x18: hours = 0x06; break;
			case 0x19: hours = 0x07; break;
			case 0x20: hours = 0x08; break;
			case 0x21: hours = 0x09; break;
			case 0x22: hours = 0x10; break;
			case 0x23: hours = 0x11; break;
			case 0x00: hours = 0x12; break;
		}
	}
	sprintf(clockText, "%d%d:%d%d", hours / 16, hours % 16, minutes / 16, minutes % 16);
	uint16 textColor = PrepareBuffer(buffer);
	CalcType_Draw(&arial_small, clockText, 2, 1, textColor, (uint8*) buffer, CLOCK_WIDTH);
}

void nes_frontend::RenderFPS(int32 fps, unsigned short* buffer) {
	char fpsText[10];
	sprintf(fpsText, "%d fps", fps);
	
	uint16 textColor = PrepareBuffer(buffer);
	CalcType_Draw(&arial_small, fpsText, 2, 1, textColor, (uint8*) buffer, CLOCK_WIDTH);
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
			mainOptions[2].disabled = false;
			mainOptions[0].OnKey = ROMFile_Selected;
			mainOptions[0].extraData = 1; // denotes continue file

			static char continueText[64];
			sprintf(continueText, "Load %s", nesSettings.GetContinueFile());
			mainOptions[0].name = continueText;
		} else {
			mainOptions[0].disabled = true;
			mainOptions[2].disabled = true;
			selectedOption = 1;
		}
	} else {
		mainOptions[0].disabled = false;
		mainOptions[2].disabled = false;
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
			nesPPU.initPalette(); // allows palette/screen options to change during session
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
		if (mainCPU.clocks >= mainCPU.apuClocks) nesAPU.step();

		// both APU and PPU can trigger an immediate IRQ
		if (mainCPU.irqMask) {
			if ((mainCPU.irqMask & 1) && mainCPU.clocks >= mainCPU.irqClock[0]) cpu6502_IRQ(0);
			else if ((mainCPU.irqMask & 2) && mainCPU.clocks >= mainCPU.irqClock[1]) cpu6502_IRQ(1);
			else if ((mainCPU.irqMask & 4) && mainCPU.clocks >= mainCPU.irqClock[2]) cpu6502_IRQ(2);
			else if ((mainCPU.irqMask & 8) && mainCPU.clocks >= mainCPU.irqClock[3]) cpu6502_IRQ(3);
		}
	}

	nesCart.OnPause();
	shouldExit = false;
}

void nes_frontend::ResetPressed() {
	int32 keyEntered =
		DrawInfoBox("Reset Game?", "Press EXE to reset", "Any other key to continue.");

	// EXE
	if (keyEntered == 31) {
		nesCart.unload();
		LoadROM(nesSettings.GetContinueFile(), false);
		nesPPU.initPalette();
	}

	RenderGameBackground();
}

MenuOption mainOptions[] =
{
	{"Continue", "Continue the current loaded game", false, Continue_Selected, nullptr, 0 },
	{"Load ROM", "Select a ROM to load from your\nRoot folder", false, LoadROM_Selected, nullptr, 0 },
	{"View FAQ", "View .txt file of the same name\nas your ROM", false, ViewFAQ_Selected, nullptr, 0 },
	{"Options", "Change controls, sound, or\nvideo options", false, Options_Selected, nullptr, 0 },
	{"About", "Where did this emulator come from?", false, About_Selected, nullptr, 0 },
};

MenuOption optionTree[] = 
{
	{ "System", "System clock and main options", false, OptionMenu, nullptr, (int) SG_System },
	{ "Controls", "Controls options and keymappings", false, OptionMenu, nullptr, (int) SG_Controls },
	{ "Display", "Misc video options", false, OptionMenu, nullptr, (int) SG_Video },
	{ "Sound", "Sound options", false, OptionMenu, nullptr, (int) SG_Sound },
	{ "Reset", "Reset all options to default", false, ResetOptions, nullptr, 0 },
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
