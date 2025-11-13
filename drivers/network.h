#ifndef NETWORK_H
#define NETWORK_H

#include "stdint.h"

// Network device driver interface
#define MAX_PACKET_SIZE 1518

typedef struct {
    uint8_t mac[6];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint8_t active;
} net_interface_t;

// Packet buffer
typedef struct {
    uint8_t data[MAX_PACKET_SIZE];
    uint16_t length;
} packet_t;

// Network driver functions
void network_init(void);
void network_poll(void);
int network_send_packet(const uint8_t* data, uint16_t length);
int network_receive_packet(uint8_t* buffer, uint16_t* length);
net_interface_t* network_get_interface(void);
void network_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway);

// RTL8139 driver (simple and common in QEMU)
void rtl8139_init(void);
void rtl8139_send(const uint8_t* data, uint16_t length);
int rtl8139_receive(uint8_t* buffer, uint16_t* length);

#endif /* NETWORK_H */
