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

#include "../../config.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pcspeaker.h"
#include "../../timing.h"

void pcspeaker_setGateState(PCSPEAKER_t* spk, uint8_t gate, uint8_t value) {
	spk->pcspeaker_gate[gate] = value;
}

void pcspeaker_selectGate(PCSPEAKER_t* spk, uint8_t value) {
	spk->pcspeaker_gateSelect = value;
}

void pcspeaker_callback(PCSPEAKER_t* spk) {
	if (spk->pcspeaker_gateSelect == PC_SPEAKER_USE_TIMER2) {
		if (spk->pcspeaker_gate[PC_SPEAKER_GATE_TIMER2] && spk->pcspeaker_gate[PC_SPEAKER_GATE_DIRECT]) {
			if (spk->pcspeaker_amplitude < 15000) {
				spk->pcspeaker_amplitude += PC_SPEAKER_MOVEMENT;
			}
		}
		else {
			if (spk->pcspeaker_amplitude > 0) {
				spk->pcspeaker_amplitude -= PC_SPEAKER_MOVEMENT;
			}
		}
		//pcspeaker_amplitude = 0;
	}
	else {
		if (spk->pcspeaker_gate[PC_SPEAKER_GATE_DIRECT]) {
			if (spk->pcspeaker_amplitude < 15000) {
				spk->pcspeaker_amplitude += PC_SPEAKER_MOVEMENT;
			}
		}
		else {
			if (spk->pcspeaker_amplitude > 0) {
				spk->pcspeaker_amplitude -= PC_SPEAKER_MOVEMENT;
			}
		}
	}
	if (spk->pcspeaker_amplitude > 15000) spk->pcspeaker_amplitude = 15000;
	if (spk->pcspeaker_amplitude < 0) spk->pcspeaker_amplitude = 0;
}

void pcspeaker_init(PCSPEAKER_t* spk) {
	memset(spk, 0, sizeof(PCSPEAKER_t));
	spk->pcspeaker_gateSelect = PC_SPEAKER_GATE_DIRECT;
	timing_addTimer(pcspeaker_callback, spk, SAMPLE_RATE, TIMING_ENABLED);
}

int16_t pcspeaker_getSample(PCSPEAKER_t* spk) {
	//return 0; //PC speaker is really broken, so silence for now
	return spk->pcspeaker_amplitude;
}
