#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include "i8259.h"

#define UART_IRQ_MSR_ENABLE			0x08
#define UART_IRQ_LSR_ENABLE			0x04
#define UART_IRQ_TX_ENABLE			0x02
#define UART_IRQ_RX_ENABLE			0x01

#define UART_PENDING_RX				0x01
#define UART_PENDING_TX				0x02
#define UART_PENDING_MSR			0x04
#define UART_PENDING_LSR			0x08

typedef struct {
	uint8_t rx;
	uint8_t tx;
	uint8_t rxnew;
	uint8_t dlab;
	uint8_t ien; //interrupt enable register
	uint8_t iir; //interrupt identification register
	uint8_t lcr; //line control register
	uint8_t mcr; //modem control register
	uint8_t lsr; //line status register
	uint8_t msr; //modem status register
	uint8_t lastmsr; //to calculate delta bits
	uint8_t scratch;
	uint16_t divisor;
	uint8_t irq;
	uint8_t pendirq;
	void* udata;
	void* udata2;
	void (*txCb)(void*, uint8_t);
	void (*mcrCb)(void*, uint8_t);
	I8259_t* i8259;
} UART_t;

void uart_writeport(UART_t* uart, uint16_t addr, uint8_t value);
uint8_t uart_readport(UART_t* uart, uint16_t addr);
void uart_rxdata(UART_t* uart, uint8_t value);
void uart_init(UART_t* uart, I8259_t* i8259, uint16_t base, uint8_t irq, void (*tx)(void*, uint8_t), void* udata, void (*mcr)(void*, uint8_t), void* udata2);

#endif
