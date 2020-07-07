#ifndef _BIOSDISK_H_
#define _BIOSDISK_H_

#include <stdio.h>
#include <stdint.h>
#include "../../cpu/cpu.h"

typedef struct {
	FILE* diskfile;
	uint32_t filesize;
	uint16_t cyls;
	uint16_t sects;
	uint16_t heads;
	uint8_t inserted;
	char* filename;
} DISK_t;

uint8_t biosdisk_insert(CPU_t* cpu, uint8_t drivenum, char* filename);
void biosdisk_eject(CPU_t* cpu, uint8_t drivenum);
void biosdisk_int13h(CPU_t* cpu, uint8_t intnum);
void biosdisk_int19h(CPU_t* cpu, uint8_t intnum);
uint8_t biosdisk_gethdcount();
void biosdisk_init(CPU_t* cpu);

extern uint8_t bootdrive;
extern DISK_t biosdisk[4];

#endif
