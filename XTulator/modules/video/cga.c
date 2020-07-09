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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
pthread_t cga_renderThreadID;
#endif
#include "cga.h"
#include "../../config.h"
#include "../../timing.h"
#include "../../utility.h"
#include "../../ports.h"
#include "../../memory.h"
#include "sdlconsole.h"
#include "../../debuglog.h"

const uint8_t cga_palette[16][3] = { //R, G, B
	{ 0x00, 0x00, 0x00 }, //black
	{ 0x00, 0x00, 0xAA }, //blue
	{ 0x00, 0xAA, 0x00 }, //green
	{ 0x00, 0xAA, 0xAA }, //cyan
	{ 0xAA, 0x00, 0x00 }, //red
	{ 0xAA, 0x00, 0xAA }, //magenta
	{ 0xAA, 0x55, 0x00 }, //brown
	{ 0xAA, 0xAA, 0xAA }, //light gray
	{ 0x55, 0x55, 0x55 }, //dark gray
	{ 0x55, 0x55, 0xFF }, //light blue
	{ 0x55, 0xFF, 0x55 }, //light green
	{ 0x55, 0xFF, 0xFF }, //light cyan
	{ 0xFF, 0x55, 0x55 }, //light red
	{ 0xFF, 0x55, 0xFF }, //light magenta
	{ 0xFF, 0xFF, 0x55 }, //yellow
	{ 0xFF, 0xFF, 0xFF }  //white
};

const uint8_t cga_gfxpal[2][2][4] = { //palettes for 320x200 graphics mode
	{
		{ 0, 2, 4, 6 }, //normal palettes
		{ 0, 3, 5, 7 }
	},
	{
		{ 0, 10, 12, 14 }, //intense palettes
		{ 0, 11, 13, 15 }
	}
};

uint8_t cga_font[4096];
uint32_t cga_framebuffer[400][640];
uint16_t cga_cursorloc = 0;
uint8_t cga_indexreg = 0, cga_datareg[256], cga_regs[16];
uint8_t cga_cursor_blink_state = 0;
uint8_t *cga_RAM = NULL;

volatile uint8_t cga_doDraw = 1;

int cga_init() {
	int x, y;

	debug_log(DEBUG_INFO, "[CGA] Initializing CGA video device\r\n");

	if (utility_loadFile(cga_font, 4096, "roms/video/cgachar.bin")) {
		debug_log(DEBUG_ERROR, "[CGA] Failed to load character generator ROM\r\n");
		return -1;
	}

	for (y = 0; y < 400; y++) {
		for (x = 0; x < 640; x++) {
			cga_framebuffer[y][x] = cga_color(CGA_BLACK);
		}
	}
	sdlconsole_blit((uint32_t *)cga_framebuffer, 640, 400, 640 * sizeof(uint32_t));

	timing_addTimer(cga_blinkCallback, NULL, 3, TIMING_ENABLED);
	timing_addTimer(cga_scanlineCallback, NULL, 62800, TIMING_ENABLED);
	timing_addTimer(cga_drawCallback, NULL, 60, TIMING_ENABLED);
	/*
		NOTE: CGA scanlines are clocked at 15.7 KHz. We are breaking each scanline into
		four parts and using the last part as a very approximate horizontal retrace period.
		
		15700 x 4 = 62800

		See cga_scanlineCallback function for more details.
	*/

	cga_RAM = (uint8_t*)malloc(16384);
	if (cga_RAM == NULL) {
		debug_log(DEBUG_ERROR, "[CGA] Failed to allocate video memory\r\n");
		return -1;
	}

	//TODO: error checking below
#ifdef _WIN32
	_beginthread(cga_renderThread, 0, NULL);
#else
	pthread_create(&cga_renderThreadID, NULL, cga_renderThread, NULL);
#endif

	ports_cbRegister(0x3D0, 16, (void*)cga_readport, NULL, (void*)cga_writeport, NULL, NULL);
	memory_mapCallbackRegister(0xB8000, 0x4000, (void*)cga_readmemory, (void*)cga_writememory, NULL);

	return 0;
}

