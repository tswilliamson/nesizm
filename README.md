# NESizm v 0.9
NESizm is a Nintendo Entertainment System emulator for the Casio Prizm series of graphics calculators. It currently supports the FX-CG20 and FX-CG50. NESizm was built from the ground up with performance in mind, while maintaining accurate emulation and compatibility wherever possible with clever caching, forced alignment, and hand written assembly where necessary. It runs most titles at 60 FPS with no overclocking on the FX-CG50.

This project has its roots in my interest in early game development technology, as well as the inherent benefits of the Prizm as a platform. There is a large install base of players who can now play NES over 100 hours of battery life with 0 input lag from keyboard to display.


## Install

Copy the nesizm.g3a file to your Casio Prizm calculator's root path when linked via USB. NES roms (.nes) also should go inside of the root directory. The filenames for these files should be simple and less than 12 characters, such as MyGame.nes. The emulator does not support the NES 2.0 ROM format, so stick with old style roms for now.

## Usage

### Menu

In the menu system use the arrow keys and SHIFT or ENTER to select.

When inside a game, the MENU key will exit to the settings screen, and pressing MENU again will take you back to the calculator OS.

### In Game

You can configure your own keys in the Settings menu, these are the default I found to work well:

- Dpad : Dpad
- A: SHIFT
- B: OPTN
- Select: F5
- Start: F6
- Save State : X (multiply), which is the alpha 's' key for save
- Load State : -> (store), which is the alpha 'l' key for load
- Increase Volume/Distortion : + 
- Decrease Volume/Distortion : -

### Save States

A single save state is supported per ROM, which can be loaded/saved using the remappable keys mentioned in the Controls section. These default to the 'S' and 'L' keys on the calculator. The save state file will be saved to your main storage with the .fcs extension.

These save states are generally intercompatible with FCEUX, the popular PC NES emulator. However, by default, FCEUX enables compression on its save states when saving, so in order to transport a save state back to your calculator, you need to disable save state compression in FCEUX.

## Support

The emulator now plays over 90% of the games I have been able to test smoothly at this point. There are scanline artifacts in  a number of them, but nothing that appears to really hamper gameplay.

Here is a basic mapper support table:

### Sound

The emulator has no current support for sound, but it is planned in the future.

## Building

My other repository, PrizmSDK, is required to build NESizm from source. Put your NESizm clone in the SDK projects directory.

To build on a Windows machine, I recommend using the project files using Visual Studio Community Edition, where I have NMAKE set up nicely. For other systems please refer to your Prizm SDK documentation on how to compile projects. Refer to the configuration options in make-DeviceRelease.bat.

If you do use Visual Studio, a project is included that uses a Windows Simulator I wrote that wraps Prizm OS functions so that the code and emulator can easily be tested and iterated on within Visual Studio. See the prizmsim.cpp/h code for details on its usage.

## Special Thanks

The NESde
<!--stackedit_data:
eyJoaXN0b3J5IjpbMTM4MTAxNzgwNywyOTM5NzU5MTZdfQ==
-->