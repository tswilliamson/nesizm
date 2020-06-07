
// Main NES APU implementation

#include "platform.h"
#include "debug.h"
#include "nes.h"	
#include "settings.h"

#include "scope_timer/scope_timer.h"
#include "snd/snd.h"

nes_apu nesAPU;
bool bSoundEnabled = false;

#if DEBUG
// debug muting of various mixers
static bool bEnabled_pulse1 = true;
static bool bEnabled_pulse2 = true;
static bool bEnabled_tri = true;
static bool bEnabled_noise = true;
static bool bEnabled_dmc = true;
#define CHECK_ENABLED(mixer) if (bEnabled_##mixer == false) mixer##Volume = 0;
#else
#define CHECK_ENABLED(mixer) 
#endif

const unsigned int palFrameCounterClocks = 8313;

// how much of a duty cycle sample from above to move through per sample, divided by 256
int duty_delta(int t) {
#if TARGET_PRIZM
	if (nesCart.isPAL) {
		const int soundNumerator = int(8192.0f / SOUND_RATE * 1662607);
		return soundNumerator / (t + 1);
	} else {
		const int soundNumerator = int(8192.0f / SOUND_RATE * 1789773);
		return soundNumerator / (t + 1);
	}
#else
	if (nesCart.isPAL) {
		const int soundNumerator = int(4096.0f / SOUND_RATE * 1662607);
		return soundNumerator / (t + 1);
	} else {
		const int soundNumerator = int(4096.0f / SOUND_RATE * 1789773);
		return soundNumerator / (t + 1);
	}
#endif
}

