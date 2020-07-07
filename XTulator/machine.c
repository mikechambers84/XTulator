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
	Machine definitions.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "config.h"
#include "debuglog.h"
#include "cpu/cpu.h"
#include "chipset/i8259.h"
#include "chipset/i8253.h"
#include "chipset/i8237.h"
#include "chipset/i8255.h"
#include "chipset/uart.h"
#include "modules/audio/pcspeaker.h"
#include "modules/audio/opl2.h"
#include "modules/audio/blaster.h"
#include "modules/disk/biosdisk.h"
#include "modules/disk/fdc.h"
#include "modules/input/input.h"
#include "modules/io/tcpmodem.h"
#include "modules/video/cga.h"
#include "modules/video/vga.h"
#include "rtc.h"
#include "memory.h"
#include "utility.h"
#include "machine.h"

/*
	ID string, full description, init function
*/
const MACHINEDEF_t machine_defs[] = {
	{ "generic_xt", "Generic XT clone with VGA, speed unlimited", machine_init_generic_xt, VIDEO_CARD_VGA, -1 },
	{ "ibm_xt", "IBM XT", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77 },
	{ "ami_xt", "AMI XT clone", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77 },
	{ "phoenix_xt", "Pheonix XT clone", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77 },
	{ "xi8088", "Xi 8088", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77 },
	{ "zenithss", "Zenith SuperSport 8088", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77 },
	{ "landmark", "Supersoft/Landmark diagnostic ROM", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77 },
	{ NULL }
};

const MACHINEMEM_t machine_mem[][10] = {
	//Generic XT clone
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xFE000, 0x02000, MACHINE_ROM_REQUIRED, "roms/machine/generic_xt/pcxtbios.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//IBM XT
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xF0000, 0x08000, MACHINE_ROM_REQUIRED, "roms/machine/ibm_xt/5000027.u19" },
		{ MACHINE_MEM_ROM, 0xF8000, 0x08000, MACHINE_ROM_REQUIRED, "roms/machine/ibm_xt/1501512.u18" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//AMI XT clone
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xFE000, 0x02000, MACHINE_ROM_REQUIRED, "roms/machine/ami_xt/ami_8088_bios_31jan89.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//Phoenix XT clone
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xFE000, 0x02000, MACHINE_ROM_REQUIRED, "roms/machine/phoenix_xt/000p001.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//Xi 8088
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/xi8088/bios128k-2.0.bin" }, //last half of this ROM is just filler for the 128k chip...
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//Zenith SuperSport 8088
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_RAM, 0xF0000, 0x04000, MACHINE_ROM_ISNOTROM, NULL }, //scratchpad RAM
		{ MACHINE_MEM_ROM, 0xF8000, 0x08000, MACHINE_ROM_REQUIRED, "roms/machine/zenithss/z184m v3.1d.10d" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//Supersoft/Landmark diagnostic
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
		{ MACHINE_MEM_ROM, 0xF8000, 0x08000, MACHINE_ROM_REQUIRED, "roms/machine/landmark/landmark.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},
};

extern double speed;

int machine_init_generic_xt(MACHINE_t* machine) {
	if (machine == NULL) return -1;

	i8259_init(&machine->i8259);
	i8253_init(&machine->i8253, &machine->i8259, &machine->pcspeaker);
	i8237_init(&machine->i8237, &machine->CPU);
	i8255_init(&machine->i8255, &machine->KeyState, &machine->pcspeaker);
	pcspeaker_init(&machine->pcspeaker);
	opl2_init(&machine->OPL2);
	blaster_init(&machine->blaster, &machine->i8237, &machine->i8259, 0x220, 1, 5);
	cpu_reset(&machine->CPU);
#ifndef USE_DISK_HLE
	fdc_init(&fdc, &machine->CPU, &i8259, &i8237);
	fdc_insert(&fdc, 0, "dos622.img");
#else
	biosdisk_init(&machine->CPU);
#endif
	rtc_init(&machine->CPU);

	switch (videocard) {
	case VIDEO_CARD_CGA:
		debug_log(DEBUG_INFO, "[MACHINE] Initializing CGA video device\r\n");
		if (cga_init()) return -1;
		break;
	case VIDEO_CARD_VGA:
		debug_log(DEBUG_INFO, "[MACHINE] Initializing VGA video device\r\n");
		if (vga_init()) return -1;
		break;
	}

	return 0;
}

int machine_init(MACHINE_t* machine, char* id) {
	int num = 0, match = 0, i = 0;

	do {
		if (machine_defs[num].id == NULL) {
			debug_log(DEBUG_ERROR, "[MACHINE] ERROR: Machine definition not found: %s\r\n", id);
			return -1;
		}

		if (_stricmp(id, machine_defs[num].id) == 0) {
			match = 1;
		}
		else {
			num++;
		}
	} while (!match);

	debug_log(DEBUG_INFO, "[MACHINE] Initializing machine: \"%s\" (%s)\r\n", machine_defs[num].description, machine_defs[num].id);

	//Initialize machine memory map
	while(1) {
		uint8_t* temp;
		if (machine_mem[num][i].memtype == MACHINE_MEM_ENDLIST) {
			break;
		}
		temp = (uint8_t*)malloc((size_t)machine_mem[num][i].size);
		if ((temp == NULL) &&
			((machine_mem[num][i].required == MACHINE_ROM_REQUIRED) || (machine_mem[num][i].required == MACHINE_ROM_ISNOTROM))) {
			debug_log(DEBUG_ERROR, "[MACHINE] ERROR: Unable to allocate %lu bytes of memory\r\n", machine_mem[num][i].size);
			return -1;
		}
		if (machine_mem[num][i].memtype == MACHINE_MEM_RAM) {
			memory_mapRegister(machine_mem[num][i].start, machine_mem[num][i].size, temp, temp);
		} else if (machine_mem[num][i].memtype == MACHINE_MEM_ROM) {
			int ret;
			ret = utility_loadFile(temp, machine_mem[num][i].size, machine_mem[num][i].filename);
			if ((machine_mem[num][i].required == MACHINE_ROM_REQUIRED) && ret) {
				debug_log(DEBUG_ERROR, "[MACHINE] Could not open file, or size is less than expected: %s\r\n", machine_mem[num][i].filename);
				return -1;
			}
			memory_mapRegister(machine_mem[num][i].start, machine_mem[num][i].size, temp, NULL);
		}
		i++;
	}

	if (videocard == 0xFF) {
		videocard = machine_defs[num].video;
	}

	if (speedarg > 0) {
		speed = speedarg;
	} else if (speedarg < 0) {
		speed = -1;
	} else {
		speed = machine_defs[num].speed;
	}

	if ((*machine_defs[num].init)(machine)) { //call machine-specific init routine
		return -1;
	}

	return num;
}

void machine_list() {
	int machine = 0;

	printf("Valid " STR_TITLE " machines:\r\n");

	while(machine_defs[machine].id != NULL) {
		printf("%s: \"%s\"\r\n", machine_defs[machine].id, machine_defs[machine].description);
		machine++;
	}
}
