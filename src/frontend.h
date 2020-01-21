// encapsulates the entire user frontend, and manages the high level state of the emulator
#pragma once

#include "platform.h"

class MenuOption {
	friend class FrontEnd;

	const char* name;
	const char* help;

protected:
	MenuOption(const char* withName, const char* withHelp) : name(withName), help(withHelp) {}

	// draw the menu option to VRAM at the given pixel location
	virtual void Draw(int x, int y, bool bSelected) const;

	// returns true if selection requires redraw of screen
	virtual bool OnSelect() const;
};

class FrontEnd {
private:
	// hash of the diagonal pixels of the menu screen to determine overwrite by OS
	uint32 MenuBGHash;

	void RenderMenuBackground();
	void Render();
public:
	const MenuOption* currentOptions;
	int numOptions;

	void Startup();
	void Run();
};
extern FrontEnd nes_frontend;