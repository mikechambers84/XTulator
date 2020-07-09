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
	Microsoft-compatible serial mouse
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../config.h"
#include "../../debuglog.h"
#include "../../ports.h"
#include "../../chipset/uart.h"
#include "mouse.h"

MOUSE_t mouse_state;
UART_t* mouse_uart = NULL;
uint8_t mouse_buf[MOUSE_BUFFER_LEN]; //room for six events
uint8_t mouse_bufpos = 0;
uint8_t mouse_lasttoggle = 0;

void mouse_addbuf(uint8_t value) {
	if (mouse_bufpos == MOUSE_BUFFER_LEN) return;

	mouse_buf[mouse_bufpos++] = value;
}

void mouse_togglereset(void* dummy, uint8_t value) { //reset mouse, allows detection, is a callback for the UART module
	if ((mouse_lasttoggle != 0x03) && ((value & 0x03) == 0x03)) {
		mouse_bufpos = 0;
		mouse_addbuf('M');
		//printf("toggle DTR ");
	}
	mouse_lasttoggle = value & 0x03;
}

void mouse_action(uint8_t action, uint8_t state, int32_t xrel, int32_t yrel) {
	if (mouse_uart == NULL) return;
	switch (action) {
	case MOUSE_ACTION_MOVE:
		//printf("X: %ld, Y: %ld\r\n", xrel, yrel);
		break;
	case MOUSE_ACTION_LEFT:
		mouse_state.left = (state == MOUSE_PRESSED) ? 1 : 0;
		break;
	case MOUSE_ACTION_RIGHT:
		mouse_state.right = (state == MOUSE_PRESSED) ? 1 : 0;
		break;
	}

	mouse_addbuf(0x40 | ((yrel & 0xC0) >> 4) | ((xrel & 0xC0) >> 6) | (mouse_state.left ? 0x20 : 0x00) | (mouse_state.right ? 0x10 : 0x00));
	mouse_addbuf(xrel & 0x3F);
	mouse_addbuf(yrel & 0x3F);
}

void mouse_rxpoll(void* dummy) {
	if (mouse_uart == NULL) return;
	if (mouse_uart->rxnew) return;
	if (mouse_bufpos == 0) return;

	uart_rxdata(mouse_uart, mouse_buf[0]);
	memmove(mouse_buf, mouse_buf + 1, MOUSE_BUFFER_LEN - 1);
	mouse_bufpos--;
}

void mouse_init(UART_t* uart) {
	debug_log(DEBUG_INFO, "[MOUSE] Initializing Microsoft-compatible serial mouse\r\n");
	mouse_uart = uart;
}
