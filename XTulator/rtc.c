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
	Generic RTC interface for XT systems, works with TIMER.COM version 1.2
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "ports.h"
#include "debuglog.h"

#ifdef _WIN32

#include <Windows.h>

uint8_t rtc_read(void* dummy, uint16_t addr) {
	uint8_t ret = 0xFF;
	SYSTEMTIME tdata;

	GetLocalTime(&tdata);

	addr &= 0x1F;
	switch (addr) {
	case 1:
		ret = (uint8_t)tdata.wMilliseconds / 10;
		break;
	case 2:
		ret = (uint8_t)tdata.wSecond;
		break;
	case 3:
		ret = (uint8_t)tdata.wMinute;
		break;
	case 4:
		ret = (uint8_t)tdata.wHour;
		break;
	case 5:
		ret = (uint8_t)tdata.wDayOfWeek;
		break;
	case 6:
		ret = (uint8_t)tdata.wDay;
		break;
	case 7:
		ret = (uint8_t)tdata.wMonth;
		break;
	case 9:
		ret = (uint8_t)tdata.wYear % 100;
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

void rtc_write(void* dummy, uint16_t addr, uint8_t value) {

}

void rtc_init() {
	debug_log(DEBUG_INFO, "[RTC] Initializing real time clock\r\n");
	ports_cbRegister(0x240, 0x18, (void*)rtc_read, NULL, (void*)rtc_write, NULL, NULL);
}
