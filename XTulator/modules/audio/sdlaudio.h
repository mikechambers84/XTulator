#ifndef _SDLAUDIO_H_
#define _SDLAUDIO_H_

#ifdef _WIN32
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif
#include "../../machine.h"

#define SDLAUDIO_TIMING_FAST		1
#define SDLAUDIO_TIMING_NORMAL		2

int sdlaudio_init(MACHINE_t* machine);
void sdlaudio_generateSample(void* dummy);
void sdlaudio_updateSampleTiming();

#endif
