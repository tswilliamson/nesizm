// encapsulates the entire user frontend, and manages the high level state of the emulator
#pragma once

#include "platform.h"

struct MenuOption {
	friend class nes_frontend;

	const char* name;
	const char* help;
	bool disabled;

	// returns true if key is handled
	bool(*OnKey)(MenuOption*, int withKey);

	// returns detail shown to the right of option, null if unused
	const char*(*GetDetail)(MenuOption*);

	// extra data (used for options type)
	int32 extraData;

protected:
	// draw the menu option to VRAM at the given pixel location
	void Draw(int x, int y, bool bSelected);
};

class nes_frontend {
private:
	// hash of the diagonal pixels of the menu screen to determine overwrite by OS
	uint32 MenuBGHash;

	void RenderMenuBackground(bool bForceRedraw = false);
	void Render();

public:
	nes_frontend();

	MenuOption* currentOptions;
	int numOptions;
	int selectedOption;
	int selectOffset;

	bool gotoGame;

	void RenderGameBackground();
	void SetMainMenu();
	void Run();
	void RunGameLoop();
};

extern nes_frontend nesFrontend;