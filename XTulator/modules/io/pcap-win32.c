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
	NPCAP interface for Win32
*/

#ifdef _WIN32

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <process.h>
#include <pcap.h>
#include "../../config.h"
#include "../../debuglog.h"
#include "../../utility.h"
#include "ne2000.h"
#include "pcap-win32.h"

volatile uint8_t pcap_havePacket = 0;
pcap_t* pcap_adhandle;

volatile u_char pcap_packetData[2048];
volatile uint32_t pcap_len = 0;

NE2000_t* pcap_ne2000 = NULL;

void pcap_listdevs() {
	pcap_if_t* alldevs;
	pcap_if_t* d;
	int i = 0;
	char errbuf[PCAP_ERRBUF_SIZE];

	/* Retrieve the device list from the local machine */
	if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL /* auth is not needed */, &alldevs, errbuf) == -1)
	{
		fprintf(stderr, "Error in pcap_findalldevs_ex: %s\n", errbuf);
		exit(1);
	}

	/* Print the list */
	for (d = alldevs; d != NULL; d = d->next)
	{
		printf("%d. %s", ++i, d->name);
		if (d->description)
			printf(" (%s)\n", d->description);
		else
			printf(" (No description available)\n");
	}

	if (i == 0)
	{
		printf("\nNo interfaces found! Make sure Npcap is installed.\n");
		return;
	}

	/* We don't need any more the device list. Free it */
	pcap_freealldevs(alldevs);
}

int pcap_init(NE2000_t* ne2000, int dev) {
	pcap_if_t* alldevs;
	pcap_if_t* d;
	int i = 0;
	char errbuf[PCAP_ERRBUF_SIZE];

	if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1) {
		debug_log(DEBUG_ERROR, "Error in pcap_findalldevs: %s\n", errbuf);
		return -1;
	}

	for (d = alldevs, i = 0; i < dev - 1; d = d->next, i++);

	debug_log(DEBUG_INFO, "[PCAP-WIN32] Initializing Npcap library using device: \"%s\"\r\n", d->description);

	if ((pcap_adhandle = pcap_open(d->name, 65536, PCAP_OPENFLAG_PROMISCUOUS, 1000, NULL, errbuf)) == NULL) {
		debug_log(DEBUG_ERROR, "\nUnable to open the adapter. %s is not supported by Npcap\n", d->name);
		pcap_freealldevs(alldevs);
		return -1;
	}

	pcap_freealldevs(alldevs);

	pcap_ne2000 = ne2000;

	_beginthread(pcap_dispatchThread, 0, NULL);

	return 0;
}

void pcap_dispatchThread() {
	pcap_loop(pcap_adhandle, -1, pcap_rx_handler, NULL);
	while (running) {
		utility_sleep(10);
	}
	pcap_breakloop(pcap_adhandle);
}

void pcap_rx_handler(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data) {
	int i, accept;
	(VOID)(param); //unused variable

	while (pcap_havePacket) {}

	accept = 1;
	for (i = 0; i < 6; i++) {
		if (pkt_data[i] != 0xFF) {
			accept = 0;
			break;
		}
	}

	if (accept == 0) {
		accept = 1;
		for (i = 0; i < 6; i++) {
			if (pkt_data[i] != pcap_ne2000->macaddr[i]) {
				accept = 0;
				break;
			}
		}
	}

	pcap_len = header->caplen;
	if (pcap_len > 2048) return;
	memcpy(pcap_packetData, pkt_data, header->caplen);
	pcap_havePacket = 1;
	//debug_log(DEBUG_INFO, "len:%d\n", header->len);
}

void pcap_rxPacket() {
	if (pcap_havePacket) {
		ne2000_rx_frame(pcap_ne2000, pcap_packetData, pcap_len);
		pcap_havePacket = 0;
	}
}

void pcap_txPacket(u_char* data, int len) {
	pcap_sendpacket(pcap_adhandle, data, len);
}

#endif
