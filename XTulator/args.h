#ifndef _ARGS_H_
#define _ARGS_H_

#include <stdint.h>
#include "machine.h"

int args_parse(MACHINE_t* machine, int argc, char* argv[]);
void args_showHelp();

#endif
