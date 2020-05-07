
// Main NES APU implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"	
#include "settings.h"

#include "snd/snd.h"

nes_apu nesAPU;
bool bSoundEnabled = false;

// how much of a duty cycle sample from above to move through per sample, divided by 256
int duty_delta(int t) {
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

static uint8 length_counter_table[32] = {
	10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU PULSE

static const int pulse_duty[4][16] = {
	{ 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // 12.5% duty
	{ 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // 25% duty
	{ 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},   // 50% duty
	{ 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}    // -25% duty
};

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU TRIANGLE

static const int tri_duty[32] = {
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0,
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
};

void nes_apu_triangle::writeReg(unsigned int regNum, uint8 value) {
	union regHelper {
		struct {
			uint8 linear_counter : 7;
			uint8 enable_counter : 1;
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
			enableLengthCounter = helper.enable_counter == 0;
			linearPeriod = helper.linear_counter;
			break;
		case 1:
			break;
		case 2:
			rawPeriod = (rawPeriod & 0x700) | helper.timer_low;
			break;
		case 3:
			rawPeriod = (rawPeriod & 0xFF) | (helper.timer_hi << 8);
			lengthCounter = length_counter_table[helper.length_load];
			reloadLinearCounter = true;
			break;
	}
}

void nes_apu_triangle::step_quarter() {
	// tick linear counter
	if (reloadLinearCounter) {
		linearCounter = linearPeriod;
		reloadLinearCounter = enableLengthCounter == false;
	} else if (linearCounter != 0) {
		linearCounter--;
	}
}

void nes_apu_triangle::step_half() {
	// tick length counter if used
	if (enableLengthCounter && lengthCounter) {
		lengthCounter--;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU NOISE

// number of samples between noise shift register switches (x16, clamped at 16)
inline int noise_samples(int noisePeriod) {
	int samples = (noisePeriod * 2 * SOUND_RATE / 111861);
	static int mult = 1;
	static int div = 1;
	samples *= mult;
	samples /= div;
	if (samples < 16) return 16;
	return samples;
}

static const int noise_period_ntsc[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

static const int noise_period_pal[16] = {
	4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778
};

void nes_apu_noise::writeReg(unsigned int regNum, uint8 value) {
	union regHelper {
		struct {
			uint8 volume : 4;
			uint8 constant_volume : 1;
			uint8 loop_or_halt : 1;
			uint8 _unused0 : 2;
		};
		struct {
			uint8 noise_period : 4;
			uint8 _unused2 : 3;
			uint8 noise_mode : 1;
		};
		struct {
			uint8 _unused3 : 3;
			uint8 length_load : 5;
		};
		uint8 value;
	};
	regHelper helper;
	helper.value = value;

	int noisePeriod;
	switch (regNum) {
		case 0:
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
			break;
		case 2:
			if (nesCart.isPAL) {
				noisePeriod = noise_period_pal[helper.noise_period];
			} else {
				noisePeriod = noise_period_ntsc[helper.noise_period];
			}
			samplesPerPeriod = noise_samples(noisePeriod);
			if (clocks > samplesPerPeriod) {
				clocks = clocks & 0xF;
			}
			noiseMode = helper.noise_mode;
			break;
		case 3:
			lengthCounter = length_counter_table[helper.length_load];
			envelopeVolume = 15;
			envelopeCounter = 0;
			break;
	}
}

void nes_apu_noise::step_quarter() {
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

void nes_apu_noise::step_half() {
	// tick length counter if used
	if (enableLengthCounter && lengthCounter) {
		lengthCounter--;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU

void sndFrame(int* buffer, int length) {
	nesAPU.mix(buffer, length);
}

void nes_apu::init() {
	memset(this, 0, sizeof(nes_apu));
	pulse2.sweepTwosComplement = true;
	noise.shiftRegister = 1;
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
		triangle.writeReg(address & 0x3, value);
	} else if (address < 0x10) {
		noise.writeReg(address & 0x3, value);
	} else if (address < 0x14) {
		// DMC
	} else if (address == 0x15) {
		// channel flags : DNT21
		if ((value & 1) == 0) pulse1.lengthCounter = 0;
		if ((value & 2) == 0) pulse2.lengthCounter = 0;
		if ((value & 4) == 0) triangle.lengthCounter = 0;
		if ((value & 8) == 0) noise.lengthCounter = 0;
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
	triangle.step_quarter();
	noise.step_quarter();
}

void nes_apu::step_half() {
	pulse1.step_half();
	pulse2.step_half();
	triangle.step_half();
	noise.step_half();
}

void nes_apu::shutdown() {
	if (bSoundEnabled) {
		sndCleanup();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MIX

void nes_apu::mix(int* intoBuffer, int length) {
	int pulse1Volume = 200 * (pulse1.useConstantVolume ? pulse1.constantVolume : pulse1.envelopeVolume);
	if (pulse1.sweepTargetPeriod > 0x7FF || pulse1.lengthCounter == 0 || pulse1.rawPeriod < 8)
		pulse1Volume = 0;

	if (pulse1Volume) {
		int pulse1_delta = duty_delta(pulse1.rawPeriod);
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
	
	int pulse2Volume = 200 * (pulse2.useConstantVolume ? pulse2.constantVolume : pulse2.envelopeVolume);
	if (pulse2.sweepTargetPeriod > 0x7FF || pulse2.lengthCounter == 0 || pulse2.rawPeriod < 8)
		pulse2Volume = 0;

	if (pulse2Volume) {
		int delta = duty_delta(pulse2.rawPeriod);
		const int* duty = pulse_duty[pulse2.dutyCycle];

		for (int32 i = 0; i < length; i++) {
			pulse2.mixOffset = (pulse2.mixOffset + delta) & 0xFFFF;
			intoBuffer[i] += duty[(pulse2.mixOffset >> 12)] * pulse2Volume;
		}
	}

	int triVolume = 200;
	if (triangle.linearCounter == 0 || triangle.lengthCounter == 0 || triangle.rawPeriod < 2)
		triVolume = 0;

	if (triVolume) {
		int delta = duty_delta(triangle.rawPeriod * 2);

		for (int32 i = 0; i < length; i++) {
			triangle.mixOffset = (triangle.mixOffset + delta) & 0xFFFF;
			intoBuffer[i] += tri_duty[(triangle.mixOffset >> 11)] * triVolume;
		}
	} else {
		triangle.mixOffset = (triangle.mixOffset + duty_delta(triangle.rawPeriod) * length) & 0xFFFF;
	}

	int noiseVolume = 200 * (noise.useConstantVolume ? noise.constantVolume : noise.envelopeVolume);
	if (noise.lengthCounter == 0)
		noiseVolume = 0;

	if (noiseVolume) {
		int remainingLength = length;
		int* bufferWrite = intoBuffer;

		while (remainingLength > 0) {
			int curFeedback = noise.shiftRegister & 1;
			if (noise.noiseMode)
				curFeedback = curFeedback ^ ((noise.shiftRegister >> 6) & 1);
			else
				curFeedback = curFeedback ^ ((noise.shiftRegister >> 1) & 1);
			int curFeedbackVolume = curFeedback ? noiseVolume : 0;

			int toMixClocks = noise.samplesPerPeriod - noise.clocks;
			int toMixSamples = toMixClocks >> 4;
			if (remainingLength < toMixSamples) {
				toMixSamples = remainingLength;
				noise.clocks = (toMixSamples << 4);
			} else {
				noise.shiftRegister = (noise.shiftRegister >> 1) | (curFeedback << 14);
				noise.clocks = (toMixSamples << 4) - toMixClocks;
			}

			if (curFeedbackVolume) {
				for (int i = 0; i < toMixSamples; i++) {
					*(bufferWrite++) += curFeedbackVolume;
				}
			} else {
				bufferWrite += toMixSamples;
			}

			remainingLength -= toMixSamples;
		}
	} else {
		noise.clocks = 0;
	}
}