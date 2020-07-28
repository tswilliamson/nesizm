![Menu Screen](gfx/Menu.png) ![Screenshot1](gfx/Shot1.png) 

# NESizm v 1.00
NESizm is a Nintendo Entertainment System emulator for the Casio Prizm series of graphics calculators. It currently supports the FX-CG10, FX-CG20, FX-CG50, and Graph 90+ E Casio calculators. NESizm was built from the ground up with performance in mind, while maintaining accurate emulation and compatibility wherever possible with clever caching, forced alignment, and hand written assembly where necessary. It runs most titles at 60 FPS with no overclocking on the FX-CG50.

This project has its roots in my interest in early game development technology, as well as the inherent benefits of the Prizm as a platform. There is a large install base of players who can now play NES over many many hours of battery life with 0 input lag from keyboard to display.

## Install

Copy the nesizm.g3a file (or nesizm_cg10.g3a if you have an FX-CG10) to your Casio Prizm calculator's root path when linked via USB. NES roms (.nes) also should go inside of the root directory. Keep filenames for these files should be simple and less than 32 characters, such as MyGame.nes. The emulator does support the NES 2.0 ROM format, but I haven't extensively tested it.

## Usage

### Menu

In the menu system use the arrow keys and SHIFT or ENTER to select. You can also use ALPHA to quickly back out of a submenu. Help for the current selected option is in the upper right.

When inside a game, the MENU key will exit to the settings screen, and pressing MENU again will take you back to the calculator OS.

### In Game

You can configure your own keys in the Settings menu, these are the default I found to work well:

- Dpad : Dpad
- A: SHIFT
- B: OPTN
- Select: F5
- Start: F6
- Turbo A: Alpha
: Turbo B: X^2
- Save State : X (multiply), which is the alpha 'S' key for save
- Load State : -> (store), which is the alpha 'L' key for load
- Fast Forward: ^ (caret), runs CPU at full speed running rendering every 8th frame, about 3x speed
- Volume Up: + (plus), increases the volume, though at the maximum level this will cause more distortion
- Volume Down - (minus), decreases the volume, though at lower levels it will have more scratchiness and lower accuracy
- Reset Cart : AC/On. This is not re-configurable, and could very well lose your save data on games with battery backed saves.

Note that if you set the turbo setting to 30 Hz, this may be too fast for some games causing them to malfunction.

### Two Players

If you desire to play with a second controller, it can be mapped to other buttons on the calculator using the Remap Buttons option in Options->Controls. This is very cumbersome, however, so by default Player 2 is not mapped to any buttons.

### Save States
 
A single save state is supported per ROM, which can be loaded/saved using the remappable keys mentioned in the Controls section. These default to the 'S' and 'L' keys on the calculator. The save state file will be saved to your main storage with the .fcs extension.

These save states are generally intercompatible with FCEUX, the popular PC NES emulator. However, by default, FCEUX enables compression on its save states when saving, so in order to transport a save state back to your calculator, you need to disable save state compression in FCEUX.

### Battery Backed Support

If a ROM uses a battery backed feature, such as Legend of Zelda, this memory will automatically be saved when you return to the main menu with the MENU button. Keep in mind that using save states will completely overwrite the battery backed data.

## Display Options

### Screen Stretch

The screen stretch option utilizes the high refresh rate of the screen to interlace the signal, which is very similar to how old interlaced TVs work. This works out with the TFT color screen, which has a slight color latency on the Prizm, to be very difficult to notice. If you set the Stretch option to 4:3, it will very closely match the proportions intended for the original games. However, it requires a high FPS. So the emulator visuals will start to look glitchy if you have a very high frameskip option set. In general, setting your options to a very high frameskip should be unnecessary. The calculator battery life does not appear to be heavily effected by frame skip. 

### Palettes and Color

3 Palettes are included to choose from. They all feel a little different so choose the one that you think is the best. There's no true correct color for the NES in a lot of respects, as the TV's of the era all could interpret the signal from the NES slightly differently. Once you select a palette, you can adjust the brightness (to a very low level if you are in a dark environment) and the color saturation amounts. Keep in mind that increasing the brightness will only increase the relative brightness of the colors used, not the brightness of your screen.

#### Custom palette

If you are so inclined, a custom palette can be used. There are 192 byte palette files you can find by easily searching online. Rename your desired palette to "Custom.pal" and include it in the root directory of your file storage. It should be selectable as the 4th palette option at this point.

### Backgrounds

4 Background options are available. The 'Warp' background matches the main menu. The TV background provides a nostalgic old TV background, particularly in the smaller screen stretch modes. The Game BG Color option will match the currently selected game background palette color. The usage of this varies per game but it works well in some, such as the blue/black background selections in Super Mario Bros.

### In-game clock

An in-game clock can optionally be shown in the lower right corner of the screen if you need to keep track of time. It does require set up though, as the clock is not commonly used in the calculator's OS. Set your clock with a 3rd party app, such as G-CLOCK or also inside of gbl08ma's excellent Utilities app.

