#ifndef _SDLAUDIO_H_
#define _SDLAUDIO_H_

#ifdef _WIN32
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

int sdlaudio_init();
void sdlaudio_generateSample(void* dummy);

#endif