static uint8 length_counter_table[32] = {
	10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU PULSE

static const int pulse_duty[4][16] = {
	{ 0, 1, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // 12.5% duty
	{ 0, 1, 4, 4, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // 25% duty
	{ 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 1, 0, 0, 0, 0, 0},   // 50% duty
	{ 4, 4, 1, 0, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}    // -25% duty
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
#if TARGET_PRIZM
			uint8 duty : 2;
			uint8 loop_or_halt : 1;
			uint8 constant_volume : 1;
			uint8 volume : 4;
#else
			uint8 volume : 4;
			uint8 constant_volume : 1;
			uint8 loop_or_halt : 1;
			uint8 duty : 2;
#endif
		};
		struct {
#if TARGET_PRIZM
			uint8 sweep_enable : 1;
			uint8 sweep_period : 3;
			uint8 sweep_negate : 1;
			uint8 sweep_shift : 3;
#else
			uint8 sweep_shift : 3;
			uint8 sweep_negate : 1;
			uint8 sweep_period : 3;
			uint8 sweep_enable : 1;
#endif
		};
		struct {
			uint8 timer_low : 8;
		};
		struct {
#if TARGET_PRIZM
			uint8 length_load : 5;
			uint8 timer_hi : 3;
#else
			uint8 timer_hi : 3;
			uint8 length_load : 5;
#endif
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

	if (sweepTwosComplement) {
		if (lengthCounter) {
			mainCPU.specialMemory[0x15] |= 0x02;
		} else {
			mainCPU.specialMemory[0x15] &= ~0x02;
		}
	} else {
		if (lengthCounter) {
			mainCPU.specialMemory[0x15] |= 0x01;
		} else {
			mainCPU.specialMemory[0x15] &= ~0x01;
		}
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
#if TARGET_PRIZM
			uint8 enable_counter : 1;
			uint8 linear_counter : 7;
#else
			uint8 linear_counter : 7;
			uint8 enable_counter : 1;
#endif
		};
		struct {
			uint8 timer_low : 8;
		};
		struct {
#if TARGET_PRIZM
			uint8 length_load : 5;
			uint8 timer_hi : 3;
#else
			uint8 timer_hi : 3;
			uint8 length_load : 5;
#endif
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

	if (lengthCounter) {
		mainCPU.specialMemory[0x15] |= 0x04;
	} else {
		mainCPU.specialMemory[0x15] &= ~0x04;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU NOISE

// number of samples between noise shift register switches (x16, clamped at 8)
inline int noise_samples(int noisePeriod) {
#if TARGET_PRIZM
	int samples = noisePeriod * (SOUND_RATE) / 111861;
#else
	int samples = noisePeriod * (SOUND_RATE * 2) / 111861;
#endif

	if (samples < 8) return 8;
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
#if TARGET_PRIZM
			uint8 _unused0 : 2;
			uint8 loop_or_halt : 1;
			uint8 constant_volume : 1;
			uint8 volume : 4;
#else
			uint8 volume : 4;
			uint8 constant_volume : 1;
			uint8 loop_or_halt : 1;
			uint8 _unused0 : 2;
#endif
		};
		struct {
#if TARGET_PRIZM
			uint8 noise_mode : 1;
			uint8 _unused2 : 3;
			uint8 noise_period : 4;
#else
			uint8 noise_period : 4;
			uint8 _unused2 : 3;
			uint8 noise_mode : 1;
#endif
		};
		struct {
#if TARGET_PRIZM
			uint8 length_load : 5;
			uint8 _unused3 : 3;
#else
			uint8 _unused3 : 3;
			uint8 length_load : 5;
#endif
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

	if (lengthCounter) {
		mainCPU.specialMemory[0x15] |= 0x08;
	} else {
		mainCPU.specialMemory[0x15] &= ~0x08;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU DMC

const int dmc_pitch_ntsc[16] = {
	428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

const int dmc_pitch_pal[16] = {
	398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50
};

void nes_apu_dmc::writeReg(unsigned int regNum, uint8 value) {
	union regHelper {
		struct {
#if TARGET_PRIZM
			uint8 irq : 1;
			uint8 loop : 1;
			uint8 _unused0 : 2;
			uint8 frequency : 4;
#else
			uint8 frequency : 4;
			uint8 _unused0 : 2;
			uint8 loop : 1;
			uint8 irq : 1;
#endif
		};
		struct {
#if TARGET_PRIZM
			uint8 _unused1 : 1;
			uint8 level_load : 7;
#else
			uint8 level_load : 7;
			uint8 _unused1 : 1;
#endif
		};
		uint8 value;
	};
	regHelper helper;
	helper.value = value;

	int dmcPeriod;
	switch (regNum) {
		case 0:
			if (helper.irq) {
				irqEnabled = true;
			} else {
				irqEnabled = false;
				nesAPU.clearDMCIRQ();
				mainCPU.ackIRQ(2);
			}
			loop = helper.loop != 0;

			if (nesCart.isPAL) {
				dmcPeriod = dmc_pitch_ntsc[helper.frequency];
			} else {
				dmcPeriod = dmc_pitch_pal[helper.frequency];
			}
			samplesPerPeriod = noise_samples(dmcPeriod);
			break;
		case 1:
			output = helper.level_load;
			break;
		case 2:
			sampleAddress = 0xC000 | (helper.value << 6);
			break;
		case 3:
			length = (helper.value * 16) + 1;
			break;
	}
}

void nes_apu_dmc::bitClear() {
	remainingLength = 0;
	nesAPU.clearDMCIRQ();
}

void nes_apu_dmc::bitSet() {
	if (remainingLength < 8) {
		remainingLength += length;
		curSampleAddress = sampleAddress;
	}
	nesAPU.clearDMCIRQ();
}

void nes_apu_dmc::step() {
	if (bitCount == 0) {
		if (remainingLength) {
			// read the next sample off the CPU
			sampleBuffer = mainCPU.readNonIO(curSampleAddress++);
			if (curSampleAddress == 0x10000) curSampleAddress = 0x8000;
			// mainCPU.clocks += 3;

			remainingLength--;
			
			if (remainingLength == 0) {
				if (loop) {
					remainingLength += length;
					curSampleAddress = sampleAddress;
				}
				if (irqEnabled) {
					mainCPU.specialMemory[0x15] |= 0x80;
					mainCPU.setIRQ(2, 0); 
				}
			}

			silentFlag = false;
		} else {
			silentFlag = true;
		}

		if (remainingLength) {
			mainCPU.specialMemory[0x15] |= 0x10;
		} else {
			mainCPU.specialMemory[0x15] &= ~0x10;
		}
		
		bitCount = 8;
	}
	
	if (silentFlag == false) {
		if (sampleBuffer & 1) {
			if (output <= 125) output += 2;
		} else {
			if (output >= 2) output -= 2;
		}

		sampleBuffer = sampleBuffer >> 1;
	}

	bitCount--;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU

void sndFrame(int* buffer, int length) {
	TIME_SCOPE();
	
	nesAPU.mix(buffer, length);
}

void nes_apu::init() {
	memset(this, 0, sizeof(nes_apu));
	pulse2.sweepTwosComplement = true;
	noise.shiftRegister = 1;
	mainCPU.specialMemory[0x15] = 0;
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
		dmc.writeReg(address & 0x3, value);
	} else if (address == 0x15) {
		// channel flags : DNT21
		if ((value & 1) == 0) pulse1.lengthCounter = 0;
		if ((value & 2) == 0) pulse2.lengthCounter = 0;
		if ((value & 4) == 0) triangle.lengthCounter = 0;
		if ((value & 8) == 0) noise.lengthCounter = 0;

		// dmc is special
		if ((value & 16) == 0)
			dmc.bitClear();
		else
			dmc.bitSet();

	} else if (address == 0x17) {
		// frame counter
		if (value & 0x80) {
			// clocks the counters if mode is set
			step_quarter();
			step_half();
		}

		// irq inhibit
		inhibitIRQ = (value & 0x40) != 0;
		if (inhibitIRQ) {
			clearFrameIRQ();
		}

		// reset step counter
		mainCPU.apuClocks = mainCPU.clocks + 7457;
		cycle = 0;
	}
}

void nes_apu::clearFrameIRQ() {
	mainCPU.specialMemory[0x15] &= ~0x40;
	mainCPU.ackIRQ(1);
}

void nes_apu::clearDMCIRQ() {
	mainCPU.specialMemory[0x15] &= ~0x80;
	mainCPU.ackIRQ(2);
}

bool nes_apu::IRQReached(int irqBit) {
	return true;
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

				// do an irq?
				if (inhibitIRQ == false) {
					mainCPU.specialMemory[0x15] |= 0x40;
					mainCPU.setIRQ(1, mainCPU.apuClocks);
				}

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
	bool bHighQuality = nesSettings.GetSetting(ST_SoundQuality) != 0;

	int triVolume = 237;
	CHECK_ENABLED(tri);
	if (triangle.linearCounter == 0 || triangle.lengthCounter == 0 || triangle.rawPeriod < 2)
		triVolume = 0;

	if (triVolume) {
		int delta = duty_delta(triangle.rawPeriod * 2);

		for (int32 i = 0; i < length; i++) {
			triangle.mixOffset = (triangle.mixOffset + delta) & 0xFFFF;
			intoBuffer[i] = tri_duty[(triangle.mixOffset >> 11)] * triVolume;
		}
	} else {
		for (int32 i = 0; i < length; i++) {
			intoBuffer[i] = 0;
		}

		triangle.mixOffset = (triangle.mixOffset + duty_delta(triangle.rawPeriod) * length) & 0xFFFF;
	}

	int noiseVolume = 138 * (noise.useConstantVolume ? noise.constantVolume : noise.envelopeVolume);
	if (noise.lengthCounter == 0)
		noiseVolume = 0;

	CHECK_ENABLED(noise);
	if (noiseVolume && noise.samplesPerPeriod) {
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
					*bufferWrite += curFeedbackVolume;
					++bufferWrite;
				}
			} else {
				bufferWrite += toMixSamples;
			}

			remainingLength -= toMixSamples;
		}
	} else {
		noise.clocks = 0;
	}

	// dmc
	if (dmc.samplesPerPeriod) {
		int remainingLength = length;
		int* bufferWrite = intoBuffer;

		while (remainingLength > 0) {
			int dmcVolume = dmc.output * 96;
			CHECK_ENABLED(dmc);

			int toMixClocks = dmc.samplesPerPeriod - dmc.clocks;
			int toMixSamples = toMixClocks >> 4;
			if (remainingLength < toMixSamples) {
				toMixSamples = remainingLength;
				dmc.clocks = (toMixSamples << 4);
			} else {
				dmc.step();
				dmc.clocks = (toMixSamples << 4) - toMixClocks;
			}

			if (dmcVolume) {
				if (bHighQuality) {
					int dampenFactor = 256 - dmc.output * 160 / 256;
					for (int i = 0; i < toMixSamples; i++) {
						*bufferWrite = *bufferWrite * dampenFactor / 256 + dmcVolume;
						// if DMC is being applied, then it's possible the total volume will clip, so keep it from wrapping
						if (*bufferWrite > 16383) *bufferWrite = 16383;
						++bufferWrite;
					}
				} else {
					for (int i = 0; i < toMixSamples; i++) {
						*bufferWrite += dmcVolume;
						// if DMC is being applied, then it's possible the total volume will clip, so keep it from wrapping
						if (*bufferWrite > 16383) *bufferWrite = 16383;
						++bufferWrite;
					}
				}
			} else {
				bufferWrite += toMixSamples;
			}

			remainingLength -= toMixSamples;
		}
	}
	
	int pulse1Volume = 210 * (pulse1.useConstantVolume ? pulse1.constantVolume : pulse1.envelopeVolume) / 4;
	CHECK_ENABLED(pulse1);

	if (pulse1.sweepTargetPeriod > 0x7FF || pulse1.lengthCounter == 0 || pulse1.rawPeriod < 8)
		pulse1Volume = 0;

	if (pulse1Volume) {
		int pulse1_delta = duty_delta(pulse1.rawPeriod);
		const int* pulse1_duty = pulse_duty[pulse1.dutyCycle];

		for (int32 i = 0; i < length; i++) {
			pulse1.mixOffset = (pulse1.mixOffset + pulse1_delta) & 0xFFFF;
			intoBuffer[i] += pulse1_duty[(pulse1.mixOffset >> 12)] * pulse1Volume;
		}
	}
	
	int pulse2Volume = 210 * (pulse2.useConstantVolume ? pulse2.constantVolume : pulse2.envelopeVolume) / 4;
	CHECK_ENABLED(pulse2);

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


	if (bHighQuality) {
		// remove anu sudden spikes/dips
		int32 maxSpike = 8192;
		for (int32 i = 1; i < length - 1; i++) {
			if (intoBuffer[i] - intoBuffer[i - 1] > maxSpike &&
				intoBuffer[i] - intoBuffer[i + 1] > maxSpike) {
				intoBuffer[i] = (intoBuffer[i - 1] + intoBuffer[i + 1]) / 2;
			} 
			else if (intoBuffer[i-1] - intoBuffer[i] > maxSpike &&
				intoBuffer[i+1] - intoBuffer[i] > maxSpike) {
				intoBuffer[i] = (intoBuffer[i - 1] + intoBuffer[i + 1]) / 2;
			}
		}
		
		// low pass filter
		int lastSample = intoBuffer[0];
		for (int32 i = 1; i < length; i++) {
			intoBuffer[i] = lastSample * 16 / 128 + intoBuffer[i] * 112 / 128;
			lastSample = intoBuffer[i];
		}
	}
}