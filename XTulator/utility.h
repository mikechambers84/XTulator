#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <stdint.h>

int utility_loadFile(uint8_t* dst, size_t len, char* srcfile);
void utility_sleep(uint32_t ms);

#endif
