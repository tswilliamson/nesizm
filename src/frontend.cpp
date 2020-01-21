
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
