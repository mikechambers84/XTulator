#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#include <stdint.h>

/*
	Debug levels:

	0 - No logging
	1 - Errors
	2 - Errors, info
	3 - Errors, info, detailed debugging
*/

#define DEBUG_NONE		0
#define DEBUG_ERROR		1
#define DEBUG_INFO		2
#define DEBUG_DETAIL	3

void debug_log(uint8_t level, char* format, ...);
void debug_setLevel(uint8_t level);
void debug_init();

#endif
