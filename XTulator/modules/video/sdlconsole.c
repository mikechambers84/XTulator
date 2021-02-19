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

#include "../../config.h"

#ifdef _WIN32
#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#include <Windows.h>
#else
#include <SDL2/SDL.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include "sdlconsole.h"
#include "../input/sdlkeys.h"
#include "../input/mouse.h"
#include "../../timing.h"
#include "../../menus.h"

SDL_Window *sdlconsole_window = NULL;
SDL_Renderer *sdlconsole_renderer = NULL;
SDL_Texture *sdlconsole_texture = NULL;

uint64_t sdlconsole_frameTime[30];
uint32_t sdlconsole_keyTimer;
uint8_t sdlconsole_curkey, sdlconsole_lastKey, sdlconsole_frameIdx = 0, sdlconsole_grabbed = 0, sdlconsole_ctrl = 0, sdlconsole_alt = 0, sdlconsole_doRepeat = 0;
int sdlconsole_curw, sdlconsole_curh;

char* sdlconsole_title;

void sdlconsole_keyRepeat(void* dummy) {
	sdlconsole_doRepeat = 1;
	timing_updateIntervalFreq(sdlconsole_keyTimer, 15);
}

int sdlconsole_init(char *title) {
#ifdef _WIN32
	HWND hwnd;
	SDL_SysWMinfo wmInfo;
#endif

	if (SDL_Init(SDL_INIT_VIDEO)) return -1;

	sdlconsole_title = title;

	sdlconsole_window = SDL_CreateWindow(sdlconsole_title,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		640, 400,
		SDL_WINDOW_OPENGL);
	if (sdlconsole_window == NULL) return -1;

	if (sdlconsole_setWindow(640, 400)) {
		return -1;
	}

	sdlconsole_keyTimer = timing_addTimer(sdlconsole_keyRepeat, NULL, 2, TIMING_DISABLED);

#ifdef _WIN32
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(sdlconsole_window, &wmInfo);
	hwnd = wmInfo.info.win.window;
	menus_init(hwnd);
#endif

	return 0;
}

int sdlconsole_setWindow(int w, int h) {
	if (sdlconsole_renderer != NULL) SDL_DestroyRenderer(sdlconsole_renderer);
	if (sdlconsole_texture != NULL) SDL_DestroyTexture(sdlconsole_texture);
	sdlconsole_renderer = NULL;
	sdlconsole_texture = NULL;

	SDL_SetWindowSize(sdlconsole_window, w, h);

	sdlconsole_renderer = SDL_CreateRenderer(sdlconsole_window, -1, 0);
	if (sdlconsole_renderer == NULL) return -1;

	sdlconsole_texture = SDL_CreateTexture(sdlconsole_renderer,
		SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		w, h);
	if (sdlconsole_texture == NULL) return -1;

	sdlconsole_curw = w;
	sdlconsole_curh = h;

	return 0;
}

void sdlconsole_setTitle(char* title) { //appends something to the main title, doesn't replace it all
	char tmp[1024];
	sprintf(tmp, "%s - %s", sdlconsole_title, title);
	SDL_SetWindowTitle(sdlconsole_window, tmp);
}

void sdlconsole_blit(uint32_t *pixels, int w, int h, int stride) {
	static uint64_t lasttime = 0;
	uint64_t curtime;
	curtime = timing_getCur();

	if ((w != sdlconsole_curw) || (h != sdlconsole_curh)) {
		sdlconsole_setWindow(w, h);
	}
	SDL_UpdateTexture(sdlconsole_texture, NULL, pixels, stride);
	SDL_RenderClear(sdlconsole_renderer);
	SDL_RenderCopy(sdlconsole_renderer, sdlconsole_texture, NULL, NULL);
	SDL_RenderPresent(sdlconsole_renderer);

	if (lasttime != 0) {
		int i, avgcount;
		uint64_t curavg;
		char tmp[64];
		sdlconsole_frameTime[sdlconsole_frameIdx++] = curtime - lasttime;
		if (sdlconsole_frameIdx == 30) {
			sdlconsole_frameIdx = 0;
			avgcount = 0;
			curavg = 0;
			for (i = 0; i < 30; i++) {
				if (sdlconsole_frameTime[i] != 0) {
					curavg += sdlconsole_frameTime[i];
					avgcount++;
				}
			}
			curavg /= avgcount;
			sprintf(tmp, "%.2f FPS", (double)((timing_getFreq() * 10) / curavg) / 10);
			sdlconsole_setTitle(tmp);
		}
	}
	lasttime = curtime;
}

