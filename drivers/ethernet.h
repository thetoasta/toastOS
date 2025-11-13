#ifndef ETHERNET_H
#define ETHERNET_H

#include "stdint.h"

// Ethernet frame structure
typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    uint8_t payload[1500];
} __attribute__((packed)) ethernet_frame_t;

// Ethertypes
#define ETHERTYPE_IP   0x0800
#define ETHERTYPE_ARP  0x0806

void ethernet_send(const uint8_t* dest_mac, uint16_t ethertype, const uint8_t* payload, uint16_t length);
void ethernet_receive(const uint8_t* frame, uint16_t length);
uint16_t htons(uint16_t n);
uint32_t htonl(uint32_t n);

#endif /* ETHERNET_H */
