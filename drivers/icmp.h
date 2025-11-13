#ifndef ICMP_H
#define ICMP_H

#include "stdint.h"

// ICMP header
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

void icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t sequence);
void icmp_receive(const uint8_t* data, uint16_t length, uint32_t src_ip);

#endif /* ICMP_H */
