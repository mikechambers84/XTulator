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
#include <string.h> //for malloc
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
pthread_t vga_renderThreadID;
#endif
#include "vga.h"
#include "../../config.h"
#include "../../timing.h"
#include "../../utility.h"
#include "../../ports.h"
#include "../../memory.h"
#include "../../debuglog.h"
#include "sdlconsole.h"

uint8_t VBIOS[32768];

uint8_t vga_palette[256][3]; //R, G, B

const uint8_t vga_gfxpal[2][2][4] = { //palettes for 320x200 graphics mode 2bpp
	{
		{ 0, 2, 4, 6 }, //normal palettes
		{ 0, 3, 5, 7 }
	},
	{
		{ 0, 10, 12, 14 }, //intense palettes
		{ 0, 11, 13, 15 }
	}
};

const uint32_t vga_fontbases[8] = { 0x0000, 0x4000, 0x8000, 0xC000, 0x2000, 0x6000, 0xA000, 0xE000 };

VGADAC_t vga_DAC;
uint32_t vga_framebuffer[1024][1024], vga_dots = 8;
volatile uint32_t vga_w = 640, vga_h = 400;
uint32_t vga_membase, vga_memmask;
uint16_t vga_cursorloc = 0;
uint8_t vga_dbl = 0;
uint8_t vga_crtci = 0, vga_crtcd[0x19];
uint8_t vga_attri = 0, vga_attrd[0x15], vga_attrflipflop = 0, vga_attrpal = 0x20;
uint8_t vga_gfxi = 0, vga_gfxd[0x09];
uint8_t vga_seqi = 0, vga_seqd[0x05];
uint8_t vga_misc, vga_status0, vga_status1;
uint8_t vga_cursor_blink_state = 0;
volatile uint8_t vga_wmode, vga_rmode, vga_shiftmode, vga_rotate, vga_logicop, vga_enableplane, vga_readmap, vga_scandbl, vga_hdbl, vga_bpp, vga_latch[4];
uint8_t* vga_RAM[4]; //4 planes

volatile uint64_t vga_hblankstart, vga_hblankend, vga_hblanklen, vga_dispinterval, vga_hblankinterval, vga_htotal;
volatile uint64_t vga_vblankstart, vga_vblankend, vga_vblanklen, vga_vblankinterval, vga_frameinterval;
volatile uint8_t vga_doRender = 0, vga_doBlit = 0;
volatile double vga_targetFPS = 60, vga_lockFPS = 0;

volatile uint32_t vga_hblankTimer, vga_hblankEndTimer, vga_drawTimer;
volatile uint16_t vga_curScanline = 0;

int vga_init() {
	int x, y, i;

	debug_log(DEBUG_INFO, "[VGA] Initializing VGA video device\r\n");

	for (y = 0; y < 400; y++) {
		for (x = 0; x < 640; x++) {
			vga_framebuffer[y][x] = vga_color(0);
		}
	}
	sdlconsole_blit((uint32_t*)vga_framebuffer, 640, 400, 1024 * sizeof(uint32_t));

	if (vga_lockFPS >= 1) {
		vga_targetFPS = vga_lockFPS;
	}

	timing_addTimer(vga_blinkCallback, NULL, 3.75, TIMING_ENABLED);
	vga_drawTimer = timing_addTimer(vga_drawCallback, NULL, vga_targetFPS, TIMING_ENABLED);
	vga_hblankTimer = timing_addTimer(vga_hblankCallback, NULL, 10000, TIMING_ENABLED); //nonsense frequency values to begin with is fine
	vga_hblankEndTimer = timing_addTimer(vga_hblankEndCallback, NULL, 100, TIMING_ENABLED); //same here
	vga_curScanline = 0;

	for (i = 0; i < 4; i++) { //4 planes of 64 KB (It's actually 64K addresses on a 32-bit data bus on real VGA hardware)
		vga_RAM[i] = (uint8_t*)malloc(65536);
		if (vga_RAM[i] == NULL) break;
	}
	if (i < 4) { //If there was an allocation error
		for (; i >= 0; i--) { //step back through any successfully allocated chunks
			free((void*)vga_RAM[i]); //and free them
		}
		return -1;
	}

	//TODO: error checking below
#ifdef _WIN32
	_beginthread(vga_renderThread, 0, NULL);
#else
	pthread_create(&vga_renderThreadID, NULL, vga_renderThread, NULL);
#endif

	ports_cbRegister(0x3B4, 39, (void*)vga_readport, NULL, (void*)vga_writeport, NULL, NULL);
	memory_mapCallbackRegister(0xA0000, 0x20000, (void*)vga_readmemory, (void*)vga_writememory, NULL);

	if (utility_loadFile(VBIOS, 32768, "roms/video/et4000.bin")) {
		return -1;
	}
	memory_mapRegister(0xC0000, 32768, VBIOS, NULL);

	return 0;
}

