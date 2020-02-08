
#include "platform.h"
#include "debug.h"

#include "frontend.h"
#include "imageDraw.h"
#include "nes.h"

/*
Menu Layout
	Continue - Loads the most recently played game and its corresponding save state / Continue playing game
	If available: View FAQ - View the game.txt file loaded in parallel with this ROM
	Load ROM - Load a new ROM file
	Options - Change various emulator options
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
			Chop sides - Many emulators chop the left/right 8 most pixels, though these pixels were visible on most TVs
				[On, Off]
			Background - Select a background image when playing (see readme for custom background support)
				[Game BG Color, Warp Field, Old TV, Black, Custom]
			Back - Return to Options
		Sound
			Enable - Whether sounds are enabled or not
				[On, Off]
			Sample Rate - Select sample rate quality (High = slower speed)
				[Low, High]
			Back - Return to Options
		Return to Main Menu
	About - Contact me on Twitter at @TSWilliamson or stalk me on the cemetech forums under the same username
*/

#include "calctype/fonts/commodore/commodore.c"		// For Menus

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

static bool Continue_Selected(MenuOption* forOption, int key) {
	if (isSelectKey(key)) {
		nesFrontend.RenderGameBackground();
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
		Bfile_NameToStr_ncpy(toArray[numFound++].path, found, 32);
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
		// unload existing game if there is one:
		if (nesCart.handle) {
			nesCart.unload();
		}

		// set up rom file, tell nes to go to game
		cpu6502_Init();
		nesPPU.init();

		char romFile[128];
		sprintf(romFile, "\\\\fls0\\%s", forOption->name);
		if (nesCart.loadROM(romFile)) {
			nesFrontend.RenderGameBackground();

			mainCPU.reset();

			nesFrontend.gotoGame = true;
		} else {
			// printf has errors
			int key = 0;
			GetKey(&key);
		}

		free((void*)nesFrontend.currentOptions);
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
	return false;
}

static bool About_Selected(MenuOption* forOption, int key) {
	return false;
}

MenuOption mainOptions[] =
{
	{"Continue", "Continue the current loaded game", false, Continue_Selected, nullptr, nullptr },
	{"Load ROM", "Select a ROM to load from your Root folder", false, LoadROM_Selected, nullptr, nullptr },
	{"View FAQ", "View .txt file of the same name as your ROM", true, ViewFAQ_Selected, nullptr, nullptr },
	{"Options", "Select a ROM to load from your Root folder", false, Options_Selected, nullptr, nullptr },
	{"About", "Where did this emulator come from?", false, About_Selected, nullptr, nullptr },
};

void MenuOption::Draw(int x, int y, bool bSelected) const {
	int color = bSelected ? COLOR_WHITE : COLOR_AQUAMARINE;
	if (disabled) {
		color = COLOR_GRAY;
	}

	char buffer[128];
	sprintf(buffer, "%s%s", bSelected ? "=> " : "   ", name);

	CalcType_Draw(&commodore, buffer, x, y, color, 0, 0);
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

	CalcType_Draw(&commodore, "@TSWilliamson", 255, 5, COLOR_AQUAMARINE, 0, 0);
	CalcType_Draw(&commodore, "v0.9", 340, 198, 0x302C, 0, 0);

	MenuBGHash = GetVRAMHash();
	SaveVRAM_1();
}

void nes_frontend::RenderGameBackground() {
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
	if (nesCart.handle == 0) {
		// no ROM loaded
		mainOptions[0].disabled = true;
		selectedOption = 1;
	} else {
		mainOptions[0].disabled = false;
		selectedOption = 0; 
	}
	selectOffset = 0;
}

void shutdown() {}

void nes_frontend::Run() {
	SetQuitHandler(shutdown);

	do {
		Render();

		int key = 0;
		GetKey(&key);

		if (currentOptions[selectedOption].OnKey((MenuOption*) &currentOptions[selectedOption], key) == false) {
			int keyOffset = 0;

			switch (key) {
				case KEY_CTRL_UP:
					keyOffset = numOptions - 1;
					break;
				case KEY_CTRL_DOWN:
					keyOffset = 1;
					break;
				case KEY_CTRL_F2:
				{
					ScopeTimer::DisplayTimes();
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
			RunGameLoop();
			gotoGame = false;
		}
	} while (true);
}

void nes_frontend::RunGameLoop() {
	while (!shouldExit) {
		cpu6502_Step();
		if (mainCPU.clocks >= mainCPU.ppuClocks) nesPPU.step();
		if (mainCPU.irqClocks && mainCPU.clocks >= mainCPU.irqClocks) cpu6502_IRQ();
	}

	nesCart.OnPause();
	shouldExit = false;
}