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
	Emulation of the Sound Blaster 2.0
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../config.h"
#include "../../debuglog.h"
#include "../../ports.h"
#include "../../timing.h"
#include "../../chipset/i8237.h"
#include "../../chipset/i8259.h"
#include "blaster.h"

const int16_t cmd_E2_table[9] = { 0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 };

void blaster_putreadbuf(BLASTER_t* blaster, uint8_t value) {
	if (blaster->readlen == 16) return;

	blaster->readbuf[blaster->readlen++] = value;
}

uint8_t blaster_getreadbuf(BLASTER_t* blaster) {
	uint8_t ret;

	ret = blaster->readbuf[0];
	if (blaster->readlen > 0) {
		blaster->readlen--;
	}
	memmove(blaster->readbuf, blaster->readbuf + 1, 15);

	return ret;
}

void blaster_reset(BLASTER_t* blaster) {
	blaster->dspenable = 0;
	blaster->sample = 0;
	blaster->readlen = 0;
	blaster_putreadbuf(blaster, 0xAA);
}

void blaster_writecmd(BLASTER_t* blaster, uint8_t value) {
	switch (blaster->lastcmd) {
	case 0x10: //direct DAC, 8-bit
		blaster->sample = value;
		blaster->sample -= 128;
		blaster->sample *= 256;
		blaster->lastcmd = 0;
		return;
	case 0x14: //DMA DAC, 8-bit
	case 0x24:
		if (blaster->writehilo == 0) {
			blaster->dmalen = value;
			blaster->writehilo = 1;
		} else {
			blaster->dmalen |= (uint32_t)value << 8;
			blaster->dmalen++;
			blaster->lastcmd = 0;
			blaster->dmacount = 0;
			blaster->silencedsp = 0;
			blaster->autoinit = 0;
			blaster->dorecord = (blaster->lastcmd == 0x24) ? 1 : 0;
			blaster->activedma = 1;
			timing_timerEnable(blaster->timer);
#ifdef DEBUG_BLASTER
			debug_log(DEBUG_DETAIL, "[BLASTER] Begin DMA transfer mode with %lu byte blocks\r\n", blaster->dmalen);
#endif
		}
		return;
	case 0x40: //set time constant
		blaster->timeconst = value;
		blaster->samplerate = 1000000.0 / (256.0 - (double)value);
		timing_updateIntervalFreq(blaster->timer, blaster->samplerate);
		blaster->lastcmd = 0;
#ifdef DEBUG_BLASTER
		debug_log(DEBUG_DETAIL, "[BLASTER] Set time constant: %u (Sample rate: %f Hz)\r\n", value, blaster->samplerate);
#endif
		return;
	case 0x48: //set DMA block size
		if (blaster->writehilo == 0) {
			blaster->dmalen = value;
			blaster->writehilo = 1;
		} else {
			blaster->dmalen |= (uint32_t)value << 8;
			blaster->dmalen++;
			blaster->lastcmd = 0;
		}
		return;
	case 0x80: //silence DAC
		if (blaster->writehilo == 0) {
			blaster->dmalen = value;
			blaster->writehilo = 1;
		} else {
			blaster->dmalen |= (uint32_t)value << 8;
			blaster->dmalen++;
			blaster->lastcmd = 0;
			blaster->dmacount = 0;
			blaster->silencedsp = 1;
			blaster->autoinit = 0;
			timing_timerEnable(blaster->timer);
		}
		return;
	case 0xE0: //DSP identification (returns bitwise NOT of data byte)
		blaster_putreadbuf(blaster, ~value);
		blaster->lastcmd = 0;
		return;
	case 0xE2: //DMA identification write
	{
		int16_t val = 0xAA, i;
		for (i = 0; i < 8; i++) {
			if ((value >> i) & 0x01) {
				val += cmd_E2_table[i];
			}
		}
		val += cmd_E2_table[8];
		i8237_write(blaster->i8237, blaster->dmachan, (uint8_t)val);
		blaster->lastcmd = 0;
		return;
	}
	case 0xE4: //write test register
		blaster->testreg = value;
		blaster->lastcmd = 0;
		return;
	}

	switch (value) {
	case 0x10: //direct DAC, 8-bit
		break;
	case 0x14: //DMA DAC, 8-bit
	case 0x24:
		blaster->writehilo = 0;
		break;
	case 0x1C: //auto-initialize DMA DAC, 8-bit
	case 0x2C:
		blaster->dmacount = 0;
		blaster->silencedsp = 0;
		blaster->autoinit = 1;
		blaster->dorecord = (value == 0x2C) ? 1 : 0;
		blaster->activedma = 1;
		timing_timerEnable(blaster->timer);
#ifdef DEBUG_BLASTER
		debug_log(DEBUG_DETAIL, "[BLASTER] Begin auto-init DMA transfer mode with %lu byte blocks\r\n", blaster->dmalen);
#endif
		break;
	case 0x20: //direct DAC, 8-bit record
		blaster_putreadbuf(blaster, 128); //Silence, though I might add actual recording support later.
		break;
	case 0x40: //set time constant
		break;
	case 0x48: //set DMA block size
		blaster->writehilo = 0;
		break;
	case 0x80: //silence DAC
		blaster->writehilo = 0;
		break;
	case 0xD0: //halt DMA operation, 8-bit
		blaster->activedma = 0;
		timing_timerDisable(blaster->timer);
		break;
	case 0xD1: //speaker on
		blaster->dspenable = 1;
		break;
	case 0xD3: //speaker off
		blaster->dspenable = 0;
		break;
	case 0xD4: //continue DMA operation, 8-bit
		blaster->activedma = 1;
		timing_timerEnable(blaster->timer);
		break;
	case 0xDA: //exit auto-initialize DMA operation, 8-bit
		blaster->activedma = 0;
		blaster->autoinit = 0;
		break;
	case 0xE0: //DSP identification (returns bitwise NOT of data byte)
		break;
	case 0xE1: //DSP version (SB 2.0 is DSP 2.01)
		blaster_putreadbuf(blaster, 2);
		blaster_putreadbuf(blaster, 1);
		break;
	case 0xE2: //DMA identification write
		break;
	case 0xE4: //write test register
		break;
	case 0xE8: //read test register
		blaster_putreadbuf(blaster, blaster->testreg);
		break;
	case 0xF2: //trigger 8-bit IRQ
		i8259_doirq(blaster->i8259, blaster->irq);
		break;
	case 0xF8: //Undocumented
		blaster_putreadbuf(blaster, 0);
		break;
	default:
		debug_log(DEBUG_ERROR, "[BLASTER] Unrecognized command: 0x%02X\r\n", value);
	}

	blaster->lastcmd = value;
}

