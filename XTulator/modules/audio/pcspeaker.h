#ifndef _PCSPEAKER_H_
#define _PCSPEAKER_H_

#include <stdint.h>

#define PC_SPEAKER_GATE_DIRECT	0
#define PC_SPEAKER_GATE_TIMER2	1

#define PC_SPEAKER_USE_DIRECT	0
#define PC_SPEAKER_USE_TIMER2	1

#define PC_SPEAKER_MOVEMENT		800

void pcspeaker_setGateState(uint8_t gate, uint8_t value);
void pcspeaker_selectGate(uint8_t value);
int16_t pcspeaker_getSample();
void pcspeaker_init();

#endif
