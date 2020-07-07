#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>

#define MEMORY_RANGE		0x100000
#define MEMORY_MASK			0x0FFFFF

void memory_mapRegister(uint32_t start, uint32_t len, uint8_t* readb, uint8_t* writeb);
void memory_mapCallbackRegister(uint32_t start, uint32_t count, uint8_t(*readb)(void*, uint32_t), void (*writeb)(void*, uint32_t, uint8_t), void* udata);
int memory_init();

#endif
