#ifndef _BLASTER_H_
#define _BLASTER_H_

#include <stdint.h>
#include "../../chipset/i8237.h"
#include "../../chipset/i8259.h"

typedef struct {
	I8237_t* i8237;
	I8259_t* i8259;
	uint8_t dspenable;
	int16_t sample;
	uint8_t readbuf[16];
	uint8_t readlen;
	uint8_t readready;
	uint8_t writebuf;
	uint8_t timeconst;
	double samplerate;
	uint32_t timer;
	uint32_t dmalen;
	uint8_t dmachan;
	uint8_t irq;
	uint8_t lastcmd;
	uint8_t writehilo;
	uint32_t dmacount;
	uint8_t autoinit;
	uint8_t testreg;
	uint8_t silencedsp;
	uint8_t dorecord;
	uint8_t activedma;
} BLASTER_t;

void blaster_write(BLASTER_t* blaster, uint16_t addr, uint8_t value);
uint8_t blaster_read(BLASTER_t* blaster, uint16_t addr);
int16_t blaster_getSample(BLASTER_t* blaster);
void blaster_init(BLASTER_t* blaster, I8237_t* i8237, I8259_t* i8259, uint16_t base, uint8_t dma, uint8_t irq);

#endif
