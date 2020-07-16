/*
  XTulator: A portable, open-source 80186 PC emulator.
  Copyright (C)2020 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*

	NOTE: I consider HLE of the disk system at the BIOS level a hack.
	I want to get rid of this and implement proper FDC and HDC controllers.
	This is here for testing purposes of the rest of the system until
	I get around to that...

*/

#include <stdio.h>
#include <stdint.h>
#include "biosdisk.h"
#include "../../cpu/cpu.h"
#include "../../debuglog.h"

DISK_t biosdisk[4];
uint8_t biosdisk_sectbuf[512];

uint8_t bootdrive = 0xFF;

uint8_t biosdisk_insert(CPU_t* cpu, uint8_t drivenum, char* filename) {
	debug_log(DEBUG_INFO, "[BIOSDISK] Inserting disk %u: %s\r\n", drivenum, filename);
	if (biosdisk[drivenum].inserted) fclose(biosdisk[drivenum].diskfile);
	biosdisk[drivenum].inserted = 1;
	biosdisk[drivenum].diskfile = fopen(filename, "r+b");
	if (biosdisk[drivenum].diskfile == NULL) {
		biosdisk[drivenum].inserted = 0;
		debug_log(DEBUG_INFO, "[BIOSDISK] Failed to insert disk %u: %s\r\n", drivenum, filename);
		return 1;
	}
	fseek(biosdisk[drivenum].diskfile, 0L, SEEK_END);
	biosdisk[drivenum].filesize = ftell(biosdisk[drivenum].diskfile);
	fseek(biosdisk[drivenum].diskfile, 0L, SEEK_SET);
	if (drivenum >= 2) { //it's a hard disk image
		biosdisk[drivenum].sects = 63;
		biosdisk[drivenum].heads = 16;
		biosdisk[drivenum].cyls = biosdisk[drivenum].filesize / (biosdisk[drivenum].sects * biosdisk[drivenum].heads * 512);
		cpu_write(cpu, 0x475, biosdisk_gethdcount());
	}
	else {   //it's a floppy image
		biosdisk[drivenum].cyls = 80;
		biosdisk[drivenum].sects = 18;
		biosdisk[drivenum].heads = 2;
		if (biosdisk[drivenum].filesize <= 1228800) biosdisk[drivenum].sects = 15;
		if (biosdisk[drivenum].filesize <= 737280) biosdisk[drivenum].sects = 9;
		if (biosdisk[drivenum].filesize <= 368640) {
			biosdisk[drivenum].cyls = 40;
			biosdisk[drivenum].sects = 9;
		}
		if (biosdisk[drivenum].filesize <= 163840) {
			biosdisk[drivenum].cyls = 40;
			biosdisk[drivenum].sects = 8;
			biosdisk[drivenum].heads = 1;
		}
	}
	return 0;
}

void biosdisk_eject(CPU_t* cpu, uint8_t drivenum) {
	biosdisk[drivenum].inserted = 0;
	if (drivenum >= 2) {
		cpu_write(cpu, 0x475, biosdisk_gethdcount());
	}
	if (biosdisk[drivenum].diskfile != NULL) fclose(biosdisk[drivenum].diskfile);
}

void biosdisk_read(CPU_t* cpu, uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head, uint16_t sectcount) {
	uint32_t memdest, lba, fileoffset, cursect, sectoffset;
	if (!sect || !biosdisk[drivenum].inserted) return;
	lba = ((uint32_t)cyl * (uint32_t)biosdisk[drivenum].heads + (uint32_t)head) * (uint32_t)biosdisk[drivenum].sects + (uint32_t)sect - 1UL;
	fileoffset = lba * 512UL;
	if (fileoffset > biosdisk[drivenum].filesize) return;
	fseek(biosdisk[drivenum].diskfile, fileoffset, SEEK_SET);
	memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
	for (cursect = 0; cursect < sectcount; cursect++) {
		if (fread(biosdisk_sectbuf, 1, 512, biosdisk[drivenum].diskfile) < 512) break;
		for (sectoffset = 0; sectoffset < 512; sectoffset++) {
			cpu_write(cpu, memdest++, biosdisk_sectbuf[sectoffset]);
		}
	}
	cpu->regs.byteregs[regal] = cursect;
	cpu->cf = 0;
	cpu->regs.byteregs[regah] = 0;
}

void biosdisk_write(CPU_t* cpu, uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head, uint16_t sectcount) {
	uint32_t memdest, lba, fileoffset, cursect, sectoffset;
	if (!sect || !biosdisk[drivenum].inserted) return;
	lba = ((uint32_t)cyl * (uint32_t)biosdisk[drivenum].heads + (uint32_t)head) * (uint32_t)biosdisk[drivenum].sects + (uint32_t)sect - 1UL;
	fileoffset = lba * 512UL;
	if (fileoffset > biosdisk[drivenum].filesize) return;
	fseek(biosdisk[drivenum].diskfile, fileoffset, SEEK_SET);
	memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
	for (cursect = 0; cursect < sectcount; cursect++) {
		for (sectoffset = 0; sectoffset < 512; sectoffset++) {
			biosdisk_sectbuf[sectoffset] = cpu_read(cpu, memdest++);
		}
		fwrite(biosdisk_sectbuf, 1, 512, biosdisk[drivenum].diskfile);
	}
	cpu->regs.byteregs[regal] = (uint8_t)sectcount;
	cpu->cf = 0;
	cpu->regs.byteregs[regah] = 0;
}