void cga_update(uint32_t start_x, uint32_t start_y, uint32_t end_x, uint32_t end_y) {
	uint32_t addr, startaddr, cursorloc, cursor_x, cursor_y;
	uint32_t scx, scy, x, y;
	uint8_t cc, attr, fontdata, blink, mode, colorset, intensity, blinkenable;

	if (cga_regs[0x8] & 0x02) { //graphics modes
		mode = (cga_regs[0x8] & 0x10) ? CGA_MODE_GRAPHICS_HI : CGA_MODE_GRAPHICS_LO;
		intensity = (cga_regs[0x9] & 0x10) ? 1 : 0;
		colorset = (cga_regs[0x9] & 0x20) ? 1 : 0;
	} else { //text modes
		mode = (cga_regs[0x8] & 0x01) ? CGA_MODE_TEXT_80X25 : CGA_MODE_TEXT_40X25;
		blinkenable = (cga_regs[0x8] & 0x20) ? 1 : 0;
	}
	startaddr = (((uint32_t)cga_datareg[0x12] & 0x3F) << 8) | (uint32_t)cga_datareg[0x13];
	cursorloc = (((uint32_t)cga_datareg[0xE] << 8) & 0xFF00) | (uint32_t)cga_datareg[0xF];

	switch (mode) {
	case CGA_MODE_TEXT_80X25:
		cursor_x = cursorloc % 80;
		cursor_y = cursorloc / 80;
		for (scy = start_y; scy <= end_y; scy++) {
			y = scy / (((cga_datareg[0x09] & 0x1F) + 1) * 2);
			for (scx = start_x; scx <= end_x; scx++) {
				x = scx / 8;
				addr = startaddr + ((y * 80) + x) * 2;
				cc = cga_RAM[addr];
				attr = cga_RAM[addr + 1];
				blink = attr >> 7;
				if (blinkenable) attr &= 0x7F; //enabling text mode blink attribute limits background color selection
				fontdata = cga_font[2048 + (cc * 8) + ((scy % 16) / 2)];
				fontdata = (fontdata >> (7 - (scx % 8))) & 1;
				if ((y == cursor_y) && (x == cursor_x) &&
					((uint8_t)(scy % 16) >= (cga_datareg[CGA_REG_DATA_CURSOR_BEGIN] & 31) * 2) &&
					((uint8_t)(scy % 16) <= (cga_datareg[CGA_REG_DATA_CURSOR_END] & 31) * 2) &&
					cga_cursor_blink_state && blinkenable) { //cursor should be displayed
					cga_framebuffer[scy][scx] = cga_color(attr & 0x0F);
				}
				else {
					if (blinkenable && blink && !cga_cursor_blink_state) {
						fontdata = 0; //all pixels in character get background color if blink attribute set and blink visible state is false
					}
					cga_framebuffer[scy][scx] = cga_color(fontdata ? (attr & 0x0F) : (attr >> 4));
				}
			}
		}
		break;
	case CGA_MODE_TEXT_40X25:
		cursor_x = cursorloc % 40;
		cursor_y = cursorloc / 40;
		for (scy = start_y; scy <= end_y; scy++) {
			y = scy / 16;
			for (scx = start_x; scx <= end_x; scx += 2) {
				x = scx / 16;
				addr = startaddr + ((y * 40) + x) * 2;
				cc = cga_RAM[addr];
				attr = cga_RAM[addr + 1];
				blink = attr >> 7;
				if (blinkenable) attr &= 0x7F; //enabling text mode blink attribute limits background color selection
				fontdata = cga_font[2048 + (cc * 8) + ((scy % 16) / 2)];
				fontdata = (fontdata >> (7 - ((scx / 2) % 8))) & 1;
				if ((y == cursor_y) && (x == cursor_x) &&
					((uint8_t)(scy % 16) >= (cga_datareg[CGA_REG_DATA_CURSOR_BEGIN] & 31) * 2) &&
					((uint8_t)(scy % 16) <= (cga_datareg[CGA_REG_DATA_CURSOR_END] & 31) * 2) &&
					cga_cursor_blink_state && blinkenable) {
					cga_framebuffer[scy][scx] = cga_color(attr & 0x0F);
				}
				else {
					if (blinkenable && blink && !cga_cursor_blink_state) {
						fontdata = 0;
					}
					cga_framebuffer[scy][scx] = cga_color(fontdata ? (attr & 0x0F) : (attr >> 4));
				}
				cga_framebuffer[scy][scx + 1] = cga_framebuffer[scy][scx]; //double pixels horizontally
			}
		}
		break;
	case CGA_MODE_GRAPHICS_LO:
		for (scy = start_y; scy <= end_y; scy += 2) {
			uint8_t isodd;
			isodd = scy & 2;
			y = scy >> 2;
			for (scx = start_x; scx <= end_x; scx += 2) {
				x = scx >> 1;
				addr = (isodd ? 0x2000 : 0x0000) + (y * 80) + (x >> 2);
				cc = cga_RAM[addr];
				cc = cga_gfxpal[intensity][colorset][(cc >> ((3 - (x & 3)) << 1)) & 3];
				cga_framebuffer[scy][scx] = cga_color(cc);
				cga_framebuffer[scy][scx + 1] = cga_framebuffer[scy][scx];
				cga_framebuffer[scy + 1][scx + 1] = cga_framebuffer[scy][scx];
				cga_framebuffer[scy + 1][scx] = cga_framebuffer[scy][scx];
			}
		}
		break;
	case CGA_MODE_GRAPHICS_HI:
		cursor_x = cursorloc % 40;
		cursor_y = cursorloc / 40;
		for (scy = start_y; scy <= end_y; scy += 2) {
			uint8_t isodd;
			isodd = scy & 2;
			y = scy >> 2;
			for (scx = start_x; scx <= end_x; scx++) {
				x = scx;
				addr = (isodd ? 0x2000 : 0x0000) + (y * 80) + (x >> 3);
				cc = cga_RAM[addr];
				cc = ((cc >> (7 - (x & 7))) & 1) * 15;
				cga_framebuffer[scy][scx] = cga_color(cc);
				cga_framebuffer[scy + 1][scx] = cga_framebuffer[scy][scx];
			}
		}
		break;
	}

	sdlconsole_blit((uint32_t *)cga_framebuffer, 640, 400, 640 * sizeof(uint32_t));
}

