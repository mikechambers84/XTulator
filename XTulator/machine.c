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
#include "modules/input/mouse.h"
#include "modules/input/input.h"
#ifdef USE_NE2000
#include "modules/io/ne2000.h"
#include "modules/io/pcap-win32.h"
#endif
#include "modules/io/tcpmodem.h"
#include "modules/video/cga.h"
#include "modules/video/vga.h"
#include "rtc.h"
#include "modules/misc/ds12885.h"
#include "memory.h"
#include "utility.h"
#include "timing.h"
#include "machine.h"

/*
	ID string, full description, init function, default video, speed in MHz (-1 = unlimited), default hardware flags
*/
const MACHINEDEF_t machine_defs[] = {
	{ "generic_xt", "Generic XT clone with VGA, speed unlimited", machine_init_generic_xt, VIDEO_CARD_VGA, -1, MACHINE_HW_BLASTER | MACHINE_HW_UART1_MOUSE | MACHINE_HW_DISK_HLE | MACHINE_HW_RTC },
	{ "ibm_xt", "IBM XT", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "ami_xt", "AMI XT clone", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "phoenix_xt", "Pheonix XT clone", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "xi8088", "Xi 8088", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "zenithss", "Zenith SuperSport 8088", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "landmark", "Supersoft/Landmark diagnostic ROM", machine_init_generic_xt, VIDEO_CARD_CGA, 4.77, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "ami_286", "AMI 286 clone", machine_init_generic_286, VIDEO_CARD_VGA, 50.0, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "award_286", "Award 286 clone", machine_init_generic_286, VIDEO_CARD_VGA, 50.0, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	{ "gw286ct", "gw286ct", machine_init_generic_286, VIDEO_CARD_VGA, 50.0, MACHINE_HW_UART1_MOUSE | MACHINE_HW_RTC },
	//tests
	{ "test_div", "Test ROM - Division", machine_init_test, VIDEO_CARD_CGA, -1, MACHINE_HW_SKIP_CHIPSET },
	{ "test_control", "Test ROM - Control", machine_init_test, VIDEO_CARD_CGA, -1, MACHINE_HW_SKIP_CHIPSET },
	{ "test_add", "Test ROM - Addition", machine_init_test, VIDEO_CARD_CGA, -1, MACHINE_HW_SKIP_CHIPSET },
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

	//AMI 286 clone
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/ami_286/AMIC206.BIN" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//Award 286 clone
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/award_286/award.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//gw286ct
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
#ifndef USE_DISK_HLE
		{ MACHINE_MEM_ROM, 0xD0000, 0x02000, MACHINE_ROM_REQUIRED, "roms/disk/ide_xt.bin" },
#endif
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/gw286ct/2ctc001.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//div test
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/tests/div.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//control test
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/tests/control.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

	//addition test
	{
		{ MACHINE_MEM_RAM, 0x00000, 0xA0000, MACHINE_ROM_ISNOTROM, NULL },
		{ MACHINE_MEM_ROM, 0xF0000, 0x10000, MACHINE_ROM_REQUIRED, "roms/machine/tests/add.bin" },
		{ MACHINE_MEM_ENDLIST, 0, 0, 0, NULL }
	},

};

uint8_t mac[6] = { 0xac, 0xde, 0x48, 0x88, 0xbb, 0xab };

int machine_init_generic_xt(MACHINE_t* machine) {
	if (machine == NULL) return -1;

	if ((machine->hwflags & MACHINE_HW_SKIP_CHIPSET) == 0) {
		i8259_init(&machine->i8259, 0);
		i8253_init(&machine->i8253, &machine->i8259, &machine->pcspeaker);
		i8237_init(&machine->i8237, &machine->CPU);
		i8255_init(&machine->i8255, &machine->KeyState, &machine->pcspeaker);
		pcspeaker_init(&machine->pcspeaker);
	}

	//check machine HW flags and init devices accordingly
	if ((machine->hwflags & MACHINE_HW_BLASTER) && !(machine->hwflags & MACHINE_HW_SKIP_BLASTER)) {
		blaster_init(&machine->blaster, &machine->i8237, &machine->i8259, 0x220, 1, 5);
		OPL3_init(&machine->OPL3);
		machine->mixBlaster = 1;
		machine->mixOPL = 1;
	}
	else if ((machine->hwflags & MACHINE_HW_OPL) && !(machine->hwflags & MACHINE_HW_SKIP_OPL)) { //else if because some games won't detect an SB without seeing the OPL, so if SB enabled then OPL already is
		//opl2_init(&machine->OPL2);
		OPL3_init(&machine->OPL3);
		machine->mixOPL = 1;
	}
	if ((machine->hwflags & MACHINE_HW_RTC) && !(machine->hwflags & MACHINE_HW_SKIP_RTC)) {
		rtc_init();
	}

	if ((machine->hwflags & MACHINE_HW_UART0_NONE) && !(machine->hwflags & MACHINE_HW_SKIP_UART0)) {
		uart_init(&machine->UART[0], &machine->i8259, 0x3F8, 4, NULL, NULL, NULL, NULL);
	}
	else if ((machine->hwflags & MACHINE_HW_UART0_MOUSE) && !(machine->hwflags & MACHINE_HW_SKIP_UART0)) {
		uart_init(&machine->UART[0], &machine->i8259, 0x3F8, 4, NULL, NULL, (void*)mouse_togglereset, NULL);
		mouse_init(&machine->UART[0]);
		timing_addTimer(mouse_rxpoll, NULL, baudrate / 9, TIMING_ENABLED);
	}
#ifdef ENABLE_TCP_MODEM
	else if ((machine->hwflags & MACHINE_HW_UART0_TCPMODEM) && !(machine->hwflags & MACHINE_HW_SKIP_UART0)) {
		uart_init(&machine->UART[0], &machine->i8259, 0x3F8, 4, (void*)tcpmodem_tx, &machine->tcpmodem[0], NULL, NULL);
		tcpmodem_init(&machine->tcpmodem[0], &machine->UART[0], 23);
		timing_addTimer(tcpmodem_rxpoll, &machine->tcpmodem[0], baudrate / 9, TIMING_ENABLED);
	}
#endif

	if ((machine->hwflags & MACHINE_HW_UART1_NONE) && !(machine->hwflags & MACHINE_HW_SKIP_UART1)) {
		uart_init(&machine->UART[1], &machine->i8259, 0x2F8, 3, NULL, NULL, NULL, NULL);
	}
	else if ((machine->hwflags & MACHINE_HW_UART1_MOUSE) && !(machine->hwflags & MACHINE_HW_SKIP_UART1)) {
		uart_init(&machine->UART[1], &machine->i8259, 0x2F8, 3, NULL, NULL, (void*)mouse_togglereset, NULL);
		mouse_init(&machine->UART[1]);
		timing_addTimer(mouse_rxpoll, NULL, baudrate / 9, TIMING_ENABLED);
	}
#ifdef ENABLE_TCP_MODEM
	else if ((machine->hwflags & MACHINE_HW_UART1_TCPMODEM) && !(machine->hwflags & MACHINE_HW_SKIP_UART1)) {
		uart_init(&machine->UART[1], &machine->i8259, 0x2F8, 3, (void*)tcpmodem_tx, &machine->tcpmodem[1], NULL, NULL);
		tcpmodem_init(&machine->tcpmodem[1], &machine->UART[1], 23);
		timing_addTimer(tcpmodem_rxpoll, &machine->tcpmodem[1], baudrate / 9, TIMING_ENABLED);
	}
#endif

#ifdef USE_NE2000
	if (machine->hwflags & MACHINE_HW_NE2000) {
		ne2000_init(&machine->ne2000, &machine->i8259, 0x300, 2, (uint8_t*)&mac);
		if (machine->pcap_if > -1) {
			if (pcap_init(&machine->ne2000, machine->pcap_if)) {
				return -1;
			}
		}
	}
#endif

	cpu_reset(&machine->CPU);
#ifndef USE_DISK_HLE
	fdc_init(&fdc, &machine->CPU, &i8259, &i8237);
	fdc_insert(&fdc, 0, "dos622.img");
#else
	biosdisk_init(&machine->CPU);
#endif

	switch (videocard) {
	case VIDEO_CARD_CGA:
		if (cga_init()) return -1;
		break;
	case VIDEO_CARD_VGA:
		if (vga_init()) return -1;
		break;
	}

	return 0;
}

int machine_init_generic_286(MACHINE_t* machine) {
	if (machine == NULL) return -1;

	if ((machine->hwflags & MACHINE_HW_SKIP_CHIPSET) == 0) {
		i8259_init(&machine->i8259, 0);
		i8259_init(&machine->i8259b, 1);
		i8253_init(&machine->i8253, &machine->i8259, &machine->pcspeaker);
		i8237_init(&machine->i8237, &machine->CPU);
		i8255_init(&machine->i8255, &machine->KeyState, &machine->pcspeaker);
		pcspeaker_init(&machine->pcspeaker);
	}

	//check machine HW flags and init devices accordingly
	if ((machine->hwflags & MACHINE_HW_BLASTER) && !(machine->hwflags & MACHINE_HW_SKIP_BLASTER)) {
		blaster_init(&machine->blaster, &machine->i8237, &machine->i8259, 0x220, 1, 5);
		OPL3_init(&machine->OPL3);
		machine->mixBlaster = 1;
		machine->mixOPL = 1;
	}
	else if ((machine->hwflags & MACHINE_HW_OPL) && !(machine->hwflags & MACHINE_HW_SKIP_OPL)) { //else if because some games won't detect an SB without seeing the OPL, so if SB enabled then OPL already is
		//opl2_init(&machine->OPL2);
		OPL3_init(&machine->OPL3);
		machine->mixOPL = 1;
	}

	//nvr_init(machine);

	if ((machine->hwflags & MACHINE_HW_UART0_NONE) && !(machine->hwflags & MACHINE_HW_SKIP_UART0)) {
		uart_init(&machine->UART[0], &machine->i8259, 0x3F8, 4, NULL, NULL, NULL, NULL);
	}
	else if ((machine->hwflags & MACHINE_HW_UART0_MOUSE) && !(machine->hwflags & MACHINE_HW_SKIP_UART0)) {
		uart_init(&machine->UART[0], &machine->i8259, 0x3F8, 4, NULL, NULL, (void*)mouse_togglereset, NULL);
		mouse_init(&machine->UART[0]);
		timing_addTimer(mouse_rxpoll, NULL, baudrate / 9, TIMING_ENABLED);
	}
#ifdef ENABLE_TCP_MODEM
	else if ((machine->hwflags & MACHINE_HW_UART0_TCPMODEM) && !(machine->hwflags & MACHINE_HW_SKIP_UART0)) {
		uart_init(&machine->UART[0], &machine->i8259, 0x3F8, 4, (void*)tcpmodem_tx, &machine->tcpmodem[0], NULL, NULL);
		tcpmodem_init(&machine->tcpmodem[0], &machine->UART[0], 23);
		timing_addTimer(tcpmodem_rxpoll, &machine->tcpmodem[0], baudrate / 9, TIMING_ENABLED);
	}
#endif

	if ((machine->hwflags & MACHINE_HW_UART1_NONE) && !(machine->hwflags & MACHINE_HW_SKIP_UART1)) {
		uart_init(&machine->UART[1], &machine->i8259, 0x2F8, 3, NULL, NULL, NULL, NULL);
	}
	else if ((machine->hwflags & MACHINE_HW_UART1_MOUSE) && !(machine->hwflags & MACHINE_HW_SKIP_UART1)) {
		uart_init(&machine->UART[1], &machine->i8259, 0x2F8, 3, NULL, NULL, (void*)mouse_togglereset, NULL);
		mouse_init(&machine->UART[1]);
		timing_addTimer(mouse_rxpoll, NULL, baudrate / 9, TIMING_ENABLED);
	}
#ifdef ENABLE_TCP_MODEM
	else if ((machine->hwflags & MACHINE_HW_UART1_TCPMODEM) && !(machine->hwflags & MACHINE_HW_SKIP_UART1)) {
		uart_init(&machine->UART[1], &machine->i8259, 0x2F8, 3, (void*)tcpmodem_tx, &machine->tcpmodem[1], NULL, NULL);
		tcpmodem_init(&machine->tcpmodem[1], &machine->UART[1], 23);
		timing_addTimer(tcpmodem_rxpoll, &machine->tcpmodem[1], baudrate / 9, TIMING_ENABLED);
	}
#endif

#ifdef USE_NE2000
	if (machine->hwflags & MACHINE_HW_NE2000) {
		ne2000_init(&machine->ne2000, &machine->i8259, 0x300, 2, (uint8_t*)&mac);
		if (machine->pcap_if > -1) {
			if (pcap_init(&machine->ne2000, machine->pcap_if)) {
				return -1;
			}
		}
	}
#endif

	cpu_reset(&machine->CPU);
#ifndef USE_DISK_HLE
	fdc_init(&fdc, &machine->CPU, &i8259, &i8237);
	fdc_insert(&fdc, 0, "dos622.img");
#else
	biosdisk_init(&machine->CPU);
#endif

	switch (videocard) {
	case VIDEO_CARD_CGA:
		if (cga_init()) return -1;
		break;
	case VIDEO_CARD_VGA:
		if (vga_init()) return -1;
		break;
	}

	return 0;
}

int machine_init_test(MACHINE_t* machine) {
	cpu_reset(&machine->CPU);

	switch (videocard) {
	case VIDEO_CARD_CGA:
		if (cga_init()) return -1;
		break;
	case VIDEO_CARD_VGA:
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

	machine->hwflags |= machine_defs[num].hwflags;

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
