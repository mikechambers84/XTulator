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
	Emulates a modem over TCP connections.

	This code is hilariously ugly. Fix that.
*/

#include "../../config.h"
#ifdef ENABLE_TCP_MODEM
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../debuglog.h"
#include "../../chipset/uart.h"
#include "../../chipset/i8259.h"
#include "../../timing.h"
#include "tcpmodem.h"

int tcpmodem_listen(TCPMODEM_t* tcpmodem, uint16_t port) {
	int ret;
	struct addrinfo hints, *result = NULL;
	unsigned long iMode = 1;
	char portstr[16];

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_addr = INADDR_ANY;

	sprintf(portstr, "%u", port);

	closesocket(tcpmodem->serversocket);

	ret = getaddrinfo(NULL, portstr, &hints, &result);
	if (ret != 0) {
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] getaddrinfo error: %d\r\n", ret);
#endif
		return -1;
	}

	tcpmodem->serversocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (tcpmodem->serversocket == INVALID_SOCKET) {
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] Could not create socket to listen on: error %d\r\n", WSAGetLastError());
#endif
		return -1;
	}
	ioctlsocket(tcpmodem->serversocket, FIONBIO, &iMode);

	ret = bind(tcpmodem->serversocket, result->ai_addr, result->ai_addrlen);
	if (ret == SOCKET_ERROR) {
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] bind error %d\r\n", WSAGetLastError());
#endif
		closesocket(tcpmodem->socket);
		return -1;
	}

	ret = listen(tcpmodem->serversocket, 1);
	if (ret == SOCKET_ERROR) {
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] listen error\r\n");
#endif
		closesocket(tcpmodem->serversocket);
		return -1;
	}

#ifdef DEBUG_TCPMODEM
	debug_log(DEBUG_INFO, "[TCPMODEM] Listening for connections on port %u\r\n", port);
#endif
	tcpmodem->listening = 1;

	return 0;
}

void tcpmodem_msrirq(TCPMODEM_t* tcpmodem) {
	if (tcpmodem->uart->ien & UART_IRQ_MSR_ENABLE) {
		tcpmodem->uart->pendirq |= UART_PENDING_MSR;
	}
}

int tcpmodem_connect(TCPMODEM_t* tcpmodem, char* host, uint16_t port) {
	int ret;
	unsigned long iMode = 1;
	HOSTENT* hostent;

	tcpmodem->rxpos = 0;

	tcpmodem->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (tcpmodem->socket == INVALID_SOCKET) {
		WSACleanup();
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] Unable to prepare a socket\r\n");
#endif
		return -1;
	}

	hostent = gethostbyname(host);
	if (hostent == NULL) {
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] Error returned from gethostbyname(\"%s\")\r\n", host);
#endif
		sprintf(tcpmodem->rxbuf, "\nNO CARRIER\r\n");
		return -1;
	}

	tcpmodem->server.sin_family = AF_INET;
	tcpmodem->server.sin_port = htons(port);
	tcpmodem->server.sin_addr.s_addr = *(uint32_t*)hostent->h_addr_list[0]; //inet_addr(host);
	//printf("%08X\r\n", tcpmodem->server.sin_addr.s_addr);

	ret = connect(tcpmodem->socket, (SOCKADDR*)&tcpmodem->server, sizeof(tcpmodem->server));
	if (ret == SOCKET_ERROR) {
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_ERROR, "[TCPMODEM] No connect\r\n");
#endif
		closesocket(tcpmodem->socket);
		sprintf(tcpmodem->rxbuf, "\nNO CARRIER\r\n");
		return -1;
	}

	iMode = 1;
	ioctlsocket(tcpmodem->socket, FIONBIO, &iMode);

	sprintf(tcpmodem->rxbuf, "\nCONNECT\r\n");
	tcpmodem->livesocket = 1;
	tcpmodem->escaped = 0;
	tcpmodem->listening = 0;
	closesocket(tcpmodem->serversocket);
	tcpmodem->uart->msr |= 0x80; //data carrier detect
	tcpmodem->uart->msr &= 0xF7; //delta data carrier detect
	tcpmodem_msrirq(tcpmodem);

	return 0;
}

void tcpmodem_setringmsr(TCPMODEM_t* tcpmodem, uint8_t state) {
	if (tcpmodem->ringstate) {
		tcpmodem->uart->msr |= 0x40;
		tcpmodem->uart->msr &= 0xFB;
	} else {
		tcpmodem->uart->msr &= 0xBF;
		tcpmodem->uart->msr |= 0x04;
	}
	tcpmodem_msrirq(tcpmodem);
}

