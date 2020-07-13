#ifndef _MENUS_H_
#define MENUS_H_

#include <stdint.h>
#ifdef _WIN32
#include <Windows.h>
#else
//TODO: Linux and Mac stuff
#endif
#include "cpu/cpu.h"

#define MENUS_FUNCTION		0
#define MENUS_SUBMENU		1
#define MENUS_SEPARATOR		2

#define MENUS_DISABLED		0
#define MENUS_HIDDEN		1
#define MENUS_ENABLED		2

typedef struct {
	wchar_t* title;
	uint8_t enabled;
	uint8_t type;
	void (*function)();
} MENU_t;

typedef struct {
	wchar_t* title;
	MENU_t* menu;
} MENUBAR_t;

int menus_init(HWND hwnd);
void menus_setCPU(CPU_t* cpu);
void menus_exit();
void menus_openFloppyFile(uint8_t disk);
void menus_changeFloppy0();
void menus_changeFloppy1();
void menus_ejectFloppy0();
void menus_ejectFloppy1();

#endif
