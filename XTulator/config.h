#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

#define STR_TITLE "XTulator"
#define STR_VERSION "0.20.7.6"

//#define DEBUG_DMA
//#define DEBUG_VGA
//#define DEBUG_CGA
//#define DEBUG_PIT
//#define DEBUG_PIC
//#define DEBUG_PPI
//#define DEBUG_UART
//#define DEBUG_TCPMODEM
//#define DEBUG_PCSPEAKER
//#define DEBUG_MEMORY
//#define DEBUG_PORTS
//#define DEBUG_TIMING
//#define DEBUG_OPL2
//#define DEBUG_BLASTER
//#define DEBUG_FDC

//#define USE_FDC

#ifdef _WIN32
#define ENABLE_TCP_MODEM
#endif

#define VIDEO_CARD_MDA		0
#define VIDEO_CARD_CGA		1
#define VIDEO_CARD_EGA		2
#define VIDEO_CARD_VGA		3

#define SAMPLE_RATE		48000
#define SAMPLE_BUFFER	4800
//There are two alternating sample buffers, each of this size

//#define IBM_XT_BIOS

#ifdef _WIN32
#define FUNC_INLINE __forceinline
#else
#define FUNC_INLINE __attribute__((always_inline))
#endif

extern volatile uint8_t running;
extern uint8_t videocard;
extern double speed, speedarg;

#endif
