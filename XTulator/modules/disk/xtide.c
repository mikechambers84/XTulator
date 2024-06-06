/*
  XTulator: A portable, open-source 80186 PC emulator.
  Copyright (C)2020-2024 Mike Chambers

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

#include <stdio.h>
#include <string.h> //for malloc
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
pthread_t vga_renderThreadID;
#endif
#include "xtide.h"
#include "../../config.h"
#include "../../timing.h"
#include "../../utility.h"
#include "../../ports.h"
#include "../../memory.h"
#include "../../debuglog.h"

struct xtide_status_s {
	uint8_t executing;
	uint8_t ready[2];
	uint8_t wfault[2];
	uint8_t seekcomplete[2];
	uint8_t bufsvc;
	uint8_t err;

	uint8_t sector_count;
	uint8_t sector_num;
	uint8_t cyl_low;
	uint8_t cyl_high;
	uint8_t head;
	uint8_t drive;
} xtide_status;

struct xtide_disk_s {
	uint32_t cyls;
	uint32_t heads;
	uint32_t sectors;

	char* filename;
	FILE* filehandle;
	uint8_t mounted;
	uint32_t filesize;
} xtide_disk[2];

uint8_t xtide_buf[5120];
uint16_t xtide_buf_len = 0;

void xtide_buf_put(uint8_t* data, uint16_t len) {
	if ((xtide_buf_len + len) > 5120) return;

	memcpy(&xtide_buf[xtide_buf_len], data, len);
	xtide_buf_len += len;
}

uint8_t xtide_buf_read() {
	uint8_t ret;
	
	if (xtide_buf_len == 0) return 0;

	ret = xtide_buf[0];
	memmove(xtide_buf, &xtide_buf[1], 5119);
	xtide_buf_len--;

	return ret;
}

void xtide_ascii_word(uint16_t* dst, char* str) {
	int len = strlen(str);
	int pos = 0;
	int i;
	for (i = 0; i < len; i+=2) {
		dst[pos++] = ((uint16_t)str[i] << 8) | str[i+1];
	}
}

void xtide_identify() {
	uint16_t* buf = xtide_buf; //data from this command is provided in words
	memset(xtide_buf, 0, 512);
	xtide_buf_len = 512;

	buf[0] =
		(1 << 6) | //Fixed drive
		(1 << 5) | //Spindle motor control option not implemented
		(1 << 3) | //Not MFM encoded
		(1 << 2) | //Not soft sectored
		(1 << 1);  //Hard sectored

	buf[1] = xtide_disk[xtide_status.drive].cyls;
	buf[3] = xtide_disk[xtide_status.drive].heads;
	//buf[4] = xtide_disk[xtide_status.drive].heads * xtide_disk[xtide_status.drive].sectors * 512;
	buf[5] = 512;
	buf[6] = xtide_disk[xtide_status.drive].sectors;
	xtide_ascii_word(&buf[0x0A], "123456789           "); //serial number
	buf[0x14] = 1; //single ported, single sector buffer
	buf[0x15] = 1; //buffer size in 512 byte increments
	buf[0x16] = 0; //ECC bytes on long operations
	xtide_ascii_word(&buf[0x17], "v1.00   "); //firmware revision
	xtide_ascii_word(&buf[0x1B], "XTulator virtual IDE disk               "); //model
}

void xtide_read_multiple() {
	uint32_t cyl, head, sect, numsects, lba;
	uint8_t disk;

	disk = xtide_status.drive;

	if (xtide_disk[disk].mounted == 0) {
		xtide_status.err = 1;
		return;
	}
	
	cyl = (uint32_t)xtide_status.cyl_low | ((uint32_t)xtide_status.cyl_high << 8);
	head = xtide_status.head;
	sect = xtide_status.sector_num;

	lba = ((uint32_t)cyl * (uint32_t)xtide_disk[disk].heads + (uint32_t)head) * (uint32_t)xtide_disk[disk].sectors + (uint32_t)sect;
	printf("disk seek: cyl %lu, head %lu, sect %lu\n", cyl, head, sect);
	fseek(xtide_disk[disk].filehandle, SEEK_SET, lba * 512UL);
	fread(xtide_buf, 1, 512, xtide_disk[disk].filehandle);
	xtide_buf_len = 512;
	xtide_status.err = 0;
}

void xtide_writeport(void* dummy, uint16_t port, uint8_t value) {
	//printf("XTIDE write port %Xh: %02X\n", port, value);

	port &= 7;
	switch (port) {
	case 1: //precomp
		break;
	case 2: //sector count
		xtide_status.sector_count = value;
		break;
	case 3: //sector number
		xtide_status.sector_num = value;
		break;
	case 4: //cylinder low
		xtide_status.cyl_low = value;
		break;
	case 5: //cylinder high
		xtide_status.cyl_high = value;
		break;
	case 6: //drive/head
		value &= 0x1F;
		xtide_status.drive = value >> 4;
		xtide_status.head = value & 0x0F;
		printf("XTIDE drive = %u, head = %u\n", xtide_status.drive, xtide_status.head);
		break;
	case 7: //command
		printf("XTIDE command = %02X\n", value);
		xtide_status.err = 0;
		switch (value) {
		case IDE_CMD_IDENTIFY:
			xtide_identify();
			break;
		case IDE_CMD_INITIALIZE_DRIVE_PARAMS:
			break;
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: //all mean seek
		case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
			break;
		case IDE_CMD_READ_MULTIPLE:
			xtide_read_multiple();
			break;
		default: //unsupported commands
			xtide_status.err = 1;
		}
		break;

	}
}

uint8_t xtide_readport(void* dummy, uint16_t port) {
	uint8_t ret = 0;
	static uint8_t blah = 0;
	//printf("XTIDE read port %Xh = ", port);

	port &= 7;
	switch (port) {
	case 0: //data
		ret = xtide_buf_read();
		break;
	case 1: //error
		ret = (xtide_status.err << 2);
		break;
	case 6: //drive/head
		break;
	case 7: //status
		if (xtide_disk[xtide_status.drive].mounted) ret |= (1 << 6); //drive ready
		if (xtide_buf_len > 0) ret |= (1 << 3); //sector buffer requires servicing
		break;

	}

	//printf("%02X\n", ret);
	return ret;
}

int xtide_mount(uint8_t disknum, char* filename) {
	if (disknum > 1) return 1;
	if ((filename == NULL) || (strlen(filename) == 0)) return 1;

	xtide_disk[disknum].filehandle = fopen(filename, "r+b");
	if (xtide_disk[disknum].filehandle == NULL) {
		xtide_disk[disknum].mounted = 0;
		debug_log(DEBUG_INFO, "[XTIDE] Failed to insert disk %u: %s\r\n", disknum, filename);
		return 1;
	}
	fseek(xtide_disk[disknum].filehandle, 0L, SEEK_END);
	xtide_disk[disknum].filesize = ftell(xtide_disk[disknum].filehandle);
	fseek(xtide_disk[disknum].filehandle, 0L, SEEK_SET);

	xtide_disk[disknum].sectors = 63;
	xtide_disk[disknum].heads = 16;
	xtide_disk[disknum].cyls = xtide_disk[disknum].filesize / (xtide_disk[disknum].sectors * xtide_disk[disknum].heads * 512);

	xtide_disk[disknum].filename = filename;
	xtide_disk[disknum].mounted = 1;
	printf("[XTIDE] Mounted disk %u: %s\n", disknum, filename);
	return 0;
}

int xtide_init() {
	ports_cbRegister(0x300, 8, (void*)xtide_readport, NULL, (void*)xtide_writeport, NULL, NULL);

	return;
}