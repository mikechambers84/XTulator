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
#include <math.h>
#include "../../config.h"
#include "../../debuglog.h"
#include "../../ports.h"
#include "../../timing.h"
#include "opl2.h"

const double opl2_attack[16] = {
	1.004, 1.005, 1.006, 1.007, 1.008, 1.009, 1.01, 1.02,
	1.03, 1.04, 1.05, 1.06, 1.07, 1.08, 1.09, 1.1
};

const double opl2_decay[16] = {
	0.99995, 0.9998, 0.9997, 0.9996, 0.9995, 0.9994, 0.9993, 0.9992,
	0.9990, 0.9989, 0.9988, 0.9987, 0.9986, 0.9985, 0.9984, 0.9983
};

/*const double opl2_modfreq[16] = {

};*/

const double opl2_suslevel[16] = {
	0.75, 0.70, 0.65, 0.60, 0.55, 0.45, 0.40, 0.35, 0.30, 0.25, 0.20, 0.15, 0.10, 0.05, 0.0025
};

const uint8_t opregoffset[22] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15
};

const uint8_t chanopnum[9][2] = {
	{ 0x00, 0x03 },
	{ 0x01, 0x04 },
	{ 0x02, 0x05 },
	{ 0x08, 0x0B },
	{ 0x09, 0x0C },
	{ 0x0A, 0x0D },
	{ 0x10, 0x13 },
	{ 0x11, 0x14 },
	{ 0x12, 0x15 }
};

const uint8_t optochan[0x16] = {
	1, 2, 3, 1, 2, 3, 255, 255, 4, 5, 6, 4, 5, 6, 255, 255, 7, 8, 9, 7, 8, 9
};

void opl2_writeData(OPL2_t* opl2, uint8_t value) {
	uint8_t ch, op, op1, op2;
	if ((opl2->addr >= 0x20) && (opl2->addr <= 0x35)) {
		op = opl2->addr - 0x20;
		opl2->oper[op].sustain = (value & 0x20) ? 1 : 0;
		opl2->oper[op].usevibrato = (value & 0x40) ? 1 : 0; //TODO: actually implement this in the operator ticks
		opl2->oper[op].usetremolo = (value & 0x80) ? 1 : 0; //TODO: actually implement this in the operator ticks
	}
	else if ((opl2->addr >= 0x40) && (opl2->addr <= 0x55)) {
		op = opl2->addr - 0x40;
		opl2->oper[op].volume = 18; // value & 0x3F; //TODO: fix volume
	}
	else if ((opl2->addr >= 0x80) && (opl2->addr <= 0x95)) {
		op = opl2->addr - 0x80;
		opl2->oper[op].sustainlevel = value >> 4;
		//TODO: release rate
	}
	else if ((opl2->addr >= 0xA0) && (opl2->addr <= 0xA8)) {
		ch = opl2->addr - 0xA0;
		opl2->chan[ch].fnum = (opl2->chan[ch].fnum & 0xFF00) | value;
	}
	else if ((opl2->addr >= 0xB0) && (opl2->addr <= 0xB8)) {
		ch = opl2->addr - 0xB0;
		op1 = chanopnum[ch][0];
		op2 = chanopnum[ch][1];
		opl2->chan[ch].fnum = (opl2->chan[ch].fnum & 0x00FF) | ((uint16_t)(value & 3) << 8);
		opl2->chan[ch].octave = (value >> 2) & 7;
		opl2->chan[ch].frequency = 2 * pow(2, opl2->chan[ch].octave) * (49716.0 / 1048576.0) * (double)opl2->chan[ch].fnum;
		if ((opl2->chan[ch].on == 0) && (value & 0x20)) {
			opl2->chan[ch].on = 1;
			opl2->oper[op1].inattack = 1;
			opl2->oper[op1].attackval = 0;
			opl2->oper[op1].decayval = 0;
			opl2->oper[op1].envelope = 0.01;
			timing_timerEnable(opl2->oper[op1].timer);
#ifdef DEBUG_OPL2
			debug_log(DEBUG_DETAIL, "[OPL2] Key on channel %u, frequency: %f\r\n", ch, opl2->chan[ch].frequency);
#endif
		}
		else {
			opl2->chan[ch].on = (value & 0x20) ? 1 : 0;
			opl2->oper[op1].amplitude = 0;
			opl2->oper[op2].amplitude = 0;
			timing_timerDisable(opl2->oper[op1].timer);
			timing_timerDisable(opl2->oper[op2].timer);
		}
	}
	else if ((opl2->addr >= 0xE0) && (opl2->addr <= 0xF5)) {
		op = opl2->addr - 0xE0;
		opl2->oper[op].waveform = value & 3;
#ifdef DEBUG_OPL2
		debug_log(DEBUG_DETAIL, "[OPL2] Oper %u waveform set to %u\r\n", op, value & 3);
#endif
	}
}