FPS can also be shown in this corner of the screen. Keep in mind this is simply the frames drawn in a given second, not the skipped frames. So if this says 40 FPS and your frame skip option is set to Auto, the emulator is still running at 100% speed, but is skipping every 3rd frame when drawing to the screen to save processing.

## Support

![Screenshot3](gfx/Shot3.png)

The emulator supports almost all games with a compatible mapper with no issues. If you encounter a game with truly significant issues, please let me know. 

Here is the mapper support table. The emulator supports almost all US NTSC and PAL mappers, excluding MMC5, covering 98% of the commercially released games in those regions. Famicom Disk and VS System support is not planned.

Mapper Name | Publisher | Name | Publisher | Name | Publisher
-|-|-|-|-|-
MMC1 | Nintendo  | AxROM | Nintendo             | Nanjing 163 | Nanjing 
MMC2 | Nintendo	 | GxROM | Nintendo				| Sunsoft 3 | Sunsoft  
MMC3 | Nintendo	 | BNROM | Nintendo				| Sunsoft 4 | Sunsoft
MMC4 | Nintendo	 | NULL / NROM | Several		| Sunsoft 5 | Sunsoft
MMC6 | Nintendo	 | Color Dreams | Color Dreams	| RAMBO-1 | Tengen     
CNROM | Nintendo | NINA-03/06 | AVE				| Nina-1 | Irem
UNROM | Nintendo | Camerica | Yes  				| JF-11/14 | Jaleco

A full table of ROMs and mappers can be found here, but I don't keep it entirely up to date:
https://docs.google.com/spreadsheets/d/1TfgiU6doDaGvIzSMY3flPSmZviFKiysoxi9uwl-RlRY/edit?usp=sharing

### PAL Titles

![Screenshot2](gfx/Shot2.png) 

PAL ROMS run at a different frame rate of 50 Hz compared to games from the USA and Japan (NTSC) which run at 60 Hz. This, plus a few other timing differences means they needs to be emulated differently. Unfortunately, most ROM files for PAL games do not accurately identify themselves as requiring PAL emulation. With NESizm, you can properly run a PAL game by including (E) or PAL (ALL CAPS) in the ROM filename, such as GamePAL.nes

## Game Genie

Game genie codes are supported via a separate file per game. If you have a text file with the file name "GameName.gg" where GameName matches your ROM file minus the nes extension, with line separated game genie codes, the emulator will load and use those codes when the ROM is loaded. You will be presented with a dialog in the menu when this happens successfully. I recommend the Utilities app by gbl08ma if you would like to edit codes manually using a text editor on the calculator. You can find that at https://github.com/gbl08ma/utilities

## FAQ Viewer

If you have a txt file with the same name as your ROM file, it will be visible while the game is loaded in the FAQ viewer via the main menu. The FAQ viewer will remember your previous position as you play. Navigate the FAQ with the arrow keys, your position is shown via the scroll bar on the right. You can jump through 10% increments of the file as well with the 0-9 keys.

## Sound

The emulator has full support for sound. The sound is a fine tuned algorithm for 1 bit sound being adjusted over 500,000 times a second. Unfortunately the sound routines will slow down emulation by about 20%, but with frame skip this is not too bad. You can also use a utility such as Ptune3 to overclock your CG50 to get 60 FPS with sound, but I do not recommend it as it is a battery drain. Enable it in the sound menu in the options. In order to use it, you'll need a 2.5 mm (male) to 3.5 mm (female) adaptor. These can be found for a couple bucks at various online vendors, nothing fancy needed! I also recommend over ear headphones for enjoyment of the sound signal.

An additional quality option is available that is more of a subjective thing. The quality option will decrease the triangle and noise wave generators based on the DMC generator just like the original NES, and adds an extra low pass filter to avoid quick shifts. In practice with one-bit sound though, it doesn't appear to be enormously important.

## Building

My other repository, PrizmSDK, is required to build NESizm from source. Put your NESizm clone in the SDK projects directory.

To build on a Windows machine, I recommend using the project files using Visual Studio Community Edition, where I have NMAKE set up nicely. For other systems please refer to your Prizm SDK documentation on how to compile projects. Refer to the configuration options in make-DeviceRelease.bat.

If you do use Visual Studio, a project is included that uses a Windows Simulator I wrote that wraps Prizm OS functions so that the code and emulator can easily be tested and iterated on within Visual Studio. See the prizmsim.cpp/h code for details on its usage.

## Special Thanks

The Nesdev wiki, found at http://wiki.nesdev.com/ was incredibly useful in the development of NESizm. My sincerest gratitude to the community of emulator developers who collected all of the information I needed to write an emulator in a single place.

FCEUX, found at http://www.fceux.com provided great debugging tools that allowed me to compare and contrast my emulator state easily for games that had compatibility issues. Show them some love by using their emulator on PC!

Critor of TI-Planet.org for his excellent write ups on NESizm and for the general Casio community. His excitement is infectious!