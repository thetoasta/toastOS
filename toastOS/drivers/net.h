#ifndef NET_H
#define NET_H

#include "stdint.h"

/*
 * toastOS Network Driver
 * ======================
 * Simple networking using the RTL8139 NIC (the default in QEMU).
 *
 * Supports:
 *   - ping <ip>        : send an ICMP echo request and wait for a reply
 *   - ret-contents <ip>: fetch the contents of a webpage (HTTP GET on port 80)
 *   - localip           : show the IP address assigned to toastOS
 *
 * How it works (in plain english):
 *   1. We talk to the RTL8139 chip through I/O ports (like ata.c does for disks).
 *   2. We give ourselves a static IP address (10.0.2.15 — QEMU's default).
 *   3. For "ping" we build a tiny ICMP packet, send it, and listen for a reply.
 *   4. For "ret-contents" we open a TCP-like connection on port 80 and read back.
 *   5. For "localip" we just print the IP we configured.
 */

/* ===== RTL8139 PCI / I/O constants ===== */
#define RTL8139_VENDOR_ID  0x10EC
#define RTL8139_DEVICE_ID  0x8139

/* RTL8139 register offsets (from the I/O base) */
#define REG_MAC0           0x00   /* MAC address bytes 0-3               */
#define REG_MAC4           0x04   /* MAC address bytes 4-5               */
#define REG_TX_STATUS0     0x10   /* Transmit status  (4 descriptors)    */
#define REG_TX_ADDR0       0x20   /* Transmit start address descriptor 0 */
#define REG_RX_BUF        0x30   /* Receive buffer start address         */
#define REG_CMD           0x37   /* Command register                     */
#define REG_RX_BUF_PTR    0x38   /* Current read pointer in rx ring      */
#define REG_IMR           0x3C   /* Interrupt mask register              */
#define REG_ISR           0x3E   /* Interrupt status register            */
#define REG_TX_CONFIG     0x40   /* Transmit configuration               */
#define REG_RX_CONFIG     0x44   /* Receive configuration                */
#define REG_CONFIG1       0x52   /* Configuration register 1             */

/* Command bits */
#define CMD_RX_ENABLE      0x08
#define CMD_TX_ENABLE      0x04
#define CMD_RESET          0x10

/* RX config: accept broadcast + multicast + physical match + all */
#define RX_CFG_AAP         (1 << 0)   /* Accept All Packets              */
#define RX_CFG_APM         (1 << 1)   /* Accept Physical Match           */
#define RX_CFG_AM          (1 << 2)   /* Accept Multicast                */
#define RX_CFG_AB          (1 << 3)   /* Accept Broadcast                */
#define RX_CFG_WRAP        (1 << 7)   /* Wrap around rx buffer           */

/* Buffer sizes */
#define RX_BUF_SIZE        8192
#define TX_BUF_SIZE        1536
#define NUM_TX_DESC        4

/* ===== Ethernet / IP / ICMP / ARP constants ===== */
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

/* ===== Packet structures (packed so the compiler doesn't add padding) ===== */

typedef struct __attribute__((packed)) {
    uint8_t  dest[6];
    uint8_t  src[6];
    uint16_t type;       /* big-endian */
} eth_header_t;

typedef struct __attribute__((packed)) {
    uint8_t  ver_ihl;    /* version (4 bits) + header length (4 bits) */
    uint8_t  tos;
    uint16_t total_len;  /* big-endian */
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;   /* big-endian */
    uint32_t src_ip;     /* big-endian */
    uint32_t dst_ip;     /* big-endian */
} ip_header_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;

typedef struct __attribute__((packed)) {
    uint16_t hw_type;     /* 1 = Ethernet */
    uint16_t proto_type;  /* 0x0800 = IPv4 */
    uint8_t  hw_len;      /* 6 for MAC */
    uint8_t  proto_len;   /* 4 for IPv4 */
    uint16_t opcode;      /* 1 = request, 2 = reply */
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} arp_header_t;

/* ===== Public API ===== */

/* Initialise the RTL8139 NIC.  Returns 0 on success, -1 if not found. */
int  net_init(void);

/* Send an ICMP ping to the given IP string (e.g. "10.0.2.2").
 * Prints round-trip result to screen.  Returns 0 on reply, -1 on timeout. */
int  net_ping(const char *ip_str);

/* Fetch the body of an HTTP page from ip_str on port 80 and print it.
 * path is the URL path, e.g. "/" or "/index.html".
 * Returns 0 on success, -1 on failure. */
int  net_http_get(const char *ip_str, const char *path);

/* Print the local IP address to screen. */
void net_print_local_ip(void);

#endif /* NET_H */
