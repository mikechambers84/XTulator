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
	Floppy disk controller
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../config.h"
#include "../../debuglog.h"
#include "../../cpu/cpu.h"
#include "../../ports.h"
#include "../../timing.h"
#include "../../chipset/i8259.h"
#include "../../chipset/i8237.h"
#include "fdc.h"

const uint8_t fdc_cmd_len[16] = {
	0, 0, 9, 3, 2, 9, 9, 2, 1, 9, 2, 0, 9, 6, 0, 3
};

const char* fdc_cmd_name[16] = {
	"INVALID",
	"INVALID",
	"Read complete track",
	"Fix drive data",
	"Check drive status",
	"Write sector",
	"Read sector",
	"Calibrate drive",
	"Check interrupt status",
	"Write deleted sector",
	"Read sector ID",
	"INVALID",
	"Read deleted sector",
	"Format track",
	"INVALID",
	"Seek/park head"
};

void fdc_cmd(FDC_t* fdc, uint8_t value) {
	uint8_t drv;

	if (fdc->busy) {
		return;
	}

#ifdef DEBUG_FDC
	if (fdc->cmd_pos == 0) {
		debug_log(DEBUG_DETAIL, "[FDC] Command: %s (%02Xh)\r\n", fdc_cmd_name[value & 0x0F], value & 0x0F);
	}
#endif

	if (fdc->cmd_pos < 9) {
		fdc->cmd[fdc->cmd_pos++] = value;
	}

	if (fdc->cmd_pos < (fdc_cmd_len[fdc->cmd[0] & 0x0F])) {
		return;
	}

	fdc_fifoclear(fdc);
	fdc->busy = 0;
	fdc->last_cmd = value & 0x0F;
	fdc->st[0] = FDC_ST0_INT_NORMAL | (fdc->disk[fdc->drivenum].inserted ? 0x00 : 0x08) | fdc->position[fdc->drivenum].head << 2 | fdc->drivenum;

	switch (fdc->cmd[0] & 0x0F) {
	case FDC_CMD_READ_TRACK:
		break;
	case FDC_CMD_SPECIFY:
		fdc->usedma = fdc->cmd[2] & 1;
		break;
	case FDC_CMD_SENSE_DRIVE_STATUS:
		break;
	case FDC_CMD_WRITE_DATA:
		break;
	case FDC_CMD_READ_DATA:
		drv = fdc->cmd[1] & 3;
		fdc->position[drv].wanttrack = fdc->cmd[2];
		fdc->position[drv].head = fdc->cmd[3];
		fdc->position[drv].sect = fdc->cmd[4];
		fdc->position[drv].seeking = 1;
		fdc->position[drv].reading = 0; //read flag gets set after we seek to correct track (see fdc_move function)
		fdc->position[drv].transferring = 0;
		fdc->busy = 1;
		break;
	case FDC_CMD_RECALIBRATE:
		drv = fdc->cmd[1] & 3;
		fdc->position[drv].wanttrack = 0;
		fdc->position[drv].seeking = 1;
		fdc->busy = 1;
		break;
	case FDC_CMD_SENSE_INTERRUPT:
		fdc_fifoadd(fdc, fdc->st[0]);
		fdc_fifoadd(fdc, fdc->position[fdc->drivenum].track);
		break;
	case FDC_CMD_WRITE_DELETED_DATA:
		break;
	case FDC_CMD_READ_ID:
		break;
	case FDC_CMD_READ_DELETED_DATA:
		break;
	case FDC_CMD_FORMAT_TRACK:
		break;
	case FDC_CMD_SEEK:
		drv = fdc->cmd[1] & 3;
		fdc->position[drv].head = (fdc->cmd[1] >> 2) & 1;
		fdc->position[drv].wanttrack = fdc->cmd[2];
		fdc->position[drv].seeking = 1;
		fdc->busy = 1;
		break;
	default: //invalid command
		fdc->st[0] = FDC_ST0_INT_INVALID;
		fdc_fifoadd(fdc, fdc->st[0]);
		break;
	}

	fdc->cmd_pos = 0;
}

void fdc_write(FDC_t* fdc, uint32_t addr, uint8_t value) {

#ifdef DEBUG_FDC
	debug_log(DEBUG_DETAIL, "[FDC] Write port %03X: %02X\r\n", addr, value);
#endif

	addr &= 7;

	switch (addr) {
	case 2: //Digital output register
		fdc->drivenum = value & 0x03;
		fdc->st[0] = (fdc->st[0] & 0xFC) | fdc->drivenum;
		fdc->st[3] = (fdc->st[0] & 0xFC) | fdc->drivenum;
		fdc->usedma = (value & 0x08) >> 3;
		fdc->motoron[0] = (value & 0x10) >> 4;
		fdc->motoron[1] = (value & 0x20) >> 5;
		fdc->motoron[2] = (value & 0x40) >> 6;
		fdc->motoron[3] = (value & 0x80) >> 7;
		if (!(fdc->reg[2] & 0x04) && (value & 0x04)) {
			fdc_reset(fdc);
		}
		break;
	case 5: //data register
		fdc_cmd(fdc, value);
		break;
	}

	fdc->reg[addr] = value;
}

