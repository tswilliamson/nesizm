
// Main NES APU implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"	
#include "settings.h"

#include "snd/snd.h"

nes_apu nesAPU;
bool bSoundEnabled = false;

static const int pulse_duty[4][16] = {
	{ 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // 12.5% duty
	{ 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // 25% duty
	{ 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},   // 50% duty
	{ 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}    // -25% duty
};

// how much of a duty cycle sample from above to move through per sample, divided by 256
int pulse_delta(nes_apu_pulse* forPulse) {
	int t = forPulse->rawPeriod;
	if (nesCart.isPAL) {
		const int palScalar = int(SOUND_RATE * 0.630682);
		return palScalar * (t - 1) / 256;
	} else {
		static float l = 16.0f, l2 = 1.0f;
		float f = 1789773.0f / (16 * (t + 1));
		float lam = SOUND_RATE * 4.0f * f / 1000000.0f;
		return int(l * lam / l2);
		//const int ntscScalar = int(SOUND_RATE * 0.585871);
		//return ntscScalar * (t - 1) / 256;
	}
}

// calculates the target period of the sweep unit
inline int sweep_target(nes_apu_pulse* forPulse) {
	int ret = forPulse->rawPeriod;
	int shift = ret >> forPulse->sweepBarrelShift;
	if (forPulse->sweepNegate) {
		shift = -shift;
		if (forPulse->sweepTwosComplement)
			shift--;
	}
	return ret + shift;
}

static uint8 length_counter_table[32] = {
	10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

void nes_apu_pulse::writeReg(unsigned int regNum, uint8 value) {
	union regHelper {
		struct {
			uint8 volume : 4;
			uint8 constant_volume : 1;
			uint8 loop_or_halt : 1;
			uint8 duty : 2;
		};
		struct {
			uint8 sweep_shift : 3;
			uint8 sweep_negate : 1;
			uint8 sweep_period : 3;
			uint8 sweep_enable : 1;
		};
		struct {
			uint8 timer_low : 8;
		};
		struct {
			uint8 timer_hi : 3;
			uint8 length_load : 5;
		};
		uint8 value;
	};
	regHelper helper;
	helper.value = value;

	switch (regNum) {
		case 0:
			dutyCycle = helper.duty;
			enableLengthCounter = helper.loop_or_halt == 0;
			if (helper.constant_volume) {
				useConstantVolume = true;
				constantVolume = helper.volume; 
			} else {
				useConstantVolume = false;
				envelopePeriod = helper.volume + 1;
			}
			break;
		case 1:
			sweepEnabled = helper.sweep_enable;
			sweepPeriod = helper.sweep_period + 1;
			sweepNegate = helper.sweep_negate != 0;
			sweepBarrelShift = helper.sweep_shift;
			sweepTargetPeriod = sweep_target(this);
			sweepCounter = 0;
			break;
		case 2:
			rawPeriod = (rawPeriod & 0x700) | helper.timer_low;
			sweepTargetPeriod = sweep_target(this);
			break;
		case 3:
			rawPeriod = (rawPeriod & 0xFF) | (helper.timer_hi << 8);
			lengthCounter = length_counter_table[helper.length_load];
			sweepTargetPeriod = sweep_target(this);
			envelopeVolume = 15;
			envelopeCounter = 0;
			break;
	}
}

void nes_apu_pulse::step_quarter() {
	// tick envelope if used
	if (useConstantVolume == false) {
		envelopeCounter++;
		if (envelopeCounter >= envelopePeriod) {
			if (envelopeVolume == 0) {
				if (enableLengthCounter == false) {
					// then we have a looping envelope
					envelopeVolume = 15;
				}
			} else {
				envelopeVolume--;
			}
			envelopeCounter = 0;
		}
	}
}

void nes_apu_pulse::step_half() {
	// tick sweep if used
	if (sweepEnabled) {
		sweepCounter++;
		if (sweepCounter >= sweepPeriod) {
			// only update the period if a non-sweep muted state
			if (sweepTargetPeriod <= 0x7ff || rawPeriod >= 8) {
				rawPeriod = sweepTargetPeriod;
			}
			sweepTargetPeriod = sweep_target(this);
			sweepCounter = 0;
		}
	}

	// tick length counter if used
	if (enableLengthCounter && lengthCounter) {
		lengthCounter--;
	}
}

void sndFrame(int* buffer, int length) {
	nesAPU.mix(buffer, length);
}

void nes_apu::init() {
	memset(this, 0, sizeof(nes_apu));
	pulse2.sweepTwosComplement = true;
}

void nes_apu::startup() {
	bSoundEnabled = nesSettings.GetSetting(ST_SoundEnabled) != 0;
	if (bSoundEnabled) {
		sndInit();
	}
}

void nes_apu::writeReg(unsigned int address, uint8 value) {
	if (address < 0x04) {
		pulse1.writeReg(address, value);
	} else if (address < 0x08) {
		pulse2.writeReg(address & 0x3, value);
	} else if (address < 0x0C) {
		// triangle
	} else if (address < 0x10) {
		// noise
	} else if (address < 0x14) {
		// DMC
	} else if (address == 0x15) {
		// channel flags : DNT21
		if ((value & 1) == 0) pulse1.lengthCounter = 0;
		if ((value & 2) == 0) pulse2.lengthCounter = 0;
	} else if (address == 0x17) {
		// frame counter
		if (value & 0x80) {
			// clocks the counters if mode is set
			step_quarter();
			step_half();
		}

		// reset step counter
		mainCPU.apuClocks = mainCPU.clocks + 7457;
		cycle = 0;
	}
}

void nes_apu::step() {
	switch (cycle) {
		case 0:
			step_quarter();
			cycle = 1;
			mainCPU.apuClocks += 7456;
			break;
		case 1:
			step_quarter();
			step_half();
			cycle = 2;
			mainCPU.apuClocks += 7458;
			break;
		case 2:
			step_quarter();
			cycle = 3;
			mainCPU.apuClocks += 7459;
			break;
		case 3:
			if (mode == 0) {
				step_quarter();
				step_half();
				cycle = 0;
				mainCPU.apuClocks += 7457;
			} else {
				cycle = 4;
				mainCPU.apuClocks += 7452;
			}
			break;
		case 4:
			step_quarter();
			step_half();
			cycle = 0;
			mainCPU.apuClocks += 7457;
			break;
	}
}

void nes_apu::step_quarter() {
	pulse1.step_quarter();
	pulse2.step_quarter();
}

void nes_apu::step_half() {
	pulse1.step_half();
	pulse2.step_half();
}

void nes_apu::shutdown() {
	if (bSoundEnabled) {
		sndCleanup();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MIX

void nes_apu::mix(int* intoBuffer, int length) {
	int pulse1Volume = 500 * (pulse1.useConstantVolume ? pulse1.constantVolume : pulse1.envelopeVolume);
	if (pulse1.sweepTargetPeriod > 0x7FF || pulse1.lengthCounter == 0 || pulse1.rawPeriod < 8)
		pulse1Volume = 0;

	if (pulse1Volume) {
		int pulse1_delta = pulse_delta(&pulse1);
		const int* pulse1_duty = pulse_duty[pulse1.dutyCycle];

		for (int32 i = 0; i < length; i++) {
			pulse1.mixOffset = (pulse1.mixOffset + pulse1_delta) & 0xFFFF;
			intoBuffer[i] = pulse1_duty[(pulse1.mixOffset >> 12)] * pulse1Volume;
		}
	} else {
		for (int32 i = 0; i < length; i++) {
			intoBuffer[i] = 0;
		}
	}
	
	int pulse2Volume = 500 * (pulse2.useConstantVolume ? pulse2.constantVolume : pulse2.envelopeVolume);
	if (pulse2.sweepTargetPeriod > 0x7FF || pulse2.lengthCounter == 0 || pulse2.rawPeriod < 8)
		pulse2Volume = 0;

	if (pulse2Volume) {
		int pulse2_delta = pulse_delta(&pulse2);
		const int* pulse2_duty = pulse_duty[pulse2.dutyCycle];

		for (int32 i = 0; i < length; i++) {
			pulse2.mixOffset = (pulse2.mixOffset + pulse2_delta) & 0xFFFF;
			intoBuffer[i] += pulse2_duty[(pulse2.mixOffset >> 12)] * pulse2Volume;
		}
	}
}