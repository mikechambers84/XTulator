#ifndef _MOUSE_H_
#define _MOUSE_H_

#include <stdint.h>
#include "../../chipset/uart.h"

#define MOUSE_ACTION_MOVE		0
#define MOUSE_ACTION_LEFT		1
#define MOUSE_ACTION_RIGHT		2

#define MOUSE_PRESSED			0
#define MOUSE_UNPRESSED			1
#define MOUSE_NEITHER			2

#define MOUSE_BUFFER_LEN		60

typedef struct {
	uint8_t left;
	uint8_t right;
} MOUSE_t;

void mouse_togglereset(void* dummy, uint8_t value);
void mouse_action(uint8_t action, uint8_t state, int32_t xrel, int32_t yrel);
void mouse_rxpoll(void* dummy);
void mouse_init(UART_t* uart);

#endif
