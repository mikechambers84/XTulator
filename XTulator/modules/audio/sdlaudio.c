/*
  XTulator: A portable, open-source 80186 PC emulator.
  Copyright (C)2020 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
	NOTE: Maybe implement some kind of crude time-stretching?
*/

#include "../../config.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "sdlaudio.h"
#include "pcspeaker.h"
#include "opl2.h"
#include "blaster.h"
#include "../../machine.h"
#include "../../timing.h"
#include "../../utility.h"
#include "../../debuglog.h"
#ifdef _WIN32
#include <Windows.h>
#include <SDL/SDL.h>
#include <process.h>
#else
#include <sys/time.h>
#include <SDL.h>
#include <pthread.h>
pthread_t sdlaudio_sampleThreadID;
#endif

SDL_mutex* sdlaudio_mutex = NULL;
SDL_cond* sdlaudio_canFill = NULL;
SDL_cond* sdlaudio_canBuffer = NULL;

int16_t sdlaudio_buffer[SAMPLE_BUFFER], sdlaudio_bufferpos = 0;
double sdlaudio_rateFast;

uint8_t sdlaudio_firstfill = 1, sdlaudio_timeIdx = 0;
SDL_AudioSpec sdlaudio_gotspec;
uint32_t sdlaudio_timer;
uint64_t sdlaudio_cbTime[10] = { //history of time between callbacks to dynamically adjust our sample generation
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
double sdlaudio_genSampRate = SAMPLE_RATE;
double sdlaudio_genInterval;

volatile uint8_t sdlaudio_updateTiming = 0;

MACHINE_t* sdlaudio_useMachine = NULL;

void sdlaudio_moveBuffer(int16_t* dst, int len);

void sdlaudio_fill(void* udata, uint8_t* stream, int len) {
	static uint8_t timesmissed = 0;
	static uint64_t curtime, lasttime;

	//SDL_LockMutex(sdlaudio_mutex);
	//SDL_CondWait(sdlaudio_canFill, sdlaudio_mutex);

	sdlaudio_moveBuffer((int16_t*)stream, len);

	//SDL_UnlockMutex(sdlaudio_mutex);
	//SDL_CondSignal(sdlaudio_canBuffer);
}

int sdlaudio_init(MACHINE_t* machine) {
	SDL_AudioSpec wanted;

	if (machine == NULL) return -1;

	if (SDL_Init(SDL_INIT_AUDIO)) return -1;

	sdlaudio_mutex = SDL_CreateMutex();
	sdlaudio_canFill = SDL_CreateCond();
	sdlaudio_canBuffer = SDL_CreateCond();

	wanted.freq = SAMPLE_RATE;
	wanted.format = AUDIO_S16;
	wanted.channels = 1;
	wanted.samples = SAMPLE_BUFFER >> 2;
	wanted.callback = sdlaudio_fill;
	wanted.userdata = NULL;

	if (SDL_OpenAudio(&wanted, NULL) < 0) {
		return -1;
	}

	sdlaudio_useMachine = machine;

	sdlaudio_rateFast = (double)(SAMPLE_RATE) * 1.01;

	sdlaudio_timer = timing_addTimer(sdlaudio_generateSample, NULL, SAMPLE_RATE, TIMING_ENABLED);

	SDL_PauseAudio(1);
	SDL_CondSignal(sdlaudio_canFill);
	SDL_CondSignal(sdlaudio_canBuffer);

	return 0;
}

void sdlaudio_bufferSample(int16_t val) {
	if (sdlaudio_bufferpos == SAMPLE_BUFFER) { //this shouldn't happen
		return;
	}

	//SDL_LockMutex(sdlaudio_mutex);
	//SDL_CondWait(sdlaudio_canBuffer, sdlaudio_mutex);

	sdlaudio_buffer[sdlaudio_bufferpos++] = val;

	if (sdlaudio_bufferpos < (int)((double)(SAMPLE_BUFFER) * 0.5)) {
		sdlaudio_updateTiming = SDLAUDIO_TIMING_FAST;
	}
	else if (sdlaudio_bufferpos >= (int)((double)(SAMPLE_BUFFER) * 0.75)) {
		sdlaudio_updateTiming = SDLAUDIO_TIMING_NORMAL;
	}

	if (sdlaudio_bufferpos == SAMPLE_BUFFER) {
		timing_timerDisable(sdlaudio_timer);
	}

	//SDL_UnlockMutex(sdlaudio_mutex);
	//SDL_CondSignal(sdlaudio_canFill);
}

void sdlaudio_updateSampleTiming() {
	if (sdlaudio_updateTiming == SDLAUDIO_TIMING_FAST) {
		timing_updateIntervalFreq(sdlaudio_timer, sdlaudio_rateFast);
	}
	else if (sdlaudio_updateTiming == SDLAUDIO_TIMING_NORMAL) {
		timing_updateIntervalFreq(sdlaudio_timer, SAMPLE_RATE);
		SDL_PauseAudio(0);
	}
	sdlaudio_updateTiming = 0;
}

//I need to make this use a ring buffer soon...
void sdlaudio_moveBuffer(int16_t* dst, int len) {
	int i;
	memset(dst, 0, len);

	if (sdlaudio_bufferpos < (int)((double)(SAMPLE_BUFFER) * 0.75)) {
		timing_timerEnable(sdlaudio_timer);
	}

	if ((sdlaudio_bufferpos << 1) < len) {
		SDL_PauseAudio(1);
		return;
	}

	for (i = 0; i < (len >> 1); i++) {
		dst[i] = sdlaudio_buffer[i];
	}
	for (; i < sdlaudio_bufferpos; i++) {
		sdlaudio_buffer[i - (len >> 1)] = sdlaudio_buffer[i];
	}
	sdlaudio_bufferpos -= len >> 1;
}

void sdlaudio_generateSample(void* dummy) {
	int16_t val;

	val = pcspeaker_getSample(&sdlaudio_useMachine->pcspeaker) / 3;
	//val += opl2_generateSample(&sdlaudio_useMachine->OPL2) / 3;
	if (sdlaudio_useMachine->mixOPL) {
		int16_t OPLsample[2];
		//val += sdlaudio_getOPLsample() / 2;
		OPL3_GenerateStream(&sdlaudio_useMachine->OPL3, OPLsample, 1);
		val += OPLsample[0] / 2;
	}
	if (sdlaudio_useMachine->mixBlaster) {
		val += blaster_getSample(&sdlaudio_useMachine->blaster) / 3;
	}

	sdlaudio_bufferSample(val);
}