void vga_updateScanlineTiming() {
	double pixelclock;
	static uint32_t lastw = 0, lasth = 0;
	static double lastFPS = 0;

	if (vga_misc & 0x04) { //pixel clock select
		pixelclock = 28322000.0;
	}
	else {
		pixelclock = 25175000.0;
	}

	vga_hblankstart = (uint64_t)vga_crtcd[0x02] * (uint64_t)vga_dots;
	vga_hblankend = ((uint64_t)vga_crtcd[0x02] * (uint64_t)vga_dots) + (((uint64_t)vga_crtcd[0x03] & 0x1F) + 1) * (uint64_t)vga_dots;
	vga_hblanklen = vga_hblankend - vga_hblankstart;
	vga_vblankstart = (uint64_t)vga_crtcd[0x10] | ((uint64_t)(vga_crtcd[0x07] & 0x04) << 6) | ((uint64_t)(vga_crtcd[0x07] & 0x80) << 2);
	vga_vblankend = (uint64_t)vga_crtcd[0x06] | ((uint64_t)(vga_crtcd[0x07] & 0x01) << 8) | ((uint64_t)(vga_crtcd[0x07] & 0x20) << 4);
	vga_vblanklen = vga_vblankend - vga_vblankstart;
	vga_htotal = (uint64_t)vga_crtcd[0x00];
	vga_targetFPS = pixelclock / ((double)(vga_htotal + 5) * (double)vga_dots * (double)vga_vblankend);

	pixelclock = (double)timing_getFreq() / pixelclock; //get ratio of pixel clock vs our timer frequency for interval calculations
	vga_dispinterval = (uint64_t)((double)(vga_htotal + 5) * (double)vga_dots * pixelclock);
	vga_hblankinterval = (uint64_t)((double)vga_hblanklen * pixelclock);
	vga_vblankinterval = (uint64_t)((double)vga_hblankend * (double)vga_vblanklen * pixelclock);
	vga_frameinterval = (uint64_t)((double)vga_hblankend * (double)vga_vblankend * pixelclock);
	/*printf("hblank start = %llu, hblank end = %llu, hblank len = %llu, disp interval = %llu, hblank interval = %llu, freq = %llu\r\n",
		vga_hblankstart, vga_hblankend, vga_hblanklen, vga_dispinterval, vga_hblankinterval, timing_freq);
	printf("vblank start = %llu, vblank end = %llu, vblank len = %llu, vblank interval = %llu, frameinterval = %llu\r\n",
		vga_vblankstart, vga_vblankend, vga_vblanklen, vga_vblankinterval, vga_frameinterval);*/
	if ((lastw != vga_w) || (lasth != vga_h) || (lastFPS != vga_targetFPS)) {
		debug_log(DEBUG_DETAIL, "[VGA] Mode switch: %lux%lu (%.02f Hz)\r\n", vga_w, vga_h, vga_targetFPS);
		lastw = vga_w;
		lasth = vga_h;
		lastFPS = vga_targetFPS;
	}

	timing_updateInterval(vga_hblankTimer, vga_dispinterval);
	timing_updateInterval(vga_hblankEndTimer, vga_hblankinterval);
	timing_timerEnable(vga_hblankTimer);
	timing_timerDisable(vga_hblankEndTimer);
	if (vga_lockFPS == 0) {
		timing_updateIntervalFreq(vga_drawTimer, vga_targetFPS);
	}
}

