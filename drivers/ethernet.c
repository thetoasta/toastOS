#include "ethernet.h"
#include "network.h"
#include "string.h"

extern void arp_receive(const uint8_t* data, uint16_t length);
extern void ip_receive(const uint8_t* data, uint16_t length);

// Byte order conversion
uint16_t htons(uint16_t n) {
    return ((n & 0xFF) << 8) | ((n >> 8) & 0xFF);
}

uint32_t htonl(uint32_t n) {
    return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n >> 8) & 0xFF00) | ((n >> 24) & 0xFF);
}

void ethernet_send(const uint8_t* dest_mac, uint16_t ethertype, const uint8_t* payload, uint16_t length) {
    uint8_t frame[MAX_PACKET_SIZE];
    net_interface_t* iface = network_get_interface();
    
    // Build Ethernet header
    for (int i = 0; i < 6; i++) {
        frame[i] = dest_mac[i];          // Destination MAC
        frame[i + 6] = iface->mac[i];     // Source MAC
    }
    frame[12] = (ethertype >> 8) & 0xFF;
    frame[13] = ethertype & 0xFF;
    
    // Copy payload
    for (uint16_t i = 0; i < length && i < (MAX_PACKET_SIZE - 14); i++) {
        frame[14 + i] = payload[i];
    }
    
    network_send_packet(frame, 14 + length);
}

void ethernet_receive(const uint8_t* frame, uint16_t length) {
    if (length < 14) return;
    
    uint16_t ethertype = (frame[12] << 8) | frame[13];
    const uint8_t* payload = frame + 14;
    uint16_t payload_length = length - 14;
    
    switch (ethertype) {
        case ETHERTYPE_ARP:
            arp_receive(payload, payload_length);
            break;
        case ETHERTYPE_IP:
            ip_receive(payload, payload_length);
            break;
    }
}
