#ifndef _PCAP_WIN32_H_
#define _PCAP_WIN32_H_

#include "../../config.h"

#ifdef USE_NE2000

#include <stdint.h>
#include <pcap.h>
#include "ne2000.h"

void pcap_rx_handler(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data);
void pcap_listdevs();
int pcap_init(NE2000_t* ne2000, int dev);
void pcap_dispatchThread();
void pcap_txPacket(u_char* data, int len);

#endif //USE_NE2000

#endif //_PCAP_WIN32_H_
