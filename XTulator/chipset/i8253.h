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

#ifndef _I8253_H_
#define _I8253_H_

#include <stdint.h>
#include "i8259.h"

#define PIT_MODE_LATCHCOUNT	0
#define PIT_MODE_LOBYTE	1
#define PIT_MODE_HIBYTE	2
#define PIT_MODE_TOGGLE	3

typedef struct {
	void* i8253;
	I8259_t* i8259;
} I8253CB_t;

typedef struct {
	uint16_t chandata[3];
	uint8_t accessmode[3];
	uint8_t bytetoggle[3];
	uint32_t effectivedata[3];
	float chanfreq[3];

	uint8_t active[3];
	int32_t counter[3];
	int32_t reload[3];
	uint8_t mode[3];
	uint8_t dataflipflop[3];
	uint8_t bcd[3];
	uint8_t rlmode[3];
	uint16_t latch[3];
	uint8_t out[3];
	I8253CB_t cbdata;
} I8253_t;

void i8253_write(I8253_t* i8253, uint16_t portnum, uint8_t value);
uint8_t i8253_read(I8253_t* i8253, uint16_t portnum);
void i8253_init(I8253_t* i8253, I8259_t* i8259);

#endif
