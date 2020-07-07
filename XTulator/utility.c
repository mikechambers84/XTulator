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
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <errno.h>
#endif
#include "config.h"
#include "memory.h"
#include "debuglog.h"

int utility_loadFile(uint8_t* dst, size_t len, char* srcfile) {
	FILE* file;
	if (dst == NULL) {
		return -1;
	}

	file = fopen(srcfile, "rb");
	if (file == NULL) {
		//debug_log(DEBUG_ERROR, "[ERROR] utility_loadFile: Unable to open %s\r\n", srcfile);
		free(dst);
		return -1;
	}
	if (fread(dst, 1, len, file) < len) {
		//debug_log(DEBUG_ERROR, "[ERROR] utility_loadFile: %s input was less than %lu bytes\r\n", srcfile, (uint32_t)len);
		free(dst);
		return -1;
	}
	fclose(file);
	return 0;
}

void utility_sleep(uint32_t ms) {
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	int res;
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = (long)ms * 1000;
	do {
		res = nanosleep(&ts, &ts);
	} while (res && errno == EINTR);
#endif
}
