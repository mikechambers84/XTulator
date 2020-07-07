#ifndef _OPL2_H_
#define _OPL2_H_

#include <stdint.h>

#define VOLUME_CONST	1.2

typedef struct {
	uint8_t chan;
	uint8_t op;
	void* opl2;
} OPL2CB_t;

typedef struct {
	uint8_t addr;
	uint8_t data[0x100];
	struct {
		uint16_t fnum;
		uint8_t octave;
		double frequency;
		uint8_t on;
	} chan[9];
	struct {
		uint32_t timer;
		double amplitude;
		double envelope;
		double sample;
		double lastsine;
		uint8_t volume;
		uint8_t inattack;
		uint8_t attackval;
		uint8_t decayval;
		uint8_t waveform;
		uint8_t sustain;
		uint8_t sustainlevel;
		uint8_t usevibrato;
		uint8_t usetremolo;
		uint32_t tick;
		OPL2CB_t opdata;
	} oper[22];
} OPL2_t;

void opl2_write(OPL2_t* opl2, uint16_t portnum, uint8_t value);
uint8_t opl2_read(OPL2_t* opl2, uint16_t portnum);
int16_t opl2_generateSample(OPL2_t* opl2);
void opl2_tickOperator(OPL2CB_t* opl2cb);
void opl2_init(OPL2_t* opl2);

#endif
