#pragma once

// sound baud rate
#if TARGET_PRIZM
#define SOUND_RATE (7168 * 2)
#else
#define SOUND_RATE 8192
#endif

// initializes the platform sound system, called when emulation begins. Returns false on error
bool sndInit();

// platform update from emulator for sound system
void sndUpdate();

// cleans up the platform sound system, called when emulation ends
void sndCleanup();

// volume controls (called per 10 frames or so if button pressed, if platform implemented)
void sndVolumeUp();
void sndVolumeDown();

// sound channel inits come from writes to bit 7 of certain IO mem regs
void sndChannelInit(int channelNum);

// called on rom start up to initialize sound registers
void sndStartup();

// called from the platform sound system to fill a 1/60 second buffer (range 0-15750) based on current sound values
void sndFrame(int* buffer, int length);

// called from emulator (once per frame, so not quite every 1/64th of a second) to emulate register updates when sound emulation is turned off
void sndInactiveFrame();

extern bool soundEnabled;

#if TARGET_PRIZM
#define condSoundUpdate() if (soundEnabled) sndUpdate()
#endif