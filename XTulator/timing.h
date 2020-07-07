#ifndef _TIMING_H_
#define _TIMING_H_

#include <stdint.h>

typedef struct TIMER_s {
	uint64_t interval;
	uint64_t previous;
	uint8_t enabled;
	void (*callback)(void*);
	void* data;
} TIMER;

#define TIMING_ENABLED	1
#define TIMING_DISABLED	0
#define TIMING_ERROR 0xFFFFFFFF

#define TIMING_RINGSIZE	1024

int timing_init();
void timing_loop();
uint32_t timing_addTimer(void* callback, void* data, double frequency, uint8_t enabled);
void timing_updateIntervalFreq(uint32_t tnum, double frequency);
void timing_updateInterval(uint32_t tnum, uint64_t interval);
void timing_speedTest();
void timing_timerEnable(uint32_t tnum);
void timing_timerDisable(uint32_t tnum);
uint64_t timing_getFreq();
uint64_t timing_getCur();

extern uint64_t timing_cur;
extern uint64_t timing_freq;

#endif
