#ifndef _I8237_H_
#define _I8237_H_

#include <stdint.h>
#include "../cpu/cpu.h"

typedef struct I8237_s {
	struct {
		uint32_t page;
		uint32_t addr;
		uint32_t reloadaddr;
		uint32_t addrinc;
		uint16_t count;
		uint16_t reloadcount;
		uint8_t autoinit;
		uint8_t mode;
		uint8_t enable;
		uint8_t masked;
		uint8_t dreq;
		uint8_t terminal;
		uint8_t operation;
	} chan[4];
	uint8_t flipflop;
	uint8_t tempreg;
	uint8_t memtomem;
	CPU_t* cpu;
} I8237_t;

#define DMA_MODE_DEMAND		0
#define DMA_MODE_SINGLE		1
#define DMA_MODE_BLOCK		2
#define DMA_MODE_CASCADE	3

#define DMA_OP_VERIFY		0
#define DMA_OP_WRITEMEM		1
#define DMA_OP_READMEM		2

void i8237_writeport(I8237_t* i8237, uint16_t addr, uint8_t value);
uint8_t i8237_readport(I8237_t* i8237, uint16_t addr);
uint8_t i8237_read(I8237_t* i8237, uint8_t ch);
void i8237_write(I8237_t* i8237, uint8_t ch, uint8_t value);
void i8237_init(I8237_t* i8237, CPU_t* cpu);

#endif
