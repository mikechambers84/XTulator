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
#include "modules/audio/blaster.h"
#include "modules/input/input.h"

#define MACHINE_MEM_RAM			0
#define MACHINE_MEM_ROM			1
#define MACHINE_MEM_ENDLIST		2

#define MACHINE_ROM_OPTIONAL	0
#define MACHINE_ROM_REQUIRED	1
#define MACHINE_ROM_ISNOTROM	2

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
	int (*init)();
	uint8_t video;
	double speed;
} MACHINE_t;

extern CPU_t myCPU;
extern I8259_t i8259;
extern I8253_t i8253;
extern I8237_t i8237;
extern I8255_t i8255;
extern UART_t UART[2];
extern OPL2_t OPL2;
extern BLASTER_t blaster;
extern KeyState myKey;

#ifdef ENABLE_TCP_MODEM
extern TCPMODEM_t tcpmodem[2];
#endif


int machine_init_generic_xt();
int machine_init(char* id);
void machine_list();

#endif
