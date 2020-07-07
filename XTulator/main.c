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
#include <string.h>
#include "config.h"
#include "args.h"
#include "timing.h"
#include "memory.h"
#include "ports.h"
#include "machine.h"
#include "debuglog.h"
#include "cpu/cpu.h"
#include "chipset/i8259.h"
#include "modules/disk/biosdisk.h"
#include "modules/video/sdlconsole.h"
#include "modules/audio/sdlaudio.h"

char* usemachine = "generic_xt"; //default

char title[64]; //assuming 64 isn't safe if somebody starts messing with STR_TITLE and STR_VERSION

uint64_t ops = 0;
uint32_t baudrate = 115200, ramsize = 640, instructionsperloop = 100;
uint8_t goCPU = 1, limitCPU = 0, showMIPS = 0;
uint8_t videocard = 0xFF;
double speed = 0;

volatile uint8_t running = 1;

MACHINE_t machine;

void optimer(void* dummy) {
	ops /= 10000;
	if (showMIPS) {
		debug_log(DEBUG_INFO, "%llu.%llu MIPS          \r", ops / 10, ops % 10);
	}
	ops = 0;
}

void cputimer(void* dummy) {
	goCPU = 1;
}

int main(int argc, char *argv[]) {

	sprintf(title, "%s v%s pre alpha", STR_TITLE, STR_VERSION);

	printf("%s (c)2020 Mike Chambers\r\n", title);
	printf("[A portable, open source 80186 PC emulator]\r\n\r\n");

	ports_init();
	timing_init();
	memory_init();

	if (args_parse(&machine, argc, argv)) {
		return -1;
	}

	if (sdlconsole_init(title)) {
		debug_log(DEBUG_ERROR, "[ERROR] SDL initialization failure\r\n");
		return -1;
	}

	if (sdlaudio_init(&machine)) {
		debug_log(DEBUG_INFO, "[WARNING] SDL audio initialization failure\r\n");
	}

	if (machine_init(&machine, usemachine) < 0) {
		debug_log(DEBUG_ERROR, "[ERROR] Machine initialization failure\r\n");
		return -1;
	}

	if (bootdrive == 0xFF) {
		if (biosdisk[2].inserted) {
			bootdrive = 0x80;
		}
		else if (biosdisk[0].inserted) {
			bootdrive = 0x00;
		}
	}

	timing_addTimer(optimer, NULL, 10, TIMING_ENABLED);
	if (speed > 0) {
		instructionsperloop = (uint32_t)((speed * 1000000.0) / 140000.0);
		limitCPU = 1;
		debug_log(DEBUG_INFO, "[MACHINE] Throttling speed to approximately a %.02f MHz 8088 (%lu instructions/sec)\r\n", speed, instructionsperloop * 10000);
		timing_addTimer(cputimer, NULL, 10000, TIMING_ENABLED);
	}
	while (running) {
		static uint32_t curloop = 0;
		if (limitCPU == 0) {
			goCPU = 1;
		}
		if (goCPU) {
			cpu_interruptCheck(&machine.CPU, &machine.i8259);
			cpu_exec(&machine.CPU, instructionsperloop);
			ops += instructionsperloop;
			goCPU = 0;
		}
		timing_loop();
		if (++curloop == 100) { //don't do this too often, it seems to be expensive
			switch (sdlconsole_loop()) {
			case SDLCONSOLE_EVENT_KEY:
				machine.KeyState.scancode = sdlconsole_getScancode();
				machine.KeyState.isNew = 1;
				i8259_doirq(&machine.i8259, 1);
				break;
			case SDLCONSOLE_EVENT_QUIT:
				running = 0;
				break;
			case SDLCONSOLE_EVENT_DEBUG_1:
				if (speed > 0) {
					speed *= 0.9;
					instructionsperloop = (uint32_t)((speed * 1000000.0) / 140000.0);
					debug_log(DEBUG_INFO, "[MACHINE] Throttling speed to approximately a %.02f MHz 8088 (%lu instructions/sec)\r\n", speed, instructionsperloop * 10000);
				}
				break;
			case SDLCONSOLE_EVENT_DEBUG_2:
				if (speed > 0) {
					speed *= 1.1;
					instructionsperloop = (uint32_t)((speed * 1000000.0) / 140000.0);
					debug_log(DEBUG_INFO, "[MACHINE] Throttling speed to approximately a %.02f MHz 8088 (%lu instructions/sec)\r\n", speed, instructionsperloop * 10000);
				}
				break;
			}
			curloop = 0;
		}
	}

	return 0;
}
