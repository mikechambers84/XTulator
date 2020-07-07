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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "sdlaudio.h"
#include "pcspeaker.h"
#include "opl2.h"
#include "blaster.h"
#include "../../timing.h"
#include "../../utility.h"
#include "../../debuglog.h"

int16_t sdlaudio_buf[2][SAMPLE_BUFFER], sdlaudio_curbuf = 1, sdlaudio_curpos[2] = { 0, 0 };

uint8_t sdlaudio_firstfill = 1, sdlaudio_timeIdx = 0;
SDL_AudioSpec sdlaudio_gotspec;
uint32_t sdlaudio_timer;
uint64_t sdlaudio_cbTime[10] = { //history of time between callbacks to dynamically adjust our sample generation
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
double sdlaudio_genSampRate = SAMPLE_RATE;
double sdlaudio_genInterval;

extern OPL2_t OPL2;
extern BLASTER_t blaster;

void sdlaudio_fill(void* udata, uint8_t* stream, int len) {
	//double resamp = ((double)sdlaudio_curpos[sdlaudio_curbuf ^ 1] * 2) / (double)len;
	//int i;
	static uint8_t timesmissed = 0;
	static uint64_t curtime, lasttime;

	if (sdlaudio_timeIdx < 10) {
		curtime = timing_getCur();
		if (!sdlaudio_firstfill) {
			sdlaudio_cbTime[sdlaudio_timeIdx++] = curtime - lasttime;
			if (sdlaudio_timeIdx == 10) {
				uint8_t i;
				uint64_t avg = 0;
				for (i = 0; i < 10; i++) {
					avg += sdlaudio_cbTime[i];
				}
				avg /= SAMPLE_BUFFER;
				avg /= 10;
				sdlaudio_genInterval = (double)avg;
				timing_updateInterval(sdlaudio_timer, (uint64_t)sdlaudio_genInterval);
			}
		}
		lasttime = timing_getCur();
		sdlaudio_firstfill = 0;
	}

	memset(stream, 0, len);
	memcpy(stream, sdlaudio_buf[sdlaudio_curbuf ^ 1], len);
	//debug_log(DEBUG_INFO, "%d       \r\n", sdlaudio_curpos[sdlaudio_curbuf ^ 1]);

	sdlaudio_curpos[sdlaudio_curbuf ^ 1] = 0;
	sdlaudio_curbuf ^= 1;
	//debug_log(DEBUG_DETAIL, "Resample: %d, %fx\r\n", len, resamp);
}

int sdlaudio_init() {
	SDL_AudioSpec wanted;

	if (SDL_Init(SDL_INIT_AUDIO)) return -1;

	wanted.freq = SAMPLE_RATE;
	wanted.format = AUDIO_S16;
	wanted.channels = 1;
	wanted.samples = SAMPLE_BUFFER;
	wanted.callback = sdlaudio_fill;
	wanted.userdata = NULL;

	if (SDL_OpenAudio(&wanted, NULL) < 0) {
		return -1;
	}

	sdlaudio_timer = timing_addTimer(sdlaudio_generateSample, NULL, SAMPLE_RATE, TIMING_ENABLED);

	SDL_PauseAudio(1);

	return 0;
}

void sdlaudio_generateSample(void* dummy) {
	int16_t val;

	if (sdlaudio_curpos[sdlaudio_curbuf] == SAMPLE_BUFFER) {
		SDL_PauseAudio(0);
	} else {
		val = pcspeaker_getSample() / 3;
		val += opl2_generateSample(&OPL2) / 3;
		val += blaster_getSample(&blaster) / 3;
		sdlaudio_buf[sdlaudio_curbuf][sdlaudio_curpos[sdlaudio_curbuf]++] = val;
	}
}
