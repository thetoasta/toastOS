#ifndef ARP_H
#define ARP_H

#include "stdint.h"

// ARP packet structure
typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

#define ARP_REQUEST 1
#define ARP_REPLY   2

void arp_init(void);
void arp_send_request(uint32_t target_ip);
void arp_receive(const uint8_t* data, uint16_t length);
int arp_resolve(uint32_t ip, uint8_t* mac);

#endif /* ARP_H */
