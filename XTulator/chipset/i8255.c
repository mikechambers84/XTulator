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
	Intel 8255 Programmable Peripheral Interface (PPI)

	This is not complete.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../config.h"
#include "../timing.h"
#include "../modules/audio/pcspeaker.h"
#include "i8255.h"
#include "../ports.h"
#include "../debuglog.h"

uint8_t i8255_readport(I8255_t* i8255, uint16_t portnum) {
#ifdef DEBUG_PPI
	debug_log(DEBUG_DETAIL, "[I8255] Read port %02X\r\n", portnum);
#endif
	portnum &= 7;
	switch (portnum) {
	case 0:
		return i8255->keystate->scancode;
	case 1:
		return i8255->portB;
	case 2:
		//debug_log(DEBUG_DETAIL, "read 0x62\r\n");
		if (i8255->portB & 8) {
			return i8255->sw2 >> 4;
		} else {
			return i8255->sw2 & 0x0F;
		}
	}
	return 0xFF;
}

void i8255_writeport(I8255_t* i8255, uint16_t portnum, uint8_t value) {
#ifdef DEBUG_PPI
	debug_log(DEBUG_DETAIL, "[I8255] Write port %02X <- %02X\r\n", portnum, value);
#endif
	portnum &= 7;
	switch (portnum) {
	case 0:
		i8255->keystate->scancode = 0xAA;
		break;
	case 1:
		if (value & 0x01) {
			pcspeaker_selectGate(i8255->pcspeaker, PC_SPEAKER_USE_TIMER2);
#ifdef DEBUG_PPI
			debug_log(DEBUG_DETAIL, "[I8255] Speaker take input from timer 2\r\n");
#endif
		} else {
			pcspeaker_selectGate(i8255->pcspeaker, PC_SPEAKER_USE_DIRECT);
#ifdef DEBUG_PPI
			debug_log(DEBUG_DETAIL, "[I8255] Speaker take input from direct\r\n");
#endif
		}
		pcspeaker_setGateState(i8255->pcspeaker, PC_SPEAKER_GATE_DIRECT, (value >> 1) & 1);
#ifdef DEBUG_PPI
		debug_log(DEBUG_DETAIL, "[I8255] Speaker direct value = %u\r\n", (value >> 1) & 1);
#endif
		if ((value & 0x40) && !(i8255->portB & 0x40)) {
			i8255->keystate->scancode = 0xAA;
#ifdef DEBUG_PPI
			debug_log(DEBUG_DETAIL, "[I8255] Keyboard reset\r\n");
#endif
		}
		i8255->portB = (value & 0xEF) | (i8255->portB & 0x10);
		break;
	}
}

void i8255_refreshToggle(I8255_t* i8255) {
	i8255->portB ^= 0x10; //simulate DRAM refresh toggle, many BIOSes require this...
}

void i8255_init(I8255_t* i8255, KEYSTATE_t* keystate, PCSPEAKER_t* pcspeaker) {
	memset(i8255, 0, sizeof(I8255_t));
	i8255->keystate = keystate;
	i8255->pcspeaker = pcspeaker;

	if (videocard == VIDEO_CARD_VGA) {
		i8255->sw2 = 0x46;
	}
	else if (videocard == VIDEO_CARD_CGA) {
		i8255->sw2 = 0x66;
	}

	ports_cbRegister(0x60, 6, (void*)i8255_readport, NULL, (void*)i8255_writeport, NULL, i8255);
	timing_addTimer(i8255_refreshToggle, i8255, 66667, TIMING_ENABLED);
}
