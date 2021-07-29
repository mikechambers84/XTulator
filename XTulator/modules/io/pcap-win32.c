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
	pcap interface for Win32
*/

#include "../../config.h"

#ifdef USE_NE2000

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
pthread_t pcap_dispatchThreadID;
#endif
#include <pcap.h>
#include "../../debuglog.h"
#include "../../utility.h"
#include "ne2000.h"
#include "pcap-win32.h"

pcap_t* pcap_adhandle;

NE2000_t* pcap_ne2000 = NULL;

void pcap_listdevs() {
	pcap_if_t* alldevs;
	pcap_if_t* d;
	int i = 0;
	char errbuf[PCAP_ERRBUF_SIZE];

	/* Retrieve the device list from the local machine */
#ifdef _WIN32
	if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL /* auth is not needed */, &alldevs, errbuf) == -1)
#else
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
#endif
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
		printf("\nNo interfaces found! Make sure pcap is installed.\n");
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

#ifdef _WIN32
	if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1) {
#else
	if (pcap_findalldevs(&alldevs, errbuf) == -1) {
#endif
		debug_log(DEBUG_ERROR, "Error in pcap_findalldevs: %s\n", errbuf);
		return -1;
	}

	for (d = alldevs, i = 0; i < dev - 1; d = d->next, i++);

	debug_log(DEBUG_INFO, "[PCAP-WIN32] Initializing pcap library using device: \"%s\"\r\n", d->description ? d->description : "No description available");

#ifdef _WIN32
	if ((pcap_adhandle = pcap_open(d->name, 65536, PCAP_OPENFLAG_PROMISCUOUS, 1000, NULL, errbuf)) == NULL) {
#else
	if ((pcap_adhandle = pcap_open_live(d->name, 65535, 1, -1, NULL)) == NULL) {
#endif
		debug_log(DEBUG_ERROR, "\nUnable to open the adapter. %s is not supported by pcap\n", d->name);
		pcap_freealldevs(alldevs);
		return -1;
	}

	pcap_freealldevs(alldevs);

	pcap_ne2000 = ne2000;



#ifdef _WIN32
	_beginthread((void*)pcap_dispatchThread, 0, NULL);
#else
	pthread_create(&pcap_dispatchThreadID, NULL, pcap_dispatchThread, NULL);
#endif

	return 0;
}

void pcap_dispatchThread() {
	pcap_loop(pcap_adhandle, 0, pcap_rx_handler, NULL);
	/*while (running) {
		pcap_dispatch(pcap_adhandle, 1, pcap_rx_handler, NULL);
		if (pcap_havePacket == 0) {
			utility_sleep(1);
		}
	}*/
}

void pcap_rx_handler(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data) {
	(void)(param); //unused variable

	ne2000_rx_frame(pcap_ne2000, (void*)pkt_data, header->caplen);
}

void pcap_txPacket(u_char* data, int len) {
	pcap_sendpacket(pcap_adhandle, data, len);
}

#endif