void vga_update(uint32_t start_x, uint32_t start_y, uint32_t end_x, uint32_t end_y) {
	uint32_t addr, startaddr, cursorloc, cursor_x, cursor_y, fontbase, color32;
	uint32_t scx, scy, x, y, hchars, divx, yscanpixels, xscanpixels, xstride, bpp, pixelsperbyte, shift;
	uint8_t cc, attr, fontdata, blink, mode, colorset, intensity, blinkenable, cursorenable, dup9;

	//debug_log(DEBUG_DETAIL, "Width: %u\r\n", vga_crtcd[0x01] - ((vga_crtcd[0x05] & 0x60) >> 5));
	if (vga_attrd[0x10] & 1) { //graphics mode enable
		if (vga_shiftmode & 0x02) {
			xscanpixels = 2;
			yscanpixels = (vga_crtcd[0x09] & 0x1F) + 1;
		} else {
			xscanpixels = (vga_seqd[0x01] & 0x08) ? 2 : 1;
			yscanpixels = (vga_crtcd[0x09] & 0x80) ? 2 : 1;
		}
		switch (vga_shiftmode) {
		case 0x00:
			if ((vga_attrd[0x12] & 0x0F) == 0x01) { //TODO: is this the right way to detect 1bpp mode?
				bpp = 1;
				pixelsperbyte = 8;
				mode = VGA_MODE_GRAPHICS_1BPP;
			} else {
				bpp = 4;
				mode = VGA_MODE_GRAPHICS_4BPP;
				pixelsperbyte = 8;
			}
			break;
		case 0x01:
			bpp = 2;
			pixelsperbyte = 4;
			mode = VGA_MODE_GRAPHICS_2BPP;
			break;
		case 0x02:
		case 0x03:
			bpp = 8;
			pixelsperbyte = 1;
			mode = VGA_MODE_GRAPHICS_8BPP;
			break;
		}
		xstride = (vga_w / xscanpixels) / pixelsperbyte;
#ifdef DEBUG_VGA
		debug_log(DEBUG_DETAIL, "[VGA] Resolution: %lux%lu %lu bpp (X stride: %lu, V lines per pixel: %lu, H lines per pixel = %lu)\r\n",
			vga_w, vga_h, bpp, xstride, yscanpixels, xscanpixels);
#endif
	} else { //text mode enable
		mode = VGA_MODE_TEXT;
		hchars = vga_dbl ? 40 : 80;
		divx = vga_dbl ? vga_dots * 2 : vga_dots;
		cursorenable = (vga_crtcd[0x0A] & 0x20) ? 0 : 1; //TODO: fix this
		blinkenable = 0;
		fontbase = vga_fontbases[vga_seqd[0x03]];
		dup9 = (vga_attrd[0x10] & 0x04) ? 0 : 1;
		vga_scandbl = 0;
#ifdef DEBUG_VGA
		debug_log(DEBUG_DETAIL, "[VGA] Resolution: %lux%lu (text mode)\r\n",
			vga_w, vga_h);
#endif
	}
	intensity = 0;
	colorset = 0;
	startaddr = ((uint32_t)vga_crtcd[0xC] << 8) | (uint32_t)vga_crtcd[0xD];
	cursorloc = ((uint32_t)vga_crtcd[0xE] << 8) | (uint32_t)vga_crtcd[0xF];

	switch (mode) {
	case VGA_MODE_TEXT:
		dup9 = 1; //TODO: fix this hack
		cursor_x = cursorloc % hchars;
		cursor_y = cursorloc / hchars;
		for (scy = start_y; scy <= end_y; scy++) {
			uint32_t maxscan = ((vga_crtcd[0x09] & 0x1F) + 1);
			y = scy / maxscan;
			for (scx = start_x; scx <= end_x; scx++) {
				uint32_t charcolumn;
				x = scx / divx;
				addr = startaddr + (y * hchars) + x;
				cc = vga_RAM[0][addr];
				attr = vga_RAM[1][addr];
				blink = attr >> 7;
				if (blinkenable) attr &= 0x7F; //enabling text mode blink attribute limits background color selection
				fontdata = vga_RAM[2][fontbase + ((uint32_t)cc * 32) + (scy % maxscan)];
				charcolumn = ((scx >> (vga_dbl ? 1 : 0)) % vga_dots);
				if (dup9 && (charcolumn == 0) && (cc >= 0xC0) && (cc <= 0xDF)) {
					charcolumn = 1;
				}
				fontdata = (fontdata >> ((vga_dots - 1) - charcolumn)) & 1;
				if ((y == cursor_y) && (x == cursor_x) &&
					((uint8_t)(scy % 16) >= (vga_crtcd[VGA_REG_DATA_CURSOR_BEGIN] & 31)) &&
					((uint8_t)(scy % 16) <= (vga_crtcd[VGA_REG_DATA_CURSOR_END] & 31)) &&
					vga_cursor_blink_state && cursorenable) { //cursor should be displayed
					color32 = vga_attrd[attr & 0x0F] | (vga_attrd[0x14] << 4);
					if (vga_attrd[0x10] & 0x80) { //P5, P4 replace
						color32 = (color32 & 0xCF) | ((vga_attrd[0x14] & 3) << 4);
					}
					color32 = vga_color(color32);
					vga_framebuffer[scy][scx] = color32;
				}
				else {
					if (blinkenable && blink && !vga_cursor_blink_state) {
						fontdata = 0; //all pixels in character get background color if blink attribute set and blink visible state is false
					}
					//determine index into actual DAC palette
					color32 = vga_attrd[fontdata ? (attr & 0x0F) : (attr >> 4)] | (vga_attrd[0x14] << 4);
					if (vga_attrd[0x10] & 0x80) { //P5, P4 replace
						color32 = (color32 & 0xCF) | ((vga_attrd[0x14] & 3) << 4);
					}
					color32 = vga_color(color32);
					vga_framebuffer[scy][scx] = color32;
				}
			}
		}
		break;
	case VGA_MODE_GRAPHICS_8BPP:
		for (scy = start_y; scy <= end_y; scy += yscanpixels) {
			y = scy / yscanpixels;
			for (scx = start_x; scx <= end_x; scx += xscanpixels) {
				uint8_t plane;
				uint32_t yadd, xadd, color32;
				x = scx / xscanpixels;
				//x += vga_attrd[0x13] & 0x0F;
				addr = ((y * xstride) + x) & 0xFFFF;
				plane = addr & 3;
				addr = (addr >> 2) + startaddr;
				cc = vga_RAM[plane][addr & 0xFFFF];
				color32 = vga_color(cc);
				for (yadd = 0; yadd < yscanpixels; yadd++) {
					for (xadd = 0; xadd < xscanpixels; xadd++) {
						vga_framebuffer[scy + yadd][scx + xadd] = color32;
					}
				}
			}
		}
		break;
	case VGA_MODE_GRAPHICS_4BPP:
		for (scy = start_y; scy <= end_y; scy += yscanpixels) {
			y = scy / yscanpixels;
			for (scx = start_x; scx <= end_x; scx += xscanpixels) {
				uint32_t yadd, xadd;
				x = scx / xscanpixels;
				//x += vga_attrd[0x13] & 0x0F;
				addr = ((y * xstride) + (x / 8)) & 0xFFFF;
				addr = addr + startaddr;
				shift = 7 - (x & 7);
				cc = (vga_RAM[0][addr & 0xFFFF] >> shift) & 1;
				cc |= ((vga_RAM[1][addr & 0xFFFF] >> shift) & 1) << 1;
				cc |= ((vga_RAM[2][addr & 0xFFFF] >> shift) & 1) << 2;
				cc |= ((vga_RAM[3][addr & 0xFFFF] >> shift) & 1) << 3;
				//determine index into actual DAC palette
				color32 = vga_attrd[cc] | (vga_attrd[0x14] << 4);
				if (vga_attrd[0x10] & 0x80) { //P5, P4 replace
					color32 = (color32 & 0xCF) | ((vga_attrd[0x14] & 3) << 4);
				}
				color32 = vga_color(color32);
				for (yadd = 0; yadd < yscanpixels; yadd++) {
					for (xadd = 0; xadd < xscanpixels; xadd++) {
						vga_framebuffer[scy + yadd][scx + xadd] = color32;
					}
				}
			}
		}
		break;
	case VGA_MODE_GRAPHICS_2BPP:
		for (scy = start_y; scy <= end_y; scy += yscanpixels) {
			uint8_t isodd;
			y = scy / yscanpixels;
			isodd = y & 1;
			y >>= 1;
			for (scx = start_x; scx <= end_x; scx += xscanpixels) {
				uint32_t yadd, xadd;
				x = scx / xscanpixels;
				//x += vga_attrd[0x13] & 0x0F;
				addr = ((8192 * isodd) + (y * xstride) + (x / pixelsperbyte)) & 0xFFFF;
				addr = addr + startaddr;
				shift = (3 - (x & 3)) << 1;
				cc = (vga_RAM[addr & 1][addr >> 1] >> shift) & 3;
				//determine index into actual DAC palette
				color32 = vga_attrd[cc] | (vga_attrd[0x14] << 4);
				if (vga_attrd[0x10] & 0x80) { //P5, P4 replace
					color32 = (color32 & 0xCF) | ((vga_attrd[0x14] & 3) << 4);
				}
				color32 = vga_color(color32);
				for (yadd = 0; yadd < yscanpixels; yadd++) {
					for (xadd = 0; xadd < xscanpixels; xadd++) {
						vga_framebuffer[scy + yadd][scx + xadd] = color32;
					}
				}
			}
		}
		break;
	case VGA_MODE_GRAPHICS_1BPP:
		for (scy = start_y; scy <= end_y; scy += yscanpixels) {
			uint8_t isodd;
			y = scy / yscanpixels;
			isodd = y & 1;
			y >>= 1;
			for (scx = start_x; scx <= end_x; scx += xscanpixels) {
				uint32_t yadd, xadd;
				x = scx / xscanpixels;
				//x += vga_attrd[0x13] & 0x0F;
				addr = ((8192 * isodd) + (y * xstride) + (x / pixelsperbyte)) & 0xFFFF;
				addr = addr + startaddr;
				shift = 7 - (x & 7);
				cc = (vga_RAM[0][addr] >> shift) & 1;
				color32 = cc ? 0xFFFFFFFF : 0x00000000;
				for (yadd = 0; yadd < yscanpixels; yadd++) {
					for (xadd = 0; xadd < xscanpixels; xadd++) {
						vga_framebuffer[scy + yadd][scx + xadd] = color32;
					}
				}
			}
		}
		break;

	}
}