void cga_renderThread(void* dummy) {
	while (running) {
		if (cga_doDraw == 1) {
			cga_update(0, 0, 639, 399);
			cga_doDraw = 0;
		}
		else {
			utility_sleep(1);
		}
	}
#ifdef _WIN32
	_endthread();
#else
	pthread_exit(NULL);
#endif
}

void cga_writeport(void* dummy, uint16_t port, uint8_t value) {
#ifdef DEBUG_CGA
	debug_log(DEBUG_DETAIL, "Write CGA port: %02X -> %03X (indexreg = %02X)\r\n", value, port, cga_indexreg);
#endif
	switch (port) {
	case 0x3D4:
		cga_indexreg = value;
		break;
	case 0x3D5:
		cga_datareg[cga_indexreg] = value;
		break;
	case 0x3DA:
		break;
	default:
		cga_regs[port - 0x3D0] = value;
	}
}

uint8_t cga_readport(void* dummy, uint16_t port) {
#ifdef DEBUG_CGA
	debug_log(DEBUG_DETAIL, "Read CGA port: %03X (indexreg = %02X)\r\n", port, cga_indexreg);
#endif
	switch (port) {
	case 0x3D4:
		return cga_indexreg;
	case 0x3D5:
		//if ((cga_indexreg < 0x0E) || (cga_indexreg > 0x0F)) return 0xFF;
		return cga_datareg[cga_indexreg];
	case 0x3DA:
		return cga_regs[0xA]; //rand() & 0xF;
	}
	return cga_regs[port - 0x3D0]; //0xFF;
}

void cga_writememory(void* dummy, uint32_t addr, uint8_t value) {
	addr -= 0xB8000;
	if (addr >= 16384) return;

	cga_RAM[addr] = value;
}

uint8_t cga_readmemory(void* dummy, uint32_t addr) {
	addr -= 0xB8000;
	if (addr >= 16384) return 0xFF;

	return cga_RAM[addr];
}

void cga_blinkCallback(void* dummy) {
	cga_cursor_blink_state ^= 1;
}

void cga_scanlineCallback(void* dummy) {
	/*
		NOTE: We are only doing very approximate CGA timing. Breaking the horizontal scan into
		four parts and setting the display inactive bit on 3DAh on the last quarter of it. Being
		more precise shouldn't be necessary and will take much more host CPU time.

		TODO: Look into whether this is true? So far, things are working fine.
	*/
	static uint16_t scanline = 0, hpart = 0;

	cga_regs[0xA] = 6; //light pen bits always high
	cga_regs[0xA] |= (hpart == 3) ? 1 : 0;
	cga_regs[0xA] |= (scanline >= 224) ? 8 : 0;
	
	hpart++;
	if (hpart == 4) {
		/*if (scanline < 200) {
			cga_update(0, (scanline<<1), 639, (scanline<<1)+1);
		}*/
		hpart = 0;
		scanline++;
	}
	if (scanline == 256) {
		scanline = 0;
	}
}

void cga_drawCallback(void* dummy) {
	cga_doDraw = 1;
}
