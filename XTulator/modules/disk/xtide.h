#ifndef _XTIDE_H_
#define _XTIDE_H_

#define IDE_CMD_IDENTIFY					0xEC
#define IDE_CMD_INITIALIZE_DRIVE_PARAMS		0x91
#define IDE_CMD_READ_SECTORS_WITH_RETRY		0x20
#define IDE_CMD_READ_SECTORS				0x21

int xtide_mount(uint8_t disknum, char* filename);
int xtide_init();

#endif