void biosdisk_int19h(CPU_t* cpu, uint8_t intnum) {
	if (intnum != 0x19) return;
	
	cpu_write(cpu, 0x475, biosdisk_gethdcount());

	//put "STI" and then "JMP -1" code at bootloader location in case nothing gets read from disk
	cpu_write(cpu, 0x07C00, 0xFB);
	cpu_write(cpu, 0x07C01, 0xEB);
	cpu_write(cpu, 0x07C02, 0xFE);

	cpu->regs.byteregs[regdl] = bootdrive;
	biosdisk_read(cpu, (cpu->regs.byteregs[regdl] & 0x80) ? cpu->regs.byteregs[regdl] - 126 : cpu->regs.byteregs[regdl], 0x0000, 0x7C00, 0, 1, 0, 1);
	cpu->segregs[regcs] = 0x0000;
	cpu->ip = 0x7C00;
}

void biosdisk_int13h(CPU_t* cpu, uint8_t intnum) {
	static uint8_t lastah = 0, lastcf = 0;
	uint8_t curdisk;

	if (intnum != 0x13) return;

	curdisk = cpu->regs.byteregs[regdl];
	if (curdisk & 0x80) curdisk = curdisk - 126;
	if (curdisk > 3) {
		cpu->cf = 1;
		cpu->regs.byteregs[regah] = 1;
		return;
	}

	switch (cpu->regs.byteregs[regah]) {
	case 0: //reset disk system
		cpu->regs.byteregs[regah] = 0;
		cpu->cf = 0; //useless function in an emulator. say success and return.
		break;
	case 1: //return last status
		cpu->regs.byteregs[regah] = lastah;
		cpu->cf = lastcf;
		return;
	case 2: //read sector(s) into memory
		if (biosdisk[curdisk].inserted) {
			biosdisk_read(cpu, curdisk, cpu->segregs[reges], getreg16(cpu, regbx), (uint16_t)cpu->regs.byteregs[regch] + ((uint16_t)cpu->regs.byteregs[regcl] / 64) * 256, (uint16_t)cpu->regs.byteregs[regcl] & 63, (uint16_t)cpu->regs.byteregs[regdh], (uint16_t)cpu->regs.byteregs[regal]);
			cpu->cf = 0;
			cpu->regs.byteregs[regah] = 0;
		}
		else {
			cpu->cf = 1;
			cpu->regs.byteregs[regah] = 1;
		}
		break;
	case 3: //write sector(s) from memory
		if (biosdisk[curdisk].inserted) {
			biosdisk_write(cpu, curdisk, cpu->segregs[reges], getreg16(cpu, regbx), (uint16_t)cpu->regs.byteregs[regch] + ((uint16_t)cpu->regs.byteregs[regcl] / 64) * 256, (uint16_t)cpu->regs.byteregs[regcl] & 63, (uint16_t)cpu->regs.byteregs[regdh], (uint16_t)cpu->regs.byteregs[regal]);
			cpu->cf = 0;
			cpu->regs.byteregs[regah] = 0;
		}
		else {
			cpu->cf = 1;
			cpu->regs.byteregs[regah] = 1;
		}
		break;
	case 4:
	case 5: //format track
		cpu->cf = 0;
		cpu->regs.byteregs[regah] = 0;
		break;
	case 8: //get drive parameters
		if (biosdisk[curdisk].inserted) {
			cpu->cf = 0;
			cpu->regs.byteregs[regah] = 0;
			cpu->regs.byteregs[regch] = biosdisk[curdisk].cyls - 1;
			cpu->regs.byteregs[regcl] = biosdisk[curdisk].sects & 63;
			cpu->regs.byteregs[regcl] = cpu->regs.byteregs[regcl] + (biosdisk[curdisk].cyls / 256) * 64;
			cpu->regs.byteregs[regdh] = biosdisk[curdisk].heads - 1;
			if (curdisk < 2) {
				cpu->regs.byteregs[regbl] = 4; //else regs.byteregs[regbl] = 0;
				cpu->regs.byteregs[regdl] = 2;
			}
			else cpu->regs.byteregs[regdl] = biosdisk_gethdcount();
		}
		else {
			cpu->cf = 1;
			cpu->regs.byteregs[regah] = 0xAA;
		}
		break;
	default:
		cpu->cf = 1;
	}
	lastah = cpu->regs.byteregs[regah];
	lastcf = cpu->cf;
	if (cpu->regs.byteregs[regdl] & 0x80) cpu_write(cpu, 0x474, cpu->regs.byteregs[regah]);
}

uint8_t biosdisk_gethdcount() {
	uint8_t ret = 0, i;

	for (i = 2; i < 4; i++) {
		if (biosdisk[i].inserted) ret++;
	}
	return ret;
}

void biosdisk_init(CPU_t* cpu) {
	cpu_registerIntCallback(cpu, 0x13, biosdisk_int13h);
	cpu_registerIntCallback(cpu, 0x19, biosdisk_int19h);
}
