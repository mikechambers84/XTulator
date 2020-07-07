#ifndef _FDC_H_
#define _FDC_H_

#include <stdio.h>
#include <stdint.h>
#include "../../cpu/cpu.h"
#include "../../chipset/i8259.h"
#include "../../chipset/i8237.h"

#define FDC_FIFO_LEN					1024

#define FDC_CMD_READ_TRACK				2
#define FDC_CMD_SPECIFY					3
#define FDC_CMD_SENSE_DRIVE_STATUS		4
#define FDC_CMD_WRITE_DATA				5
#define FDC_CMD_READ_DATA				6
#define FDC_CMD_RECALIBRATE				7
#define FDC_CMD_SENSE_INTERRUPT			8
#define FDC_CMD_WRITE_DELETED_DATA		9
#define FDC_CMD_READ_ID					10
#define FDC_CMD_READ_DELETED_DATA		12
#define FDC_CMD_FORMAT_TRACK			13
#define FDC_CMD_SEEK					15

#define FDC_ST0_HD						0x04
#define FDC_ST0_NR						0x08
#define FDC_ST0_UC						0x10
#define FDC_ST0_SE						0x20
#define FDC_ST0_INT_NORMAL				0x00
#define FDC_ST0_INT_ABNORMAL			0x40
#define FDC_ST0_INT_INVALID				0x80
#define FDC_ST0_INT_ABNORMAL_POLL		0xC0

#define FDC_ST1_NID						0x01
#define FDC_ST1_NW						0x02
#define FDC_ST1_NDAT					0x04
#define FDC_ST1_TO						0x10
#define FDC_ST1_DE						0x20
#define FDC_ST1_EN						0x80

#define FDC_ST2_NDAM					0x01
#define FDC_ST2_BCYL					0x02
#define FDC_ST2_SERR					0x04
#define FDC_ST2_SEQ						0x08
#define FDC_ST2_WCYL					0x10
#define FDC_ST2_CRCE					0x20
#define FDC_ST2_DADM					0x40

#define FDC_ST3_HDDR					0x04
#define FDC_ST3_DSDR					0x08
#define FDC_ST3_TRK0					0x10
#define FDC_ST3_RDY						0x20
#define FDC_ST3_WPDR					0x40
#define FDC_ST3_ESIG					0x80

typedef struct {
	uint8_t inserted;
	FILE* dfile;
	uint32_t size;
	uint32_t sectors;
	uint32_t tracks;
	uint32_t sides;
} FDCDISK_t;

typedef struct {
	uint32_t track;
	uint32_t head;
	uint32_t sect;
	uint32_t wanttrack;
	uint8_t seeking;
	uint8_t reading;
	uint8_t transferring;
} FDCPOS_t;

typedef struct {
	CPU_t* cpu;
	I8259_t* i8259;
	I8237_t* i8237;
	uint8_t irq;
	uint8_t dma;
	uint8_t reg[8];
	uint8_t cmd[9];
	uint8_t cmd_pos;
	uint8_t last_cmd;
	uint8_t drivenum;
	uint8_t motoron[4];
	uint8_t datatosend;
	uint8_t fifo[FDC_FIFO_LEN];
	uint16_t fifopos;
	uint16_t fifolen;
	uint8_t st[4]; //status registers
	uint8_t usedma;
	uint8_t busy;
	uint32_t timerseek;
	uint32_t timerread;
	FDCPOS_t position[4];
	FDCDISK_t disk[4];
	uint8_t sectbuf[512];
	uint16_t sectpos; //where the CPU is during the sector read process
} FDC_t;

uint8_t fdc_fiforead(FDC_t* fdc);
void fdc_fifoadd(FDC_t* fdc, uint8_t value);
void fdc_fifoclear(FDC_t* fdc);
void fdc_reset(FDC_t* fdc);
int fdc_insert(FDC_t* fdc, uint8_t num, char* dfile);
int fdc_init(FDC_t* fdc, CPU_t* cpu, I8259_t* i8259, I8237_t* i8237);

#endif