void tcpmodem_offline(TCPMODEM_t* tcpmodem) {
	closesocket(tcpmodem->socket);
	tcpmodem->livesocket = 0;
	tcpmodem->escaped = 1;
	sprintf(tcpmodem->rxbuf, "\nNO CARRIER\r\n");
	tcpmodem->rxpos = 0;
	tcpmodem->listening = 0;
	tcpmodem->ringing = 0;
	tcpmodem->uart->msr &= 0x7F; //data carrier detect
	tcpmodem->uart->msr |= 0x08; //delta data carrier detect
	tcpmodem_msrirq(tcpmodem);
	tcpmodem_listen(tcpmodem, tcpmodem->listenport);
}

void tcpmodem_parseAT(TCPMODEM_t* tcpmodem) {
	uint16_t i = 0, port = 23, hostpos = 0, gethost = 0, isdial = 0;
	char host[1024];
	if ((tcpmodem->txbuf[0] != 'A') || (tcpmodem->txbuf[1] != 'T')) return;
	if (tcpmodem->txbuf[2] == 'D') {
		if (tcpmodem->txbuf[3] == 'T') {
			i = 4;
		} else {
			i = 3;
		}
		isdial = 1;
	}

	if (tcpmodem->txbuf[2] == 'H') {
		if (tcpmodem->livesocket) {
			tcpmodem_offline(tcpmodem);
		} else {
			sprintf(tcpmodem->rxbuf, "\nOK\r\n");
			tcpmodem->rxpos = 0;
		}
		return;
	}

	if (tcpmodem->txbuf[2] == 'A') {
		if (tcpmodem->ringing) {
			timing_timerDisable(tcpmodem->ringtimer);
			tcpmodem->ringing = 0;
			tcpmodem->escaped = 0;
			tcpmodem->uart->msr |= 0x80; //data carrier detect
			tcpmodem->uart->msr &= 0xF7; //delta data carrier detect
			tcpmodem_msrirq(tcpmodem);				
			sprintf(tcpmodem->rxbuf, "\nCONNECT\r\n");
			tcpmodem->rxpos = 0;
		}
		else {
			sprintf(tcpmodem->rxbuf, "\nOK\r\n");
			tcpmodem->rxpos = 0;
		}
		return;
	}

	if (tcpmodem->txbuf[2] == 'E') {
		tcpmodem->echocmd = (tcpmodem->txbuf[3] - '0') & 1;
		sprintf(tcpmodem->rxbuf, "%u\r\n", tcpmodem->echocmd);
		tcpmodem->rxpos = 0;
	}

	if (isdial) {
		for (; i < tcpmodem->txpos; i++) {
			uint8_t cc;
			cc = tcpmodem->txbuf[i];
			if (cc == ':') {
				port = (uint16_t)atol(&tcpmodem->txbuf[i + 1]);
				break;
			}
			host[hostpos++] = cc;
		}
		host[hostpos++] = 0;
#ifdef DEBUG_TCPMODEM
		debug_log(DEBUG_DETAIL, "[TCPMODEM] Connect to %s (port %u)\r\n", host, port);
#endif
		tcpmodem_connect(tcpmodem, host, port);
	} else {
		sprintf(tcpmodem->rxbuf, "\nOK\r\n");
		tcpmodem->rxpos = 0;
	}
}

void tcpmodem_rxpoll(TCPMODEM_t* tcpmodem) {
	char cc;
	int ret;

	if (tcpmodem->uart->rxnew) return;

	if (tcpmodem->livesocket && !(tcpmodem->uart->mcr & 1)) { //software hung up via DTR toggle
		tcpmodem_offline(tcpmodem);
	}

	if (tcpmodem->livesocket && !tcpmodem->escaped && (tcpmodem->rxbuf[tcpmodem->rxpos] == 0)) {
		ret = recv(tcpmodem->socket, &cc, 1, 0);
		//printf("%d\r\n", ret);
		if (ret > 0) {
			uart_rxdata(tcpmodem->uart, (uint8_t)cc);
			//printf("%c", cc);
		} else if (ret == SOCKET_ERROR) {
			switch (WSAGetLastError()) {
				//case 10035:
			case WSAENETDOWN:
			case WSAENETUNREACH:
			case WSAENETRESET:
			case WSAECONNABORTED:
			case WSAECONNRESET:
			case WSAENOTCONN:
			case WSAEHOSTUNREACH:
				tcpmodem_offline(tcpmodem);
				break;
			}
		}
	} else { //no live socket connection
		cc = tcpmodem->rxbuf[tcpmodem->rxpos];
		if (cc != 0) {
			uart_rxdata(tcpmodem->uart, (uint8_t)cc);
			tcpmodem->rxpos++;
		} else {
			memset(tcpmodem->rxbuf, 0, 1024);
			tcpmodem->rxpos = 0;
		}

		if (tcpmodem->listening) {
			closesocket(tcpmodem->socket);
			tcpmodem->socket = accept(tcpmodem->serversocket, NULL, NULL);
			if ((tcpmodem->socket == INVALID_SOCKET) && (WSAGetLastError() != WSAEWOULDBLOCK)) {
#ifdef DEBUG_TCPMODEM
				debug_log(DEBUG_DETAIL, "[TCPMODEM] Accept error: %d\r\n", WSAGetLastError());
#endif
				tcpmodem_listen(tcpmodem, tcpmodem->listenport);
			} else if (tcpmodem->socket != INVALID_SOCKET) {
				closesocket(tcpmodem->serversocket);
				tcpmodem->livesocket = 1;
				tcpmodem->escaped = 1;
				tcpmodem->listening = 0;
				tcpmodem->ringing = 1;
				tcpmodem->ringstate = 0;
				timing_timerEnable(tcpmodem->ringtimer);
			}
		}
	}
}

