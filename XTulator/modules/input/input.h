#ifndef _INPUT_H_
#define _INPUT_H_

#include <stdint.h>

typedef struct KeyState_s {
	uint8_t scancode;
	uint8_t isNew;
} KeyState;

#endif
