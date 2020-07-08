#ifndef _VGA_H_
#define _VGA_H_

#include <stdint.h>
#include "../../cpu/cpu.h"

typedef struct {
	uint8_t state;
	uint8_t index;
	uint8_t step;
	uint8_t pal[256][3];
} VGADAC_t;

extern uint8_t vga_palette[256][3];
extern volatile double vga_lockFPS;

int vga_init();
void vga_updateScanlineTiming();
void vga_update(uint32_t start_x, uint32_t start_y, uint32_t end_x, uint32_t end_y);
void vga_writeport(void* dummy, uint16_t port, uint8_t value);
uint8_t vga_readport(void* dummy, uint16_t port);
void vga_blinkCallback(void* dummy);
void vga_hblankCallback(void* dummy);
void vga_hblankEndCallback(void* dummy);
void vga_drawCallback(void* dummy);
void vga_renderThread(void* cpu);
void vga_writememory(void* dummy, uint32_t addr, uint8_t value);
uint8_t vga_readmemory(void* dummy, uint32_t addr);
void vga_dumpregs();

//#define cga_color(c) ((uint32_t)cga_palette[c][0] | ((uint32_t)cga_palette[c][1]<<8) | ((uint32_t)cga_palette[c][2]<<16))
#define vga_color(c) ((uint32_t)vga_palette[c][2] | ((uint32_t)vga_palette[c][1]<<8) | ((uint32_t)vga_palette[c][0]<<16))

#define vga_dorotate(v) ((uint8_t)((v >> vga_rotate) | (v << (8 - vga_rotate))))

#define VGA_DAC_MODE_READ	0x00
#define VGA_DAC_MODE_WRITE	0x03

#define VGA_REG_DATA_CURSOR_BEGIN			0x0A
#define VGA_REG_DATA_CURSOR_END				0x0B

#define VGA_MODE_TEXT						0
#define VGA_MODE_GRAPHICS_8BPP				1
#define VGA_MODE_GRAPHICS_4BPP				2
#define VGA_MODE_GRAPHICS_2BPP				3
#define VGA_MODE_GRAPHICS_1BPP				4

#endif