void tcpmodem_tx(TCPMODEM_t* tcpmodem, uint8_t value) {
	int ret;

	tcpmodem->lasttx[0] = tcpmodem->lasttx[1];
	tcpmodem->lasttx[1] = tcpmodem->lasttx[2];
	tcpmodem->lasttx[2] = (char)value;
	if ((tcpmodem->lasttx[0] == '+') &&
		(tcpmodem->lasttx[1] == '+') &&
		(tcpmodem->lasttx[2] == '+') &&
		tcpmodem->livesocket) {
		tcpmodem->lasttx[0] = tcpmodem->lasttx[1] = tcpmodem->lasttx[2] = 0;
		tcpmodem->escaped ^= 1;
		if (tcpmodem->escaped) {
			sprintf(tcpmodem->rxbuf, "\nOK\r\n");
			tcpmodem->rxpos = 0;
		}
	}

	if (tcpmodem->livesocket && !tcpmodem->escaped) {
		ret = send(tcpmodem->socket, &value, 1, 0);
		if (ret == SOCKET_ERROR) {
			switch (WSAGetLastError()) {
				//case 10035:
			case WSAENETDOWN:
			case WSAENETUNREACH:
			case WSAENETRESET:
			case WSAECONNABORTED:
			case WSAECONNRESET:
			case WSAENOTCONN:
			case WSAEHOSTUNREACH:
				tcpmodem_offline(tcpmodem);
				break;
			}
		}
	} else {
		if (tcpmodem->echocmd) {
			uart_rxdata(tcpmodem->uart, value);
		}
		if (value == 8) {
			if (tcpmodem->txpos > 0) {
				tcpmodem->txpos--;
			}
		}
		else if (value == 13) {
			tcpmodem->txbuf[tcpmodem->txpos + 1] = 0;
			tcpmodem_parseAT(tcpmodem);
			tcpmodem->txpos = 0;
		}
		else if (tcpmodem->txpos < 1023) {
			switch (value) {
			case 0:
			case 32:
			case '+':
				break;
			default:
				if ((value >= 'a') && (value <= 'z')) {
					value -= 'a' - 'A';
				}
				tcpmodem->txbuf[tcpmodem->txpos++] = value;
				break;
			}
		}
	}
}

void tcpmodem_ringer(TCPMODEM_t* tcpmodem) {
	if (tcpmodem->ringing == 0) return;

	tcpmodem->ringstate ^= 1;
	if (tcpmodem->ringstate) {
		sprintf(tcpmodem->rxbuf, "RING\r\n");
		tcpmodem->rxpos = 0;
	}
	tcpmodem_setringmsr(tcpmodem, tcpmodem->ringstate);
}

int tcpmodem_init(TCPMODEM_t* tcpmodem, UART_t* uart, uint16_t port) {
	debug_log(DEBUG_INFO, "[TCPMODEM] Initializing TCP serial modem emulator (listen on port %u)\r\n", port);

	memset(tcpmodem, 0, sizeof(TCPMODEM_t));
	tcpmodem->uart = uart;
	tcpmodem->escaped = 1;
	tcpmodem->echocmd = 1;
	tcpmodem->listenport = port;

	WSAStartup(MAKEWORD(2, 2), &tcpmodem->wsa);
	tcpmodem_listen(tcpmodem, tcpmodem->listenport);
	tcpmodem->ringtimer = timing_addTimer(tcpmodem_ringer, tcpmodem, 1, TIMING_DISABLED);

	return 0;
}
#endif
