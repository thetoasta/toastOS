#include "arp.h"
#include "ethernet.h"
#include "network.h"
#include "kio.h"

#define ARP_CACHE_SIZE 16

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

void arp_init(void) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = 0;
    }
}

void arp_send_request(uint32_t target_ip) {
    arp_packet_t arp;
    net_interface_t* iface = network_get_interface();
    
    arp.hw_type = htons(1);      // Ethernet
    arp.proto_type = htons(0x0800); // IPv4
    arp.hw_size = 6;
    arp.proto_size = 4;
    arp.opcode = htons(ARP_REQUEST);
    
    for (int i = 0; i < 6; i++) {
        arp.sender_mac[i] = iface->mac[i];
        arp.target_mac[i] = 0;
    }
    arp.sender_ip = htonl(iface->ip);
    arp.target_ip = htonl(target_ip);
    
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ethernet_send(broadcast, ETHERTYPE_ARP, (uint8_t*)&arp, sizeof(arp));
}

void arp_receive(const uint8_t* data, uint16_t length) {
    if (length < sizeof(arp_packet_t)) return;
    
    arp_packet_t* arp = (arp_packet_t*)data;
    net_interface_t* iface = network_get_interface();
    
    uint16_t opcode = htons(arp->opcode);
    uint32_t sender_ip = htonl(arp->sender_ip);
    uint32_t target_ip = htonl(arp->target_ip);
    
    // Add to cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == sender_ip) {
            arp_cache[i].ip = sender_ip;
            for (int j = 0; j < 6; j++) {
                arp_cache[i].mac[j] = arp->sender_mac[j];
            }
            arp_cache[i].valid = 1;
            break;
        }
    }
    
    // If it's a request for us, send reply
    if (opcode == ARP_REQUEST && target_ip == iface->ip) {
        arp_packet_t reply;
        reply.hw_type = htons(1);
        reply.proto_type = htons(0x0800);
        reply.hw_size = 6;
        reply.proto_size = 4;
        reply.opcode = htons(ARP_REPLY);
        
        for (int i = 0; i < 6; i++) {
            reply.sender_mac[i] = iface->mac[i];
            reply.target_mac[i] = arp->sender_mac[i];
        }
        reply.sender_ip = htonl(iface->ip);
        reply.target_ip = arp->sender_ip;
        
        ethernet_send(arp->sender_mac, ETHERTYPE_ARP, (uint8_t*)&reply, sizeof(reply));
    }
}

int arp_resolve(uint32_t ip, uint8_t* mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            for (int j = 0; j < 6; j++) {
                mac[j] = arp_cache[i].mac[j];
            }
            return 1;
        }
    }
    return 0;
}
