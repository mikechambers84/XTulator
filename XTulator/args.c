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
#include <string.h>
#include <stdint.h>
#include "config.h"
#include "timing.h"
#include "machine.h"
#include "cpu/cpu.h"
#include "chipset/i8259.h"
#include "chipset/i8253.h"
#include "chipset/i8237.h"
#include "chipset/i8255.h"
#include "chipset/uart.h"
#ifdef ENABLE_TCP_MODEM
#include "modules/io/tcpmodem.h"
#endif
#ifdef USE_NE2000
#include "modules/io/pcap-win32.h"
#endif
#include "modules/input/mouse.h"
#include "modules/disk/biosdisk.h"
#include "modules/audio/pcspeaker.h"
#include "modules/audio/opl2.h"
#include "modules/audio/blaster.h"
#include "modules/video/cga.h"
#include "modules/video/vga.h"
#include "debuglog.h"

double speedarg = 0;

int args_isMatch(char* s1, char* s2) {
	int i = 0, match = 1;

	while (1) {
		char c1, c2;
		c1 = s1[i];
		c2 = s2[i++];
		if ((c1 >= 'A') && (c1 <= 'Z')) {
			c1 -= 'A' - 'a';
		}
		if ((c2 >= 'A') && (c2 <= 'Z')) {
			c2 -= 'A' - 'a';
		}
		if (c1 != c2) {
			match = 0;
			break;
		}
		if (!c1 || !c2) {
			break;
		}
	}

	return match;
}

void args_showHelp() {
	printf(STR_TITLE " command line parameters:\r\n\r\n");

	printf("Machine options:\r\n");
	printf("  -machine <id>          Emulate machine definition defined by <id>. (Default is generic_xt)\r\n");
	printf("                         Use -machine list to display <id> options.\r\n");
	printf("  -speed <mhz>           Run the emulated CPU at approximately <mhz> MHz. (Default is as fast as possible)\r\n");
	printf("                         There is currently no clock ticks counted per instruction, so the emulator is just going\r\n");
	printf("                         to estimate how many instructions would come out to approximately the desired speed.\r\n");
	printf("                         There will be more accurate speed-throttling at some point in the future.\r\n\r\n");

	printf("Disk options:\r\n");
	printf("  -fd0 <file>            Insert <file> disk image as floppy 0.\r\n");
	printf("  -fd1 <file>            Insert <file> disk image as floppy 1.\r\n");
	printf("  -hd0 <file>            Insert <file> disk image as hard disk 0.\r\n");
	printf("  -hd1 <file>            Insert <file> disk image as hard disk 1.\r\n");
	printf("  -boot <disk>           Use <disk> (fd0, fd1, hd0 or hd1) as boot disk.\r\n\r\n");

	printf("Video options:\r\n");
	printf("  -video <type>          Use <type> (CGA or VGA) video card emulation. (Default is machine-dependent)\r\n");
	printf("  -fpslock <FPS>         Attempt to lock video refresh to <FPS> frames per second.\r\n");
	printf("                         (Default is to base FPS on video adapter timings and is dynamic)\r\n\r\n");

	printf("Serial options:\r\n");
#ifdef ENABLE_TCP_MODEM
	printf("  -baud <value>          Use <value> as baud rate for TCP modem emulation. (Default is 115200)\r\n");
	printf("                         Valid range is from 300 to 115200.\r\n");
#endif
	printf("  -uart0 <type> [port]   Emulate 8250 UART on 3F8h, IRQ 4 (COM1) and connect it to <type> device.\r\n");
	printf("                         Specify [port] to listen on if using tcpmodem type.\r\n");
	printf("  -uart1 <type> [port]   Emulate 8250 UART on 2F8h, IRQ 3 (COM2) and connect it to <type> device.\r\n");
	printf("                         Specify [port] to listen on if using tcpmodem type.\r\n\r\n");

#ifdef ENABLE_TCP_MODEM
	printf("Valid <type> values for -uart0 and -uart1 are currently: none, mouse, tcpmodem\r\n\r\n");
#else
	printf("Valid <type> values for -uart0 and -uart1 are currently: none, mouse\r\n\r\n");
#endif
	printf("none provides a visible serial port to the system, but with nothing attached to it.\r\n\r\n");
	
	printf("tcpmodem attaches a somewhat Hayes-compatible modem to the system, and simulates phone line\r\n");
	printf("connections using TCP sockets instead. For example, you could use these modem commands from a terminal:\r\n\r\n");

	printf("ATDTbbs.example.com      - This will \"dial\" bbs.example.com\r\n");
	printf("ATDTbbs.example.com:45   - This will \"dial\" bbs.example.com on port 45 instead of the default 23.\r\n\r\n");

	printf("The tcpmodem devices will also listen for incoming connections, by default on port 23. If there is a\r\n");
	printf("connection, it will provide a RING notification both as text and through the modem status register bit.\r\n");
	printf("If you are using two tcpmodem devices, you will need to specify an alternate listen port for one of them.\r\n\r\n");

#ifdef USE_NE2000
	printf("Networking options:\r\n");
	printf("  -net <id>              Initialize emulated NE2000 adapter using physical interface number specified\r\n");
	printf("                         by <id>. Use \"-net list\" to display available interfaces. NE2000 will be\r\n");
	printf("                         available to guest system at base port 0x300, IRQ 2.\r\n\r\n");
#endif

	printf("Miscellaneous options:\r\n");
	printf("  -mem <size>            Initialize emulator with only <size> KB of base memory. (Default is 640)\r\n");
	printf("                         The maximum size is 736 KB, but this can only work with CGA video and a\r\n");
	printf("                         system BIOS that will test beyond 640 KB.\r\n");
	printf("  -debug <level>         <level> can be: NONE, ERROR, INFO, DETAIL. (Default is INFO)\r\n");
	printf("  -mips                  Display live MIPS being emulated.\r\n");
	printf("  -h                     Show this help screen.\r\n");
}

