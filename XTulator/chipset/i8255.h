#ifndef _I8255_H_
#define _I8255_H_

#include <stdint.h>
#include "../modules/audio/pcspeaker.h"
#include "../modules/input/input.h"

typedef struct {
	uint8_t sw2;
	uint8_t portA;
	uint8_t portB;
	uint8_t portC;
	KEYSTATE_t* keystate;
	PCSPEAKER_t* pcspeaker;
} I8255_t;

uint8_t i8255_readport(I8255_t* i8255, uint16_t portnum);
void i8255_writeport(I8255_t* i8255, uint16_t portnum, uint8_t value);
void i8255_init(I8255_t* i8255, KEYSTATE_t* keystate, PCSPEAKER_t* pcspeaker);

#endif
