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
	Emulates the 8250 UART.
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../config.h"
#include "../debuglog.h"
#include "i8259.h"
#include "../ports.h"
#include "uart.h"

const uint8_t uart_wordmask[4] = { 0x1F, 0x3F, 0x7F, 0xFF }; //5, 6, 7, or 8 bit words based on bits 1-0 in LCR

void uart_writeport(UART_t* uart, uint16_t addr, uint8_t value) {
#ifdef DEBUG_UART
	debug_log(DEBUG_DETAIL, "[UART] Write %03X: %u\r\n", addr, value);
#endif

	addr &= 0x07;

	switch (addr) {
	case 0x00:
		if (uart->dlab == 0) {
			uart->tx = value & uart_wordmask[uart->lcr & 0x03];
			if (uart->mcr & 0x10) { //loopback mode
				uart_rxdata(uart, uart->tx);
			} else {
				if (uart->txCb != NULL) {
					(*uart->txCb)(uart->udata, uart->tx);
					if (uart->ien & UART_IRQ_TX_ENABLE) {
						uart->pendirq |= UART_PENDING_TX;
						i8259_doirq(uart->i8259, uart->irq);
					}
					if (uart->ien & UART_IRQ_LSR_ENABLE) {
						uart->pendirq |= UART_PENDING_LSR;
						i8259_doirq(uart->i8259, uart->irq);
					}
				}
			}
		} else {
			uart->divisor = (uart->divisor & 0xFF00) | value;
		}
		break;
	case 0x01: //IEN
		if (uart->dlab == 0) {
			uart->ien = value;
		} else {
			uart->divisor = (uart->divisor & 0x00FF) | ((uint16_t)value << 8);
		}
		break;
	case 0x03: //LCR
		uart->lcr = value;
		uart->dlab = value >> 7;
		break;
	case 0x04: //MCR
		uart->mcr = value;
		if (uart->mcrCb != NULL) {
			(*uart->mcrCb)(uart->udata2, value);
		}
		break;
	case 0x07:
		uart->scratch = value;
		break;
	}
}

uint8_t uart_readport(UART_t* uart, uint16_t addr) {
	uint8_t ret = 0; // xFF;

#ifdef DEBUG_UART
	debug_log(DEBUG_DETAIL, "[UART] Read %03X\r\n", addr);
#endif
	addr &= 0x07;
	
	switch (addr) {
	case 0x00:
		if (uart->dlab == 0) {
			ret = uart->rx;
			uart->rxnew = 0;
			uart->pendirq &= ~UART_PENDING_RX;
			if (uart->ien & UART_IRQ_LSR_ENABLE) {
				uart->pendirq |= UART_PENDING_LSR;
				i8259_doirq(uart->i8259, uart->irq);
			}
		} else {
			ret = (uint8_t)uart->divisor;
		}
		break;
	case 0x01: //IEN
		if (uart->dlab == 0) {
			ret = uart->ien;
		} else {
			ret = (uint8_t)(uart->divisor >> 8);
		}
		break;
	case 0x02: //IIR
		ret = uart->pendirq ? 0x00 : 0x01;
		if (uart->pendirq & UART_PENDING_LSR) {
			ret |= 0x06;
		}
		else if (uart->pendirq & UART_PENDING_RX) {
			ret |= 0x04;
		}
		else if (uart->pendirq & UART_PENDING_TX) {
			ret |= 0x02;
			uart->pendirq &= ~UART_PENDING_TX;
		}
		else if (uart->pendirq & UART_PENDING_MSR) {
			//nothing to do
		}
		if (uart->pendirq) {
			i8259_doirq(uart->i8259, uart->irq);
		}
		break;
	case 0x03: //LCR
		ret = uart->lcr;
		break;
	case 0x04: //MCR
		ret = uart->mcr;
		break;
	case 0x05: //LSR
		ret = 0x60; //transmit register always report empty in emulator
		ret |= uart->rxnew ? 0x01 : 0x00;
		uart->pendirq &= ~UART_PENDING_LSR;
		break;
	case 0x06: //MSR
		ret = uart->msr & 0xF0;
		//calculate deltas:
		ret |= ((uart->msr & 0x80) != (uart->lastmsr & 0x80)) ? 0x08 : 0x00;
		ret |= ((uart->msr & 0x20) != (uart->lastmsr & 0x20)) ? 0x02 : 0x00;
		ret |= ((uart->msr & 0x10) != (uart->lastmsr & 0x10)) ? 0x01 : 0x00;
		uart->lastmsr = uart->msr;
		uart->pendirq &= ~UART_PENDING_MSR;
		break;
	case 0x07:
		ret = 0xFF; // uart->scratch;
		break;
	}

	return ret;
}

void uart_rxdata(UART_t* uart, uint8_t value) {
	uart->rx = value;
	uart->rxnew = 1;
	if (uart->ien & UART_IRQ_RX_ENABLE) {
		uart->pendirq |= UART_PENDING_RX;
		i8259_doirq(uart->i8259, uart->irq);
	}
}

void uart_init(UART_t* uart, I8259_t* i8259, uint16_t base, uint8_t irq, void (*tx)(void*, uint8_t), void* udata, void (*mcr)(void*, uint8_t), void* udata2) {
	debug_log(DEBUG_INFO, "[UART] Initializing 8250 UART at base port 0x%03X, IRQ %u\r\n", base, irq);
	memset(uart, 0, sizeof(UART_t));
	uart->i8259 = i8259;
	uart->irq = irq;
	uart->udata = udata;
	uart->txCb = tx;
	uart->udata2 = udata2;
	uart->mcrCb = mcr;
	uart->msr = 0x30;
	ports_cbRegister(base, 8, (void*)uart_readport, NULL, (void*)uart_writeport, NULL, uart);
}
