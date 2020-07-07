#ifndef _CGA_H_
#define _CGA_H_

#include <stdint.h>
#include "../../cpu/cpu.h"

extern const uint8_t cga_palette[16][3];

int cga_init();
void cga_update(uint32_t start_x, uint32_t start_y, uint32_t end_x, uint32_t end_y);
void cga_writeport(void* dummy, uint16_t port, uint8_t value);
uint8_t cga_readport(void* dummy, uint16_t port);
void cga_blinkCallback(void* dummy);
void cga_scanlineCallback(void* dummy);
void cga_renderThread(void* cpu);
void cga_writememory(void* dummy, uint32_t addr, uint8_t value);
uint8_t cga_readmemory(void* dummy, uint32_t addr);
void cga_drawCallback(void* dummy);

//#define cga_color(c) ((uint32_t)cga_palette[c][0] | ((uint32_t)cga_palette[c][1]<<8) | ((uint32_t)cga_palette[c][2]<<16))
#define cga_color(c) ((uint32_t)cga_palette[c][2] | ((uint32_t)cga_palette[c][1]<<8) | ((uint32_t)cga_palette[c][0]<<16))

#define CGA_BLACK			0
#define CGA_BLUE			1
#define CGA_GREEN			2
#define CGA_CYAN			3
#define CGA_RED				4
#define CGA_MAGENTA			5
#define CGA_BROWN			6
#define CGA_LIGHT_GRAY		7
#define CGA_DARK_GRAY		8
#define CGA_LIGHT_BLUE		9
#define CGA_LIGHT_GREEN		10
#define CGA_LIGHT_CYAN		11
#define CGA_LIGHT_RED		12
#define CGA_LIGHT_MAGENTA	13
#define CGA_YELLOW			14
#define CGA_WHITE			15

#define CGA_REG_DATA_CURSOR_BEGIN			0x0A
#define CGA_REG_DATA_CURSOR_END				0x0B

#define CGA_MODE_TEXT_40X25					0
#define CGA_MODE_TEXT_80X25					1
#define CGA_MODE_GRAPHICS_LO				2
#define CGA_MODE_GRAPHICS_HI				3

#endif
