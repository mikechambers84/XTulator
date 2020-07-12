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
#include "modules/io/ne2000.h"
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

#define MACHINE_HW_OPL					0x0000000000000001ULL
#define MACHINE_HW_BLASTER				0x0000000000000002ULL
#define MACHINE_HW_UART0_NONE			0x0000000000000004ULL
#define MACHINE_HW_UART0_MOUSE			0x0000000000000008ULL
#define MACHINE_HW_UART0_TCPMODEM		0x0000000000000010ULL
#define MACHINE_HW_UART1_NONE			0x0000000000000020ULL
#define MACHINE_HW_UART1_MOUSE			0x0000000000000040ULL
#define MACHINE_HW_UART1_TCPMODEM		0x0000000000000080ULL
#define MACHINE_HW_RTC					0x0000000000000100ULL
#define MACHINE_HW_DISK_HLE				0x0000000000000200ULL
#define MACHINE_HW_NE2000				0x0000000000000400ULL

//the "skip" HW flags are set in args.c to make sure machine init functions don't override explicit settings from the command line
#define MACHINE_HW_SKIP_OPL				0x8000000000000000ULL
#define MACHINE_HW_SKIP_BLASTER			0x4000000000000000ULL
#define MACHINE_HW_SKIP_UART0			0x2000000000000000ULL
#define MACHINE_HW_SKIP_UART1			0x1000000000000000ULL
#define MACHINE_HW_SKIP_DISK			0x0800000000000000ULL
#define MACHINE_HW_SKIP_RTC				0x0400000000000000ULL

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
	uint8_t mixOPL;
	BLASTER_t blaster;
	uint8_t mixBlaster;
	PCSPEAKER_t pcspeaker;
#ifdef USE_NE2000
	NE2000_t ne2000;
#endif
	KEYSTATE_t KeyState;
	FDC_t fdc;
	uint64_t hwflags;
	int pcap_if;
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
	uint64_t hwflags;
} MACHINEDEF_t;

int machine_init_generic_xt(MACHINE_t* machine);
int machine_init(MACHINE_t* machine, char* id);
void machine_list();

#endif
