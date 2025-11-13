#include "icmp.h"
#include "ip.h"
#include "network.h"
#include "ethernet.h"
#include "kio.h"
#include "funcs.h"

extern void serial_write_string(const char* str);

void icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t sequence) {
    uint8_t packet[64];
    icmp_header_t* icmp = (icmp_header_t*)packet;
    
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(sequence);
    
    // Add some data
    for (int i = 0; i < 48; i++) {
        packet[8 + i] = 0x42 + i;
    }
    
    icmp->checksum = htons(ip_checksum(packet, 56));
    
    ip_send(dest_ip, IP_PROTOCOL_ICMP, packet, 56);
}

void icmp_receive(const uint8_t* data, uint16_t length, uint32_t src_ip) {
    if (length < 8) return;
    
    icmp_header_t* icmp = (icmp_header_t*)data;
    
    if (icmp->type == ICMP_ECHO_REQUEST) {
        // Send echo reply
        uint8_t reply[MAX_PACKET_SIZE];
        for (uint16_t i = 0; i < length; i++) {
            reply[i] = data[i];
        }
        
        icmp_header_t* reply_hdr = (icmp_header_t*)reply;
        reply_hdr->type = ICMP_ECHO_REPLY;
        reply_hdr->code = 0;
        reply_hdr->checksum = 0;
        reply_hdr->checksum = htons(ip_checksum(reply, length));
        
        ip_send(src_ip, IP_PROTOCOL_ICMP, reply, length);
        
        serial_write_string("[NET] Replied to ICMP echo request\n");
    } else if (icmp->type == ICMP_ECHO_REPLY) {
        serial_write_string("[NET] Received ICMP echo reply from ");
        // Would print IP here
        serial_write_string("\n");
        
        toast_shell_color("Reply from ", LIGHT_GREEN);
        // Print IP address
        uint8_t* ip_bytes = (uint8_t*)&src_ip;
        kprint_int(ip_bytes[3]);
        kprint(".");
        kprint_int(ip_bytes[2]);
        kprint(".");
        kprint_int(ip_bytes[1]);
        kprint(".");
        kprint_int(ip_bytes[0]);
        kprint(" seq=");
        kprint_int(htons(icmp->sequence));
        kprint_newline();
    }
}