void sdlconsole_mousegrab() {
	sdlconsole_ctrl = sdlconsole_alt = 0;
	if (sdlconsole_grabbed) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		sdlconsole_grabbed = 0;
	} else {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		sdlconsole_grabbed = 1;
	}
}

int sdlconsole_loop() {
	SDL_Event event;
	int8_t xrel, yrel;
	uint8_t action = 0;

	if (sdlconsole_doRepeat) {
		sdlconsole_doRepeat = 0;
		sdlconsole_curkey = sdlconsole_lastKey;
		return SDLCONSOLE_EVENT_KEY;
	}

	if (!SDL_PollEvent(&event)) return SDLCONSOLE_EVENT_NONE;
	switch (event.type) {
		case SDL_KEYDOWN:
#ifdef DEBUG_VGA
			if (event.key.keysym.sym == SDLK_F12) {
				vga_dumpregs();
			}
#endif
			if (event.key.repeat) return SDLCONSOLE_EVENT_NONE;
			switch (event.key.keysym.sym) {
			case SDLK_F11:
				return SDLCONSOLE_EVENT_DEBUG_1;
			case SDLK_F12:
				return SDLCONSOLE_EVENT_DEBUG_2;
			default:
				if (event.key.keysym.sym == SDLK_LCTRL) sdlconsole_ctrl = 1;
				if (event.key.keysym.sym == SDLK_LALT) sdlconsole_alt = 1;
				if (sdlconsole_ctrl & sdlconsole_alt) {
					sdlconsole_mousegrab();
				}
				sdlconsole_curkey = sdlconsole_translateScancode(event.key.keysym.sym);
				if (sdlconsole_curkey == 0x00) {
					return SDLCONSOLE_EVENT_NONE;
				} else {
					sdlconsole_lastKey = sdlconsole_curkey;
					timing_updateIntervalFreq(sdlconsole_keyTimer, 2);
					timing_timerEnable(sdlconsole_keyTimer);
					return SDLCONSOLE_EVENT_KEY;
				}
			}
		case SDL_KEYUP:
			if (event.key.repeat) return SDLCONSOLE_EVENT_NONE;
			if (event.key.keysym.sym == SDLK_LCTRL) sdlconsole_ctrl = 0;
			if (event.key.keysym.sym == SDLK_LALT) sdlconsole_alt = 0;
			sdlconsole_curkey = sdlconsole_translateScancode(event.key.keysym.sym) | 0x80;
			if ((sdlconsole_curkey & 0x7F) == sdlconsole_lastKey) {
				timing_timerDisable(sdlconsole_keyTimer);
			}
			return (sdlconsole_curkey == 0x80) ? SDLCONSOLE_EVENT_NONE : SDLCONSOLE_EVENT_KEY;
		case SDL_MOUSEMOTION:
			xrel = (event.motion.xrel < -128) ? -128 : (int8_t)event.motion.xrel;
			xrel = (event.motion.xrel > 127) ? 127 : (int8_t)event.motion.xrel;
			yrel = (event.motion.yrel < -128) ? -128 : (int8_t)event.motion.yrel;
			yrel = (event.motion.yrel > 127) ? 127 : (int8_t)event.motion.yrel;
			if (sdlconsole_grabbed) {
				mouse_action(MOUSE_ACTION_MOVE, MOUSE_NEITHER, xrel, yrel);
			}
			return SDLCONSOLE_EVENT_NONE;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (event.button.button == SDL_BUTTON_LEFT) {
				action = MOUSE_ACTION_LEFT;
				if (!sdlconsole_grabbed) {
					sdlconsole_mousegrab();
					break;
				}
			}
			else if (event.button.button == SDL_BUTTON_RIGHT) {
				action = MOUSE_ACTION_RIGHT;
			}
			if (sdlconsole_grabbed) {
				mouse_action(action, (event.button.state == SDL_PRESSED) ? MOUSE_PRESSED : MOUSE_UNPRESSED, 0, 0);
			}
			return SDLCONSOLE_EVENT_NONE;
		case SDL_QUIT:
			return SDLCONSOLE_EVENT_QUIT;
	}
	return SDLCONSOLE_EVENT_NONE;
}

uint8_t sdlconsole_getScancode() {
	//debug_log(DEBUG_DETAIL, "curkey: %02X\r\n", sdlconsole_curkey);
	return sdlconsole_curkey;
}

uint8_t sdlconsole_translateScancode(SDL_Keycode keyval) {
	uint8_t i;
	for (i = 0; i < 95; i++) {
		if (keyval == (SDL_Keycode)sdlconsole_translateMatrix[i][0]) {
			return (uint8_t)sdlconsole_translateMatrix[i][1];
		}
	}
	return 0x00;
}
