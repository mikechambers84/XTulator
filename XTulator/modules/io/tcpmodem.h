#ifndef _TCPMODEM_H_
#define _TCPMODEM_H_

#include "../../config.h"

#ifdef ENABLE_TCP_MODEM
#include <stdint.h>
#include "../../chipset/uart.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>

typedef struct {
	uint8_t escaped;
	uint8_t livesocket;
	uint8_t listening;
	uint8_t ringing;
	uint8_t ringstate;
	uint32_t ringtimer;
	uint16_t listenport;
	uint8_t echocmd;
	char rxbuf[1024]; //used only in offline mode
	char txbuf[1024]; //used only in offline mode
	uint16_t rxpos;
	uint16_t txpos;
	char lasttx[3];
	WSADATA wsa;
	SOCKET socket;
	SOCKET serversocket;
	SOCKADDR_IN server;
	UART_t* uart;
} TCPMODEM_t;

int tcpmodem_connect(TCPMODEM_t* tcpmodem, char* host, uint16_t port);
void tcpmodem_offline(TCPMODEM_t* tcpmodem);
void tcpmodem_rxpoll(TCPMODEM_t* tcpmodem);
void tcpmodem_tx(TCPMODEM_t* tcpmodem, uint8_t value);
int tcpmodem_init(TCPMODEM_t* tcpmodem, UART_t* uart, uint16_t port);

#else

#endif

#endif

#endif
