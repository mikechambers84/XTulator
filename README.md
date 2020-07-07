# XTulator - A portable, open source (GPLv2) 80186 PC emulator

### About

XTulator is an x86 PC emulator that is designed to run software that was written for Intel processors up to the 80186. It's able to run MS-DOS, FreeDOS, Windows 3.0, and many games of the era. It supports graphics up to the EGA/VGA standard, and emulates the Sound Blaster 2.0. It also emulates peripherals like the standard Microsoft-compatible serial mouse and a serial modem, which can simulate phone line connections via TCP.

This is actually a rewrite of an emulator I wrote many years ago, but has been on hiatus since 2013. It was poorly implemented, even though it worked fairly well. It had many hacks and a poor architecture, but most old 80186 software could still run under it.

But I've never been happy with it, so I am rewriting and/or refactoring all of the code to be more sane, accurate and (hopefully) readable. I'm aiming to create a much more modular architecture this time around and remove shared global variables wherever possible since the overuse of these in the original version turned the code into a mess.

### Re-write goals

- More sane architecture in general
- Minimize (preferably eliminate) use of shared globals
- Implement a sort of generic "virtual ISA card" interface
- Keep CPU and other modules as independent of each other as possible
- Improve accuracy of chipset and components emulation
- Maintain platform-independence as much as possible
- Implement proper hard drive and floppy disk controller interfaces, rather than "cheating" with HLE like before

### Current status

WARNING: This software is still currently in the early stages, and not really ready for general use but it's usable.

It supports multiple machine defitions which are selectable via the command line. (-machine option, use -h for help) Only the "generic_xt" machine is currently bootable. This is currently making use of the [Super PC/Turbo XT BIOS](http://www.phatcode.net/downloads.php?id=101) from [phatcode.net](http://www.phatcode.net) which is attributed to Ya'akov Miles and Jon Petrosky.

I hope to get the stock IBM XT and other BIOSes bootable in the near future. They don't seem to like something about my chipset implementation, which is my highest priority bug at the moment.

[You can download the current ROM set for the defined machines by clicking here.](https://gofile.io/d/HDBU6i) (Links to gofile.io)

Checkmarks below mean it's implemented enough to boot and run things with the "Turbo XT" BIOS. See comments for details.

- [x] CPU - Complete
- [x] Implement support for multiple machine defnitions (This exists, but only the generic_xt machine boots, the other BIOSes have issues. This is a high priority thing to fix)
- [x] Intel 8253 timer (Re-implemented, but needs some work to be fully accurate)
- [x] Intel 8259 interrupt controller (Working, also needs some more attention. This may be the cause of some of the BIOS issues.)
- [ ] Intel 8237 DMA controller (Partially implemented, same as above regarding BIOS boot issues)
- [ ] Intel 8255 PPI controller (Partially implemented)
- [x] Re-implement proper system timing
- [x] Re-implement proper video rendering
- [x] Rewrite and improve EGA/VGA handling from old emulator (EGA/VGA works now, with some bugs)
- [ ] Implement proper IDE and floppy controller interfaces (Work begun on FDC)
- [x] Keyboard input
- [ ] Mouse input  (Partially working. Works in DOS things, Windows 3.0 doesn't recognize it)
- [x] PC speaker (Works, but need to figure out why RealSound doesn't work in Links)
- [ ] Adlib/OPL2 (It's making noises, but not good noises)
- [x] Sound Blaster (SB 2.0 compatibility, but still need to add high speed DMA mode. Glitches with a couple of games I've tried, working on it...)
- [x] RTC (Need to fix this under non-Win32 OSes)
- [ ] Emulate a Hayes-compatible a serial modem using TCP (somewhat working, only in Windows)
- [ ] Novell NE1000/NE2000 ethernet
- [ ] Implement Intel 8080 emulation in the NEC V20 mode (low priority)


### Compiling

This project has been tested to compile in Visual Studio 2019, Debian 10 and MacOS, though I may occasionally break Linux/MacOS support as it's not my main platform and I don't always test it right now.

##### Windows (Visual Studio 2019)

The repository includes a Visual Studio 2019 solution and project. You will need to install the [SDL2](http://www.libsdl.org) dev library.

##### Linux

Sorry, there's no configure script or makefile yet, so you'l have to compile and link it by hand.

You will need the SDL2 dev library. On Debian/Ubuntu and related distributions, you can install it with the following line.

<pre><code>sudo apt-get install libsdl2-dev</code></pre>

After this, the following line should successfully compile the code.

<pre><code>gcc -O3 -o XTulator XTulator/*.c XTulator/chipset/*.c XTulator/cpu/cpu.c XTulator/modules/audio/*.c XTulator/modules/disk/*.c XTulator/modules/input/*.c XTulator/modules/io/*.c XTulator/modules/video/*.c -lm `sdl2-config --cflags --libs`</code></pre>


### Some screenshots

![Screenshot 1](https://i.imgur.com/Qkut2rl.png)

![Screenshot 2](https://i.imgur.com/uEgW0WN.png)

![Screenshot 3](https://i.imgur.com/JCkGRdO.png)

![Screenshot 4](https://i.imgur.com/69z2BwQ.png)

![Screenshot 5](https://i.imgur.com/ieLk41s.png)

![Screenshot 6](https://i.imgur.com/0CGsd1F.png)

![Screenshot 7](https://i.imgur.com/wKKxrFj.png)

![Screenshot 8](https://i.imgur.com/CvfuGic.png)
