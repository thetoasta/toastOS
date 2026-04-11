/*
 * toastOS++ Network Driver
 * Namespace: toast::net
 */

#ifndef NET_HPP
#define NET_HPP

#include "stdint.hpp"

/* ===== RTL8139 PCI / I/O constants ===== */
#define RTL8139_VENDOR_ID  0x10EC
#define RTL8139_DEVICE_ID  0x8139

#define REG_MAC0           0x00
#define REG_MAC4           0x04
#define REG_TX_STATUS0     0x10
#define REG_TX_ADDR0       0x20
#define REG_RX_BUF         0x30
#define REG_CMD            0x37
#define REG_RX_BUF_PTR     0x38
#define REG_IMR            0x3C
#define REG_ISR            0x3E
#define REG_TX_CONFIG      0x40
#define REG_RX_CONFIG      0x44
#define REG_CONFIG1        0x52

#define CMD_RX_ENABLE      0x08
#define CMD_TX_ENABLE      0x04
#define CMD_RESET          0x10

#define RX_CFG_AAP         (1 << 0)
#define RX_CFG_APM         (1 << 1)
#define RX_CFG_AM          (1 << 2)
#define RX_CFG_AB          (1 << 3)
#define RX_CFG_WRAP        (1 << 7)

#define RX_BUF_SIZE        8192
#define TX_BUF_SIZE        1536
#define NUM_TX_DESC        4

#define ETH_HEADER_SIZE    14
#define ETH_TYPE_IP        0x0800
#define ETH_TYPE_ARP       0x0806

#define IP_PROTO_ICMP      1
#define IP_PROTO_TCP       6
#define IP_PROTO_UDP       17

#define ICMP_ECHO_REQUEST  8
#define ICMP_ECHO_REPLY    0

#define ARP_REQUEST        1
#define ARP_REPLY          2

#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

/* Packet structures */
struct __attribute__((packed)) eth_header_t {
    uint8_t  dest[6];
    uint8_t  src[6];
    uint16_t type;
};

struct __attribute__((packed)) ip_header_t {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
};

struct __attribute__((packed)) icmp_header_t {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

struct __attribute__((packed)) arp_header_t {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
};

struct __attribute__((packed)) tcp_header_t {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_off;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
};

namespace toast {
namespace net {

struct Info {
    char connection_type[16];
    char nic_name[32];
    int  link_up;
};

/* Initialize RTL8139 NIC */
int init();

/* Send ICMP ping to IP or hostname */
int ping(const char* host);

/* HTTP GET request */
int http_get(const char* host, const char* path);

/* Simple terminal web browser */
int browse(const char* url);

/* Print local IP */
void print_local_ip();

/* Query NIC status */
void get_info(Info* info);

} // namespace net
} // namespace toast

/* Legacy C-style type alias */
typedef toast::net::Info net_info_t;

/* Legacy C-style function aliases */
int net_init();
int net_ping(const char* host);
int net_http_get(const char* host, const char* path);
int net_browse(const char* url);
void net_print_local_ip();
void net_get_info(net_info_t* info);

#endif /* NET_HPP */