int args_parse(MACHINE_t* machine, int argc, char* argv[]) {
	int i;

#ifndef _WIN32
	if (argc < 2) {
		printf("Specify command line parameters. Use -h for help.\r\n");
		return -1;
	}
#endif

	for (i = 1; i < argc; i++) {
		if (args_isMatch(argv[i], "-h")) {
			args_showHelp();
			return -1;
		}
		else if (args_isMatch(argv[i], "-machine")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -machine. Use -h for help.\r\n");
				return -1;
			}
			i++;
			if (args_isMatch(argv[i], "list")) {
				machine_list();
				return -1;
			}
			usemachine = argv[i];
		}
		else if (args_isMatch(argv[i], "-speed")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -speed. Use -h for help.\r\n");
				return -1;
			}
			speedarg = atof(argv[++i]);
		}
		else if (args_isMatch(argv[i], "-fd0")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -fd0. Use -h for help.\r\n");
				return -1;
			}
			biosdisk_insert(&machine->CPU, 0, argv[++i]);
		}
		else if (args_isMatch(argv[i], "-fd1")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -fd1. Use -h for help.\r\n");
				return -1;
			}
			biosdisk_insert(&machine->CPU, 1, argv[++i]);
		}
		else if (args_isMatch(argv[i], "-hd0")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -hd0. Use -h for help.\r\n");
				return -1;
			}
			biosdisk_insert(&machine->CPU, 2, argv[++i]);
		}
		else if (args_isMatch(argv[i], "-hd1")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -hd1. Use -h for help.\r\n");
				return -1;
			}
			biosdisk_insert(&machine->CPU, 3, argv[++i]);
		}
		else if (args_isMatch(argv[i], "-boot")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -boot. Use -h for help.\r\n");
				return -1;
			}
			if (args_isMatch(argv[i + 1], "fd0")) bootdrive = 0x00;
			else if (args_isMatch(argv[i + 1], "fd1")) bootdrive = 0x01;
			else if (args_isMatch(argv[i + 1], "hd0")) bootdrive = 0x80;
			else if (args_isMatch(argv[i + 1], "hd1")) bootdrive = 0x81;
			else {
				printf("%s is an invalid boot option\r\n", argv[i + 1]);
				return -1;
			}
			i++;
		}
		else if (args_isMatch(argv[i], "-video")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -video. Use -h for help.\r\n");
				return -1;
			}
			if (args_isMatch(argv[i + 1], "vga")) videocard = VIDEO_CARD_VGA;
			else if (args_isMatch(argv[i + 1], "cga")) videocard = VIDEO_CARD_CGA;
			else {
				printf("%s is an invalid video card option\r\n", argv[i + 1]);
				return -1;
			}
			i++;
		}
		else if (args_isMatch(argv[i], "-fpslock")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -fpslock. Use -h for help.\r\n");
				return -1;
			}
			vga_lockFPS = atof(argv[++i]);
			if ((vga_lockFPS < 1) || (vga_lockFPS > 144)) {
				printf("%f is an invalid FPS option, valid range is 1 to 144\r\n", vga_lockFPS);
				return -1;
			}
		}
		else if (args_isMatch(argv[i], "-mem")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -mem. Use -h for help.\r\n");
				return -1;
			}
			ramsize = atol(argv[++i]);
			if (ramsize > 736) {
				printf("The limit for base memory is 736 KB.\r\n");
				return -1;
			}
		}
		else if (args_isMatch(argv[i], "-debug")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -debug. Use -h for help.\r\n");
				return -1;
			}
			if (args_isMatch(argv[i + 1], "none")) debug_setLevel(DEBUG_NONE);
			else if (args_isMatch(argv[i + 1], "error")) debug_setLevel(DEBUG_ERROR);
			else if (args_isMatch(argv[i + 1], "info")) debug_setLevel(DEBUG_INFO);
			else if (args_isMatch(argv[i + 1], "detail")) debug_setLevel(DEBUG_DETAIL);
			else {
				printf("%s is an invalid debug option\r\n", argv[i + 1]);
				return -1;
			}
			i++;
		}
		else if (args_isMatch(argv[i], "-mips")) {
			showMIPS = 1;
		}
		else if (args_isMatch(argv[i], "-baud")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -baud. Use -h for help.\r\n");
				return -1;
			}
			baudrate = atol(argv[++i]);
			if ((baudrate < 300) | (baudrate > 115200)) {
				printf("Baud rate must be between 300 and 115200.\r\n");
				return -1;
			}
		}
		else if (args_isMatch(argv[i], "-uart0") || args_isMatch(argv[i], "-uart1")) {
			uint16_t port, base;
			uint8_t irq, uartnum;
			uartnum = argv[i][5] - '0';
			if (uartnum == 0) {
				base = 0x3F8;
				irq = 4;
			} else {
				base = 0x2F8;
				irq = 3;
			}
			if ((i + 1) == argc) {
				printf("Parameter required for -uart%u. Use -h for help.\r\n", uartnum);
				return -1;
			}
			machine->hwflags |= uartnum ? MACHINE_HW_SKIP_UART1 : MACHINE_HW_SKIP_UART0;
#ifdef ENABLE_TCP_MODEM
			if (args_isMatch(argv[i + 1], "tcpmodem")) {
				i++;
				if ((i + 1) == argc) {
					port = 23;
				}
				else if (argv[i + 1][0] == '-') {
					port = 23;
				}
				else {
					port = (uint16_t)atol(argv[++i]);
				}
				uart_init(&machine->UART[uartnum], &machine->i8259, base, irq, (void*)tcpmodem_tx, &machine->tcpmodem[uartnum], NULL, NULL);
				tcpmodem_init(&machine->tcpmodem[uartnum], &machine->UART[uartnum], port);
				timing_addTimer(tcpmodem_rxpoll, &machine->tcpmodem[uartnum], baudrate / 9, TIMING_ENABLED);
			} else
#endif
				if (args_isMatch(argv[i + 1], "mouse")) {
				i++;
				uart_init(&machine->UART[uartnum], &machine->i8259, base, irq, NULL, NULL, (void*)mouse_togglereset, NULL);
				mouse_init(&machine->UART[uartnum]);
				timing_addTimer(mouse_rxpoll, NULL, baudrate / 9, TIMING_ENABLED);
			}
			else if (args_isMatch(argv[i + 1], "none")) {
				i++;
				uart_init(&machine->UART[uartnum], &machine->i8259, base, irq, NULL, NULL, NULL, NULL);
			}
			else {
				printf("%s is not a valid parameter for -uart%u. Use -h for help.\r\n", argv[i + 1], uartnum);
				return -1;
			}
		}
		else if (args_isMatch(argv[i], "-hw")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -hw. Use -h for help.\r\n");
				return -1;
			}
			if (args_isMatch(argv[i + 1], "opl")) {
				machine->hwflags |= MACHINE_HW_OPL;
			}
			else if (args_isMatch(argv[i + 1], "noopl")) {
				machine->hwflags |= MACHINE_HW_SKIP_OPL;
			}
			else if (args_isMatch(argv[i + 1], "blaster")) {
				machine->hwflags |= MACHINE_HW_BLASTER;
			}
			else if (args_isMatch(argv[i + 1], "noblaster")) {
				machine->hwflags |= MACHINE_HW_SKIP_BLASTER;
			}
			else if (args_isMatch(argv[i + 1], "rtc")) {
				machine->hwflags |= MACHINE_HW_RTC;
			}
			else if (args_isMatch(argv[i + 1], "nortc")) {
				machine->hwflags |= MACHINE_HW_SKIP_RTC;
			}
			else {
				printf("%s is an invalid hardware option\r\n", argv[i + 1]);
				return -1;
			}
			i++;
		}
#ifdef USE_NE2000
		else if (args_isMatch(argv[i], "-net")) {
			if ((i + 1) == argc) {
				printf("Parameter required for -list. Use -h for help.\r\n");
				return -1;
			}
			i++;
			if (args_isMatch(argv[i], "list")) {
				pcap_listdevs();
				return -1;
			}
			machine->pcap_if = atoi(argv[i]);
			machine->hwflags |= MACHINE_HW_NE2000;
		}
#endif
		else {
			printf("%s is not a valid parameter. Use -h for help.\r\n", argv[i]);
			return -1;
		}
	}

	return 0;
}
