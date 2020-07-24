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
	DS12885 CMOS/RTC chip
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../config.h"
#include "../../ports.h"
#include "../../debuglog.h"
#include "../../machine.h"
#include "../../chipset/i8259.h"

MACHINE_t* nvr_useMachine = NULL;
uint8_t nvr_RAM[128], nvr_addr = 0;

#ifdef _WIN32

#include <Windows.h>

void nvr_write(void* dummy, uint16_t addr, uint8_t value) {
	debug_log(DEBUG_INFO, "[NVRAM] Write %03X <- %02X\r\n", addr, value);
	addr &= 1;
	if (addr == 0) {
		nvr_addr = value;
	}
	else {
		switch (nvr_addr & 0x7F) {
		case 0x0C:
		case 0x0D:
			break;
		default:
			nvr_RAM[nvr_addr & 0x7F] = value;
			break;
		}

		if (!(nvr_addr & 0x80)) {
			i8259_doirq(&nvr_useMachine->i8259b, 0);
		}
	}
}

uint8_t nvr_read(void* dummy, uint16_t addr) {
	uint8_t ret = 0xFF;
	SYSTEMTIME tdata;

	debug_log(DEBUG_INFO, "[NVRAM] Read %03X\r\n", addr);

	if (addr & 1) {
		if ((nvr_addr & 0x7F) < 0x0A) {
			GetLocalTime(&tdata);
			ret = 0x01;
		}
		else {
			ret = nvr_RAM[nvr_addr & 0x7F];
			switch (nvr_addr & 0x7F) {
			case 0x00:
				break;
			case 0x0C:
				nvr_RAM[0x0C] = 0;
				break;
			case 0x0D:
				ret = 0x80;
				break;
			}
		}
	}

	return ret;
}

#else

#include <time.h>

uint8_t rtc_read(void* dummy, uint16_t addr) {
	uint8_t ret = 0xFF;
	struct tm tdata;

	time(&tdata);

	addr &= 0x1F;
	switch (addr) {
	case 1:
		ret = 0;
		break;
	case 2:
		ret = (uint8_t)tdata.tm_sec;
		break;
	case 3:
		ret = (uint8_t)tdata.tm_min;
		break;
	case 4:
		ret = (uint8_t)tdata.tm_hour;
		break;
	case 5:
		ret = (uint8_t)tdata.tm_wday;
		break;
	case 6:
		ret = (uint8_t)tdata.tm_mday;
		break;
	case 7:
		ret = (uint8_t)tdata.tm_mon;
		break;
	case 9:
		ret = (uint8_t)tdata.tm_year % 100;
		break;
	}

	if (ret != 0xFF) {
		uint8_t rh, rl;
		rh = (ret / 10) % 10;
		rl = ret % 10;
		ret = (rh << 4) | rl;
	}

	return ret;
}

#endif

void nvr_init(MACHINE_t* machine) {
	debug_log(DEBUG_INFO, "[NVR] Initializing DS12885 CMOS/RTC\r\n");
	nvr_useMachine = machine;
	ports_cbRegister(0x70, 16, (void*)nvr_read, NULL, (void*)nvr_write, NULL, NULL);
	memset(nvr_RAM, 0, 128);
}
