#ifndef _MACHINE_H_
#define _MACHINE_H_

#include "config.h"
#include <stdint.h>
#include "cpu/cpu.h"
#include "chipset/i8259.h"
#include "chipset/i8253.h"
#include "chipset/i8237.h"
#include "chipset/i8255.h"
#include "chipset/uart.h"
#include "modules/io/tcpmodem.h"
#include "modules/audio/opl2.h"
#include "modules/audio/nukedopl.h"
#include "modules/audio/blaster.h"
#include "modules/audio/pcspeaker.h"
#include "modules/disk/fdc.h"
#include "modules/input/input.h"

#define MACHINE_MEM_RAM			0
#define MACHINE_MEM_ROM			1
#define MACHINE_MEM_ENDLIST		2

#define MACHINE_ROM_OPTIONAL	0
#define MACHINE_ROM_REQUIRED	1
#define MACHINE_ROM_ISNOTROM	2

typedef struct {
	CPU_t CPU;
	I8259_t i8259;
	I8253_t i8253;
	I8237_t i8237;
	I8255_t i8255;
	UART_t UART[2];
#ifdef ENABLE_TCP_MODEM
	TCPMODEM_t tcpmodem[2];
#endif
	OPL2_t OPL2;
	opl3_chip OPL3;
	BLASTER_t blaster;
	PCSPEAKER_t pcspeaker;
	KEYSTATE_t KeyState;
	FDC_t fdc;
} MACHINE_t;

typedef struct {
	uint8_t memtype;
	uint32_t start;
	uint32_t size;
	uint8_t required;
	char* filename;
} MACHINEMEM_t;

typedef struct {
	char* id;
	char* description;
	int (*init)(MACHINE_t* machine);
	uint8_t video;
	double speed;
} MACHINEDEF_t;

int machine_init_generic_xt(MACHINE_t* machine);
int machine_init(MACHINE_t* machine, char* id);
void machine_list();

#endif