void vga_renderThread(void* dummy) {
	while (running) {
		if (vga_doRender == 1) {
			vga_update(0, 0, vga_w - 1, vga_h - 1);
			vga_doRender = 0;
		}

		if (vga_doBlit == 1) {
			sdlconsole_blit((uint32_t*)vga_framebuffer, (int)vga_w, (int)vga_h, 1024 * sizeof(uint32_t));
			vga_doBlit = 0;
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

void vga_calcmemorymap() {
	switch (vga_gfxd[0x06] & 0x0C) {
	case 0x00: //0xA0000 - 0xBFFFF (128 KB)
		vga_membase = 0x00000;
		vga_memmask = 0xFFFF;
		break;
	case 0x04: //0xA0000 - 0xAFFFF (64 KB)
		vga_membase = 0x00000;
		vga_memmask = 0xFFFF;
		break;
	case 0x08: //0xB0000 - 0xB7FFF (32 KB)
		vga_membase = 0x10000;
		vga_memmask = 0x7FFF;
		break;
	case 0x0C: //0xB8000 - 0xBFFFF (32 KB)
		vga_membase = 0x18000;
		vga_memmask = 0x7FFF;
		break;
	}
	//debug_log(DEBUG_DETAIL, "vga_membase = %05X, vga_memmask = %04X\r\n", vga_membase, vga_memmask);
}

void vga_calcscreensize() {
	vga_w = (1 + vga_crtcd[0x01] - ((vga_crtcd[0x05] & 0x60) >> 5)) * vga_dots;
	vga_h = 1 + vga_crtcd[0x12] | ((vga_crtcd[0x07] & 2) ? 0x100 : 0) | ((vga_crtcd[0x07] & 64) ? 0x200 : 0);

	if (((vga_shiftmode & 0x20) == 0) && (vga_seqd[0x01] & 0x08)) {
		vga_w <<= 1;
	}

	vga_updateScanlineTiming();

	//debug_log(DEBUG_DETAIL, "video size: %lux%lu\r\n", vga_w, vga_h);
}

uint8_t vga_readcrtci() {
	return vga_crtci;
}

uint8_t vga_readcrtcd() {
	if (vga_crtci < 0x19) {
		return vga_crtcd[vga_crtci];
	}
	return 0xFF;
}

void vga_writecrtci(uint8_t value) {
	vga_crtci = value & 0x1F;
}

void vga_writecrtcd(uint8_t value) {
	if (vga_crtci > 0x18) return;

	vga_crtcd[vga_crtci] = value;
	//debug_log(DEBUG_DETAIL, "VGA CRTC index %02X = %u\r\n", vga_crtci, value);
	switch (vga_crtci) {
	case 0x01:
	case 0x12:
	case 0x07:
		vga_calcscreensize();
		break;
	//case 0x09:
		//vga_scandbl = value & 0x1F; //(value & 0x80) ? 1 : 0;
		//vga_calcscreensize();
		//break;
	}
}

void vga_writeport(void* dummy, uint16_t port, uint8_t value) {
#ifdef DEBUG_VGA
	debug_log(DEBUG_DETAIL, "Write VGA port: %02X -> %03X\r\n", value, port);
#endif
	switch (port) {
	case 0x3B4:
		if ((vga_misc & 1) == 0) {
			vga_writecrtci(value);
		}
		break;
	case 0x3B5:
		if ((vga_misc & 1) == 0) {
			vga_writecrtcd(value);
		}
		break;
	case 0x3C0:
	case 0x3C1:
		if (vga_attrflipflop == 0) {
			vga_attri = value & 0x1F;
			vga_attrpal = value & 0x20;
		}
		else {
			if (vga_attri < 0x15) {
				vga_attrd[vga_attri] = value;
			}
		}
		vga_attrflipflop ^= 1;
		break;
	case 0x3C7:
		vga_DAC.state = VGA_DAC_MODE_READ;
		vga_DAC.index = value;
		vga_DAC.step = 0;
		break;
	case 0x3C8:
		vga_DAC.state = VGA_DAC_MODE_WRITE;
		vga_DAC.index = value;
		vga_DAC.step = 0;
		break;
	case 0x3C9:
		//debug_log(DEBUG_DETAIL, "write pal %u = %02X\r\n", vga_DAC.index, value & 0x3F);
		vga_DAC.pal[vga_DAC.index][vga_DAC.step++] = value & 0x3F;
		if (vga_DAC.step == 3) {
			vga_palette[vga_DAC.index][0] = vga_DAC.pal[vga_DAC.index][0] << 2;
			vga_palette[vga_DAC.index][1] = vga_DAC.pal[vga_DAC.index][1] << 2;
			vga_palette[vga_DAC.index][2] = vga_DAC.pal[vga_DAC.index][2] << 2;
			vga_DAC.step = 0;
			vga_DAC.index++;
		}
		break;
	case 0x3C2:
		vga_misc = value;
		break;
	case 0x3C4:
		vga_seqi = value & 0x1F;
		break;
	case 0x3C5:
		if (vga_seqi < 0x05) {
			vga_seqd[vga_seqi] = value;
			switch (vga_seqi) {
			case 0x01:
				vga_dots = (value & 0x01) ? 8 : 9;
				vga_dbl = (value & 0x08) ? 1 : 0;
				vga_calcscreensize();
				break;
			case 0x02:
				vga_enableplane = value & 0x0F;
				break;
			}
		}
		break;
	case 0x3CE:
		vga_gfxi = value & 0x1F;
		break;
	case 0x3CF:
		if (vga_gfxi < 0x09) {
			vga_gfxd[vga_gfxi] = value;
			switch (vga_gfxi) {
			case 0x03:
				vga_rotate = value & 7;
				vga_logicop = (value >> 3) & 3;
				break;
			case 0x04:
				vga_readmap = value & 3;
				break;
			case 0x05:
				vga_wmode = value & 3;
				vga_rmode = (value >> 3) & 1;
				vga_shiftmode = (value >> 5) & 3;
				//debug_log(DEBUG_DETAIL, "wmode = %u\r\n", vga_wmode);
				//debug_log(DEBUG_DETAIL, "rmode = %u\r\n", vga_rmode);
				break;
			case 0x06:
				vga_calcmemorymap();
				break;
			}
		}
		break;
	case 0x3D4:
		if ((vga_misc & 1) == 1) {
			vga_writecrtci(value);
		}
		break;
	case 0x3D5:
		if ((vga_misc & 1) == 1) {
			vga_writecrtcd(value);
		}
		break;
	}
}

uint8_t vga_readport(void* dummy, uint16_t port) {
	uint8_t ret = 0xFF;
#ifdef DEBUG_VGA
	debug_log(DEBUG_DETAIL, "Read VGA port: %03X\r\n", port);
#endif
	switch (port) {
	case 0x3B4:
		if ((vga_misc & 1) == 0) {
			return vga_readcrtci();
		}
		break;
	case 0x3B5:
		if ((vga_misc & 1) == 0) {
			return vga_readcrtcd();
		}
		break;
	case 0x3C0:
		if (vga_attrflipflop == 0) {
			ret = vga_attri | vga_attrpal;
		}
		else {
			if (vga_attri < 0x15) {
				ret = vga_attrd[vga_attri];
			}
		}
		break;
	case 0x3C1:
		if (vga_attri < 0x15) {
			return vga_attrd[vga_attri];
		}
		break;
	case 0x3C4:
		return vga_seqi;
	case 0x3C5:
		if (vga_seqi < 0x05) {
			return vga_seqd[vga_seqi];
		}
		break;
	case 0x3C7:
		return vga_DAC.state;
	case 0x3C8:
		return vga_DAC.index;
	case 0x3C9:
		ret = vga_DAC.pal[vga_DAC.index][vga_DAC.step++];
		if (vga_DAC.step == 3) {
			vga_DAC.step = 0;
			vga_DAC.index++;
		}
		break;
	case 0x3CC:
		return vga_misc;
	case 0x3CE:
		return vga_gfxi;
	case 0x3CF:
		if (vga_gfxi < 0x09) {
			return vga_gfxd[vga_gfxi];
		}
		break;
	case 0x3D4:
		if ((vga_misc & 1) == 1) {
			return vga_readcrtci();
		}
		break;
	case 0x3D5:
		if ((vga_misc & 1) == 1) {
			return vga_readcrtcd();
		}
		break;
	case 0x3DA:
		vga_attrflipflop = 0; //because VGA is weird
		return vga_status1;
	}
	return ret;
}

uint8_t vga_dologic(uint8_t value, uint8_t latch) {
	switch (vga_logicop) {
	case 0:
		return value;
	case 1:
		return value & latch;
	case 2:
		return value | latch;
	default: //3, but say default to stop MSVC from warning me
		return value ^ latch;
	}
}

void vga_writememory(void* dummy, uint32_t addr, uint8_t value) {
	uint8_t temp, plane;
	if ((vga_misc & 0x02) == 0) return; //RAM writes are disabled
	addr -= 0xA0000;
	addr = (addr - vga_membase) & vga_memmask; //TODO: Is this right?

	if (vga_gfxd[0x05] & 0x10) { //host odd/even mode (text)
		vga_RAM[addr & 1][addr >> 1] = value;
		return;
	}

	if (vga_seqd[0x04] & 0x08) { //chain-4
		vga_RAM[addr & 3][addr >> 2] = value;
		return;
	}

	switch (vga_wmode) {
	case 0:
		for (plane = 0; plane < 4; plane++) {
			if (vga_enableplane & (1 << plane)) { //are we allowed to write to this plane?
				if (vga_gfxd[0x01] & (1 << plane)) { //test enable set/reset bit for plane
					temp = (vga_gfxd[0x00] & (1 << plane)) ? 0xFF : 0x00; //set/reset expansion as source data
				} else { //host data as source
					temp = vga_dorotate(value);
				}
				temp = vga_dologic(temp, vga_latch[plane]);
				temp = (temp & vga_gfxd[0x08]) | (vga_latch[plane] & (~vga_gfxd[0x08]));
				vga_RAM[plane][addr] = temp;
				//debug_log(DEBUG_DETAIL, "mode 0 write plane %u\r\n", plane);
			}
		}
		break;
	case 1:
		for (plane = 0; plane < 4; plane++) {
			if (vga_enableplane & (1 << plane)) {
				vga_RAM[plane][addr] = vga_latch[plane];
				//debug_log(DEBUG_DETAIL, "mode 1 write plane %u\r\n", plane);
			}
		}
		break;
	case 2:
		for (plane = 0; plane < 4; plane++) {
			if (vga_enableplane & (1 << plane)) {
				temp = (value & (1 << plane)) ? 0xFF : 0x00;
				temp = vga_dologic(temp, vga_latch[plane]);
				temp = (temp & vga_gfxd[0x08]) | (vga_latch[plane] & (~vga_gfxd[0x08]));
				vga_RAM[plane][addr] = temp;
			}
		}
		break;
	case 3:
		for (plane = 0; plane < 4; plane++) {
			if (vga_enableplane & (1 << plane)) {
				temp = (vga_gfxd[0x00] & (1 << plane)) ? 0xFF : 0x00;
				temp = (vga_dorotate(value) & vga_gfxd[0x08]) | (temp & (~vga_gfxd[0x08])); //bit mask logic
				vga_RAM[plane][addr] = temp;
			}
		}
		break;
	}
}

uint8_t vga_readmemory(void* dummy, uint32_t addr) {
	uint8_t plane, ret;

	addr -= 0xA0000;
	addr = (addr - vga_membase) & vga_memmask; //TODO: Is this right?

	if (vga_gfxd[0x05] & 0x10) { //host odd/even mode (text)
		return vga_RAM[addr & 1][addr >> 1];
	}

	if (vga_seqd[0x04] & 0x08) { //chain-4
		return vga_RAM[addr & 3][addr >> 2];
	}

	vga_latch[0] = vga_RAM[0][addr];
	vga_latch[1] = vga_RAM[1][addr];
	vga_latch[2] = vga_RAM[2][addr];
	vga_latch[3] = vga_RAM[3][addr];

	if (vga_rmode == 0) {
		return vga_RAM[vga_readmap][addr];
	} else {
		//TODO: Is this correct?
		ret = 0;
		for (plane = 0; plane < 4; plane++) {
			if (vga_gfxd[0x07] & (1 << plane)) { //color don't care bit check
				if ((vga_RAM[plane][addr] & 0x0F) == (vga_gfxd[0x02] & 0x0F)) { //compare RAM value with color compare register
					ret |= 1 << plane; //set bit if true
				}
			}
		}
		return ret;
	}
}

void vga_drawCallback(void* dummy) {
	vga_doRender = 1;
	vga_doBlit = 1;
}

void vga_blinkCallback(void* dummy) {
	vga_cursor_blink_state ^= 1;
}

void vga_hblankCallback(void* dummy) {
	timing_timerEnable(vga_hblankEndTimer);
	vga_status1 |= 0x01;
	vga_curScanline++;
	if (vga_curScanline == vga_vblankstart) {
		vga_status1 |= 0x08;
	}
	else if (vga_curScanline == vga_vblankend) {
		vga_curScanline = 0;
		vga_status1 &= 0xF7;
	}
}

void vga_hblankEndCallback(void* dummy) {
	timing_timerDisable(vga_hblankEndTimer);
	vga_status1 &= 0xFE;
}

void vga_dumpregs() {
#ifdef DEBUG_VGA
	int i;
	debug_log(DEBUG_DETAIL, "VGA registers:\r\n");
	for (i = 0; i < 0x15; i++) {
		debug_log(DEBUG_DETAIL, "  ATTR[0x%02X] = %u%u%u%u%u%u%u%u\r\n", i,
			(vga_attrd[i] >> 7) & 1, (vga_attrd[i] >> 6) & 1, (vga_attrd[i] >> 5) & 1, (vga_attrd[i] >> 4) & 1,
			(vga_attrd[i] >> 3) & 1, (vga_attrd[i] >> 2) & 1, (vga_attrd[i] >> 1) & 1, (vga_attrd[i] >> 0) & 1);
	}
	debug_log(DEBUG_DETAIL, "\r\n");
	for (i = 0; i < 0x05; i++) {
		debug_log(DEBUG_DETAIL, "  SEQ[0x%02X] = %u%u%u%u%u%u%u%u\r\n", i,
			(vga_seqd[i] >> 7) & 1, (vga_seqd[i] >> 6) & 1, (vga_seqd[i] >> 5) & 1, (vga_seqd[i] >> 4) & 1,
			(vga_seqd[i] >> 3) & 1, (vga_seqd[i] >> 2) & 1, (vga_seqd[i] >> 1) & 1, (vga_seqd[i] >> 0) & 1);
	}
	debug_log(DEBUG_DETAIL, "\r\n");
	for (i = 0; i < 0x09; i++) {
		debug_log(DEBUG_DETAIL, "  GFX[0x%02X] = %u%u%u%u%u%u%u%u\r\n", i,
			(vga_gfxd[i] >> 7) & 1, (vga_gfxd[i] >> 6) & 1, (vga_gfxd[i] >> 5) & 1, (vga_gfxd[i] >> 4) & 1,
			(vga_gfxd[i] >> 3) & 1, (vga_gfxd[i] >> 2) & 1, (vga_gfxd[i] >> 1) & 1, (vga_gfxd[i] >> 0) & 1);
	}
	debug_log(DEBUG_DETAIL, "\r\n");
	for (i = 0; i < 0x19; i++) {
		debug_log(DEBUG_DETAIL, "  CRTC[0x%02X] = %u%u%u%u%u%u%u%u\r\n", i,
			(vga_crtcd[i] >> 7) & 1, (vga_crtcd[i] >> 6) & 1, (vga_crtcd[i] >> 5) & 1, (vga_crtcd[i] >> 4) & 1,
			(vga_crtcd[i] >> 3) & 1, (vga_crtcd[i] >> 2) & 1, (vga_crtcd[i] >> 1) & 1, (vga_crtcd[i] >> 0) & 1);
	}
	debug_log(DEBUG_DETAIL, "\r\n");
#endif
}