uint8_t fdc_read(FDC_t* fdc, uint32_t addr) {
	uint8_t ret = 0xFF, drv;

#ifdef DEBUG_FDC
	debug_log(DEBUG_DETAIL, "[FDC] Read port %03X\r\n", addr);
#endif

	addr &= 7;

	switch (addr) {
	case 4: //main status register
		if (fdc->busy) {
			ret = 0x10;
		}
		else if ((fdc->fifolen - fdc->fifopos) > 1) { //TODO: is this right??? or check datatosend...
			ret = 0xD0;
		}
		else {
			ret = 0x80;
		}
		ret |= fdc->usedma ? 0x00 : 0x20;
		//ret |= ((fdc->cmd[0] & 0x0F) == 0x06) ? 0x10 : 0x00;
		for (drv = 0; drv < 4; drv++) {
			if (fdc->position[drv].seeking) {
				ret |= 1 << drv;
			}
		}
#ifdef DEBUG_FDC
		debug_log(DEBUG_DETAIL, "[FDC] Returned MSR value: %02X\r\n", ret);
#endif
		break;
	case 5: //data register
		ret = fdc_fiforead(fdc);
		break;
	}

	return ret;
}

uint8_t fdc_fiforead(FDC_t* fdc) {
	uint8_t ret = 0x00;

	if (fdc->fifopos == fdc->fifolen) {
		ret = 0x00;
	}
	else {
		ret = fdc->fifo[fdc->fifopos++];
	}

	if (fdc->fifopos == fdc->fifolen) {
		fdc_fifoclear(fdc);
	}

	return ret;
}

void fdc_fifoadd(FDC_t* fdc, uint8_t value) {
	if (fdc->fifolen == FDC_FIFO_LEN) {
		return;
	}

	fdc->fifo[fdc->fifolen++] = value;
	fdc->datatosend = 1;
}

void fdc_fifoclear(FDC_t* fdc) {
	fdc->fifolen = 0;
	fdc->fifopos = 0;
	fdc->datatosend = 0;
}

void fdc_move(FDC_t* fdc) {
	uint8_t drv;

	for (drv = 0; drv < 4; drv++) {
		if (fdc->position[drv].seeking) {
			if (fdc->position[drv].track < fdc->position[drv].wanttrack) {
				fdc->position[drv].track++;
			}
			else if (fdc->position[drv].track > fdc->position[drv].wanttrack) {
				fdc->position[drv].track--;
			}
			else { //got to requested track
				fdc->position[drv].seeking = 0;
				switch (fdc->cmd[0] & 0x0F) {
				case FDC_CMD_READ_DATA:
					fdc->position[drv].seeking = 0;
					fdc->position[drv].reading = 1;
					fdc->position[drv].transferring = 0;
					break;
				case FDC_CMD_RECALIBRATE:
				case FDC_CMD_SEEK:
					fdc->busy = 0;
					fdc->st[0] = FDC_ST0_INT_NORMAL | FDC_ST0_SE | (fdc->position[drv].head << 2) | drv;
					i8259_doirq(fdc->i8259, fdc->irq);
					break;
				}
#ifdef DEBUG_FDC
				debug_log(DEBUG_DETAIL, "[FDC] Completed drive %u seek to track %lu\r\n", drv, fdc->position[drv].track);
#endif
				break; //Don't allow more than one IRQ to be triggered by seeks at a time
			}

			if ((fdc->position[drv].track >= fdc->disk[drv].tracks) || (fdc->disk[drv].inserted == 0)) {
				fdc->position[drv].seeking = 0;
				fdc->busy = 0;
				fdc->st[0] = FDC_ST0_INT_ABNORMAL | FDC_ST0_UC | (fdc->position[drv].head << 2) | drv; //TODO: UC for no inserted disk, or use NR?
				i8259_doirq(fdc->i8259, fdc->irq);
				break; //Don't allow more than one IRQ to be triggered by seeks at a time
			}
		}
	}
}

//TODO: is this right?
void fdc_incrementsect(FDC_t* fdc, uint8_t drv) {
	fdc->position[drv].sect++;
	if (fdc->position[drv].sect > fdc->disk[drv].sectors) {
		fdc->position[drv].sect = 1;
		fdc->position[drv].head++;
	}
	if (fdc->position[drv].head >= fdc->disk[drv].sides) {
		fdc->position[drv].head = 0;
	}
}

