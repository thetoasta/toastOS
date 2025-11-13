#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "network.h"
#include "funcs.h"

extern void icmp_receive(const uint8_t* data, uint16_t length, uint32_t src_ip);
extern void udp_receive(const uint8_t* data, uint16_t length, uint32_t src_ip);
extern void tcp_receive(const uint8_t* data, uint16_t length, uint32_t src_ip);

uint16_t ip_checksum(const uint8_t* data, uint16_t length) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i < length; i += 2) {
        uint16_t word = (data[i] << 8) | (i + 1 < length ? data[i + 1] : 0);
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}

void ip_send(uint32_t dest_ip, uint8_t protocol, const uint8_t* payload, uint16_t length) {
    net_interface_t* iface = network_get_interface();
    uint8_t packet[MAX_PACKET_SIZE];
    
    ip_header_t* ip = (ip_header_t*)packet;
    ip->version_ihl = 0x45;  // IPv4, 20 byte header
    ip->tos = 0;
    ip->total_length = htons(20 + length);
    ip->id = htons(1234);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = htonl(iface->ip);
    ip->dest_ip = htonl(dest_ip);
    
    ip->checksum = htons(ip_checksum((uint8_t*)ip, 20));
    
    // Copy payload
    for (uint16_t i = 0; i < length; i++) {
        packet[20 + i] = payload[i];
    }
    
    // Resolve MAC address with retry
    uint8_t dest_mac[6];
    int attempts = 0;
    while (attempts < 3) {
        if (arp_resolve(dest_ip, dest_mac)) {
            ethernet_send(dest_mac, ETHERTYPE_IP, packet, 20 + length);
            return;
        }
        
        // Send ARP request and wait
        arp_send_request(dest_ip);
        
        // Poll network while waiting for ARP reply
        for (volatile int i = 0; i < 1000000; i++) {
            if (i % 10000 == 0) {
                network_poll();
            }
        }
        
        attempts++;
    }
    
    serial_write_string("[IP] Failed to resolve MAC address for destination\n");
}

void ip_receive(const uint8_t* data, uint16_t length) {
    if (length < 20) return;
    
    ip_header_t* ip = (ip_header_t*)data;
    uint16_t total_len = htons(ip->total_length);
    uint32_t src_ip = htonl(ip->src_ip);
    uint32_t dest_ip = htonl(ip->dest_ip);
    
    net_interface_t* iface = network_get_interface();
    if (dest_ip != iface->ip) return;  // Not for us
    
    const uint8_t* payload = data + 20;
    uint16_t payload_len = total_len - 20;
    
    switch (ip->protocol) {
        case IP_PROTOCOL_ICMP:
            icmp_receive(payload, payload_len, src_ip);
            break;
        case IP_PROTOCOL_UDP:
            udp_receive(payload, payload_len, src_ip);
            break;
        case IP_PROTOCOL_TCP:
            tcp_receive(payload, payload_len, src_ip);
            break;
    }
}
