#ifndef _PCSPEAKER_H_
#define _PCSPEAKER_H_

#include <stdint.h>

#define PC_SPEAKER_GATE_DIRECT	0
#define PC_SPEAKER_GATE_TIMER2	1

#define PC_SPEAKER_USE_DIRECT	0
#define PC_SPEAKER_USE_TIMER2	1

#define PC_SPEAKER_MOVEMENT		800

typedef struct {
	uint8_t pcspeaker_gateSelect;
	uint8_t pcspeaker_gate[2];
	int16_t pcspeaker_amplitude;
} PCSPEAKER_t;

void pcspeaker_setGateState(PCSPEAKER_t* spk, uint8_t gate, uint8_t value);
void pcspeaker_selectGate(PCSPEAKER_t* spk, uint8_t value);
int16_t pcspeaker_getSample(PCSPEAKER_t* spk);
void pcspeaker_init(PCSPEAKER_t* spk);

#endif