void opl2_write(OPL2_t* opl2, uint16_t portnum, uint8_t value) {
#ifdef DEBUG_OPL2
	debug_log(DEBUG_DETAIL, "[OPL2] Write %03X: %u\r\n", portnum, value);
#endif

	portnum &= 1;
	switch (portnum) {
	case 0: //address
		opl2->addr = value;
		break;
	case 1: //data
		opl2->data[opl2->addr] = value;
		opl2_writeData(opl2, value);
		break;
	}
}

uint8_t opl2_read(OPL2_t* opl2, uint16_t portnum) {
#ifdef DEBUG_OPL2
	debug_log(DEBUG_DETAIL, "[OPL2] Read %03X\r\n", portnum);
#endif
	portnum &= 1;
	if (portnum == 0) { //status port
		uint8_t ret;
		ret = (opl2->data[0x04] & 0x01) ? 0x40 : 0x00;
		ret |= (opl2->data[0x04] & 0x02) ? 0x20 : 0x00;
		ret |= ret ? 0x80 : 0x00;
		return ret;
	}
	else {
		return 0xFF;
	}
}

int16_t opl2_generateSample(OPL2_t* opl2) {
	uint16_t op;
	//int16_t tmp;
	double val = 0;
	//static FILE* wavout = NULL;

	//if (wavout == NULL) {
	//	wavout = fopen("audio.raw", "wb");
	//}

	for (op = 0; op < 0x16; op++) {
		if (optochan[op] != 255) {
			val += opl2->oper[op].sample;
		}
	}

	//printf("%f\r\n", val);

	//tmp = (int16_t)val;
	//fwrite(&tmp, 2, 1, wavout);

	return (int16_t)val;
}

void opl2_tickOperator(OPL2CB_t* opl2cb) {
	OPL2_t* opl2;
	uint8_t chan, op;
	double mult, sine;

	opl2 = opl2cb->opl2;
	op = opl2cb->op;
	chan = optochan[op];
	if (chan == 255) return;
	chan--;

	sine = sin(opl2->chan[chan].frequency * 6.283185307179586 * (double)opl2->oper[op].tick / SAMPLE_RATE);
	mult = 1.0;
	switch (opl2->oper[op].waveform) {
	case 1:
		if (sine < 0) mult = 0.0;
		break;
	case 2:
		if (sine < 0) mult = -1.0;
		break;
	case 3:
		if ((sine < opl2->oper[op].lastsine) || (sine < 0)) mult = 0.0;
		break;
	}
	opl2->oper[op].amplitude = opl2->oper[op].envelope * pow(VOLUME_CONST, (double)(63 - opl2->oper[op].volume));
	opl2->oper[op].sample = mult * opl2->oper[op].amplitude * sine;
	opl2->oper[op].tick = (opl2->oper[op].tick + 1) % SAMPLE_RATE;
	opl2->oper[op].lastsine = sine;

	if (opl2->oper[op].inattack) {
		opl2->oper[op].envelope *= opl2_attack[opl2->oper[op].attackval];
		if (opl2->oper[op].envelope >= 1.0) {
			opl2->oper[op].inattack = 0;
		}
	} else {
		opl2->oper[op].envelope *= opl2_decay[opl2->oper[op].decayval];
		if (opl2->oper[op].sustain) {
			//TODO: I'm doing this completely wrong...
			if (opl2->oper[op].envelope < opl2_suslevel[opl2->oper[op].sustainlevel]) {
				opl2->oper[op].envelope = opl2_suslevel[opl2->oper[op].sustainlevel];
			}
		}
	}
}

void opl2_init(OPL2_t* opl2) {
	uint8_t i;
	memset(opl2, 0, sizeof(OPL2_t));
	ports_cbRegister(0x388, 2, (void*)opl2_read, NULL, (void*)opl2_write, NULL, opl2);

	for (i = 0; i < 0x16; i++) {
		//TODO: add error handling
		//opl2->oper[i].opdata.chan = i;
		opl2->oper[i].opdata.op = i;
		opl2->oper[i].opdata.opl2 = (void*)opl2;
		opl2->oper[i].timer = timing_addTimer(opl2_tickOperator, &opl2->oper[i].opdata, SAMPLE_RATE, TIMING_DISABLED);
	}
}
