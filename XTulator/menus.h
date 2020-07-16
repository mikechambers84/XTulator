#ifndef _MENUS_H_
#define MENUS_H_

#ifdef _WIN32
#include <stdint.h>
#include <Windows.h>
#include "machine.h"

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
void menus_setMachine(MACHINE_t* machine);
void menus_exit();
void menus_openFloppyFile(uint8_t disk);
void menus_changeFloppy0();
void menus_changeFloppy1();
void menus_ejectFloppy0();
void menus_ejectFloppy1();
void menus_insertHard0();
void menus_insertHard1();
void menus_setBootFloppy0();
void menus_setBootHard0();
void menus_reset();
void menus_speed477();
void menus_speed8();
void menus_speed10();
void menus_speed16();
void menus_speed25();
void menus_speed50();
void menus_speedunlimited();

#endif //_WIN32

#endif //_MENUS_H_