void blaster_write(BLASTER_t* blaster, uint16_t addr, uint8_t value) {
#ifdef DEBUG_BLASTER
	debug_log(DEBUG_DETAIL, "[BLASTER] Write %03X: %02X\r\n", addr, value);
#endif
	addr &= 0x0F;

	switch (addr) {
	case 0x06:
		if (value == 0) {
			blaster_reset(blaster);
		}
		break;
	case 0x0C: //DSP write (command/data)
		blaster_writecmd(blaster, value);
		break;
	}
}

uint8_t blaster_read(BLASTER_t* blaster, uint16_t addr) {
	uint8_t ret = 0xFF;

#ifdef DEBUG_BLASTER
	debug_log(DEBUG_DETAIL, "[BLASTER] Read %03X\r\n", addr);
#endif
	addr &= 0x0F;

	switch (addr) {
	case 0x0A:
		return blaster_getreadbuf(blaster);
	case 0x0C:
		return 0x00;
	case 0x0E:
		return (blaster->readlen > 0) ? 0x80 : 0x00;
	}

	return ret;
}

void blaster_generateSample(BLASTER_t* blaster) { //for DMA mode
	if (blaster->silencedsp == 0) {
		if (blaster->dorecord == 0) {
			blaster->sample = i8237_read(blaster->i8237, blaster->dmachan);
			blaster->sample -= 128;
			blaster->sample *= 256;
		} else {
			i8237_write(blaster->i8237, blaster->dmachan, 128); //silence
		}
	} else {
		blaster->sample = 0;
	}

	if (++blaster->dmacount == blaster->dmalen) {
		blaster->dmacount = 0;
		i8259_doirq(blaster->i8259, blaster->irq);
		if (blaster->autoinit == 0) {
			blaster->activedma = 0;
			timing_timerDisable(blaster->timer);
		}
	}

	if (blaster->dspenable == 0) {
		blaster->sample = 0;
		return;
	}
}

int16_t blaster_getSample(BLASTER_t* blaster) {
	return blaster->sample;
}

void blaster_init(BLASTER_t* blaster, I8237_t* i8237, I8259_t* i8259, uint16_t base, uint8_t dma, uint8_t irq) {
	debug_log(DEBUG_INFO, "[BLASTER] Initializing Sound Blaster 2.0 at base port 0x%03X, IRQ %u, DMA %u\r\n", base, irq, dma);
	memset(blaster, 0, sizeof(BLASTER_t));
	blaster->i8237 = i8237;
	blaster->i8259 = i8259;
	blaster->dmachan = dma;
	blaster->irq = irq;
	ports_cbRegister(base, 16, (void*)blaster_read, NULL, (void*)blaster_write, NULL, blaster);

	//TODO: error handling
	blaster->timer = timing_addTimer(blaster_generateSample, blaster, 22050, TIMING_DISABLED);
}