void fdc_transfersector(FDC_t* fdc) {
	uint8_t drv;
	uint32_t lba, tracksize, filepos;

	for (drv = 0; drv < 4; drv++) {
		tracksize = fdc->disk[drv].sectors * 512;
		if (fdc->position[drv].transferring) {
			if (fdc->sectpos < 512) {
				if (fdc->usedma) {
					//printf("%c", fdc->sectbuf[fdc->sectpos]);
					i8237_write(fdc->i8237, fdc->dma, fdc->sectbuf[fdc->sectpos++]);
					//printf("FDC DMA write sectpos %u\r\n", fdc->sectpos);
				}
				else {
					if (fdc->fifopos == fdc->fifolen) { //TODO: doing PIO mode correctly?
						fdc_fifoclear(fdc);
						fdc_fifoadd(fdc, fdc->sectbuf[fdc->sectpos++]);
						i8259_doirq(fdc->i8259, fdc->irq);
						//printf("FDC PIO write sectpos %u\r\n", fdc->sectpos);
					}
				}
			}
			else if (fdc->sectpos == 512) {
				//fdc_incrementsect(fdc, drv);
				filepos = ftell(fdc->disk[drv].dfile);

				fdc->position[drv].transferring = 0;
				fdc->position[drv].reading = 0;
				fdc->busy = 0;
				fdc->st[0] = FDC_ST0_INT_NORMAL | FDC_ST0_SE | (fdc->position[drv].head << 2) | drv;
				fdc->st[1] = 0;
				fdc->st[2] = 0;
				fdc_fifoclear(fdc);
				fdc_fifoadd(fdc, fdc->st[0]);
				fdc_fifoadd(fdc, fdc->st[1]);
				fdc_fifoadd(fdc, fdc->st[2]);
				fdc_fifoadd(fdc, fdc->position[drv].track);
				fdc_fifoadd(fdc, fdc->position[drv].head);
				fdc_fifoadd(fdc, fdc->position[drv].sect);
				fdc_fifoadd(fdc, 2);
				i8259_doirq(fdc->i8259, fdc->irq);
#ifdef DEBUG_FDC
				debug_log(DEBUG_DETAIL, "[FDC] Finished sector transfer, raised IRQ 6\r\n");
#endif
				break;
			}
		}
		else if (fdc->position[drv].reading) {
			//printf("Reading drive %u\r\n", drv);
			lba = (fdc->position[drv].track * tracksize * 2) + (fdc->position[drv].head * tracksize) + ((fdc->position[drv].sect - 1) * 512);
			//printf("LBA = %lu\r\n", lba);
			fseek(fdc->disk[drv].dfile, SEEK_SET, lba);
			fread(fdc->sectbuf, 1, 512, fdc->disk[drv].dfile);
			fdc->position[drv].transferring = 1;
			fdc->sectpos = 0;
			fdc_fifoclear(fdc);
		}
	}
}

void fdc_reset(FDC_t* fdc) {
#ifdef DEBUG_FDC
	debug_log(DEBUG_DETAIL, "[FDC] Reset controller\r\n");
#endif

	i8259_doirq(fdc->i8259, fdc->irq);

	fdc_fifoclear(fdc);
	fdc->cmd_pos = 0;
}

int fdc_insert(FDC_t* fdc, uint8_t num, char* dfile) {
	int ret = 0;

	if (num > 1) {
		return -1;
	}

	fdc->disk[num].inserted = 0;

	if (fdc->disk[num].dfile != NULL) {
		fclose(fdc->disk[num].dfile);
	}

	fdc->disk[num].dfile = fopen(dfile, "rb");
	if (fdc->disk[num].dfile == NULL) {
		return -1;
	}

	fseek(fdc->disk[num].dfile, 0, SEEK_END);
	fdc->disk[num].size = ftell(fdc->disk[num].dfile);
	fseek(fdc->disk[num].dfile, 0, SEEK_SET);

	fdc->disk[num].tracks = 80;
	fdc->disk[num].sectors = 18;
	fdc->disk[num].sides = 2;
	if (fdc->disk[num].size <= 1228800) {
		fdc->disk[num].sectors = 15;
	}
	if (fdc->disk[num].size <= 737280) {
		fdc->disk[num].sectors = 9;
	}
	if (fdc->disk[num].size <= 368640) {
		fdc->disk[num].tracks = 40;
		fdc->disk[num].sectors = 9;
	}
	if (fdc->disk[num].size <= 184320) {
		fdc->disk[num].tracks = 40;
		fdc->disk[num].sectors = 9;
		fdc->disk[num].sides = 1;
	}
	if (fdc->disk[num].size <= 163840) {
		fdc->disk[num].tracks = 40;
		fdc->disk[num].sectors = 8;
		fdc->disk[num].sides = 1;
	}

	fdc->disk[num].inserted = 1;
#ifdef DEBUG_FDC
	debug_log(DEBUG_DETAIL, "[FDC] Inserted floppy: %s (%lu KB)\r\n", dfile, fdc->disk[num].size >> 10);
#endif

	return 0;
}

int fdc_init(FDC_t* fdc, CPU_t* cpu, I8259_t* i8259, I8237_t* i8237) {
	memset(fdc, 0, sizeof(FDC_t));

	fdc->st[0] = FDC_ST0_NR;

	fdc->cpu = cpu;
	fdc->i8259 = i8259;
	fdc->i8237 = i8237;
	fdc->irq = 6;
	fdc->dma = 2;

	fdc->timerseek = timing_addTimer(fdc_move, fdc, 50, TIMING_ENABLED);
	fdc->timerread = timing_addTimer(fdc_transfersector, fdc, 500000 / 8, TIMING_ENABLED);
	ports_cbRegister(0x3F0, 8, (void*)fdc_read, NULL, (void*)fdc_write, NULL, fdc);

	return 0;
}
