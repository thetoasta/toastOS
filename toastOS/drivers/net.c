#include "net.h"
#include "kio.h"
#include "funcs.h"
#include "stdio.h"
#include "time.h"
#include "string.h"

/*
 * toastOS Network Driver  -  RTL8139-based
 * ==========================================
 *
 * How networking works at a high level:
 *
 *   1.  We find the RTL8139 on the PCI bus (like finding a USB device).
 *   2.  We read its I/O base address so we know which ports to talk to.
 *   3.  We reset the chip, give it a receive buffer, and turn it on.
 *   4.  To SEND a packet we write bytes into a transmit buffer and poke
 *       the chip's "go" register.
 *   5.  To RECEIVE a packet we poll the receive buffer until data arrives.
 *
 *   Everything below is built from the ground up - no external libraries.
 */

/* ===== Helpers for 16-bit and 32-bit port I/O ===== */

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ===== Byte-order helpers (x86 is little-endian, network is big-endian) ===== */

static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline uint16_t ntohs(uint16_t v) { return htons(v); }

static inline uint32_t htonl(uint32_t v) {
    return ((v >> 24) & 0xFF)
         | ((v >>  8) & 0xFF00)
         | ((v <<  8) & 0xFF0000)
         | ((v << 24) & 0xFF000000);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

/* ===== Static state ===== */

static uint16_t nic_iobase = 0;
static int      nic_initialized = 0;

static uint8_t our_mac[6];

/*
 * ALL IP addresses stored in NETWORK byte order (big-endian) throughout.
 * ip_parse() returns network order.  Put on the wire as-is.
 */
static uint32_t our_ip;
static uint32_t gateway_ip;
static uint32_t dns_ip;
static uint8_t  gateway_mac[6];
static uint8_t  gateway_mac_known = 0;

static uint8_t rx_buffer[RX_BUF_SIZE + 1536 + 16] __attribute__((aligned(4)));
static uint16_t rx_read_ptr = 0;

static uint8_t tx_buffers[NUM_TX_DESC][TX_BUF_SIZE] __attribute__((aligned(4)));
static int     tx_cur = 0;

static uint16_t ip_id_counter = 1;

/* ===== PCI bus helpers ===== */

static uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg) {
    uint32_t addr = (uint32_t)(
        (1u << 31)
        | ((uint32_t)bus    << 16)
        | ((uint32_t)device << 11)
        | ((uint32_t)func   <<  8)
        | ((uint32_t)(reg & 0xFC))
    );
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static void pci_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg, uint32_t val) {
    uint32_t addr = (uint32_t)(
        (1u << 31)
        | ((uint32_t)bus    << 16)
        | ((uint32_t)device << 11)
        | ((uint32_t)func   <<  8)
        | ((uint32_t)(reg & 0xFC))
    );
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

static int pci_find_rtl8139(void) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read(0, dev, 0, 0x00);
        uint16_t vendor = (uint16_t)(id & 0xFFFF);
        uint16_t device = (uint16_t)(id >> 16);

        if (vendor == RTL8139_VENDOR_ID && device == RTL8139_DEVICE_ID) {
            uint32_t bar0 = pci_read(0, dev, 0, 0x10);
            nic_iobase = (uint16_t)(bar0 & 0xFFFC);

            /* Enable PCI bus mastering so the NIC can DMA */
            uint32_t cmd = pci_read(0, dev, 0, 0x04);
            cmd |= (1 << 2);
            pci_write(0, dev, 0, 0x04, cmd);

            return 0;
        }
    }
    return -1;
}

/* ===== NIC register shortcuts ===== */

static inline void nic_write8(uint16_t reg, uint8_t val)  { outb(nic_iobase + reg, val); }
static inline void nic_write16(uint16_t reg, uint16_t val) { outw(nic_iobase + reg, val); }
static inline void nic_write32(uint16_t reg, uint32_t val) { outl(nic_iobase + reg, val); }
static inline uint8_t  nic_read8(uint16_t reg)  { return inb(nic_iobase + reg); }
static inline uint16_t nic_read16(uint16_t reg) { return inw(nic_iobase + reg); }

/* ===== Checksum ===== */

static uint16_t ip_checksum(const void *data, int len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len == 1)     sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* ===== IP address helpers ===== */

/*
 * Parse "10.0.2.2" -> network-order uint32_t.
 */
static uint32_t ip_parse(const char *s) {
    uint32_t parts[4] = {0};
    int idx = 0;
    while (*s && idx < 4) {
        if (*s >= '0' && *s <= '9')
            parts[idx] = parts[idx] * 10 + (*s - '0');
        else if (*s == '.')
            idx++;
        s++;
    }
    uint32_t host = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return htonl(host);
}

static void ip_print(uint32_t ip_net) {
    uint32_t h = ntohl(ip_net);
    print_num((h >> 24) & 0xFF); kprint(".");
    print_num((h >> 16) & 0xFF); kprint(".");
    print_num((h >>  8) & 0xFF); kprint(".");
    print_num( h        & 0xFF);
}

static void net_delay_short(void) {
    for (volatile int i = 0; i < 5000; i++) inb(0x80);
}

static void net_delay_ms(void) {
    for (volatile int i = 0; i < 20000; i++) inb(0x80);
}

/* ===== Transmit ===== */

static void net_send(const void *data, uint16_t len) {
    if (len > TX_BUF_SIZE) return;

    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++)
        tx_buffers[tx_cur][i] = src[i];

    nic_write32(REG_TX_ADDR0 + tx_cur * 4, (uint32_t)(uintptr_t)tx_buffers[tx_cur]);
    nic_write32(REG_TX_STATUS0 + tx_cur * 4, (uint32_t)len);

    for (int t = 0; t < 2000; t++) {
        uint32_t st = inl(nic_iobase + REG_TX_STATUS0 + tx_cur * 4);
        if (st & ((1 << 15) | (1 << 14)))  /* TOK or TUN */
            break;
        net_delay_short();
    }

    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
}

/* ===== Receive ===== */

static int net_recv(uint8_t *out_buf, uint16_t max_len) {
    /* Check BUFE (buffer empty) in the command register */
    uint8_t cmd = nic_read8(REG_CMD);
    if (cmd & 0x01)
        return 0;

    uint16_t offset = rx_read_ptr % RX_BUF_SIZE;
    uint16_t status = *(uint16_t *)(rx_buffer + offset);
    uint16_t length = *(uint16_t *)(rx_buffer + offset + 2);

    if (length == 0 || length > 1600) {
        /* Garbage - skip */
        return 0;
    }

    if (!(status & 0x01)) {
        /* Bad packet - advance past it */
        rx_read_ptr = (uint16_t)((offset + length + 4 + 3) & ~3u);
        rx_read_ptr %= RX_BUF_SIZE;
        nic_write16(REG_RX_BUF_PTR, (uint16_t)(rx_read_ptr - 16));
        return 0;
    }

    uint16_t pkt_len = length - 4;  /* strip CRC */
    if (pkt_len > max_len) pkt_len = max_len;

    for (uint16_t i = 0; i < pkt_len; i++)
        out_buf[i] = rx_buffer[(offset + 4 + i) % (RX_BUF_SIZE + 1536)];

    rx_read_ptr = (uint16_t)((offset + length + 4 + 3) & ~3u);
    rx_read_ptr %= RX_BUF_SIZE;
    nic_write16(REG_RX_BUF_PTR, (uint16_t)(rx_read_ptr - 16));

    /* Ack all pending interrupts */
    nic_write16(REG_ISR, 0xFFFF);

    return (int)pkt_len;
}

/* ===== ARP ===== */

static void net_send_arp_request(uint32_t target_ip_net) {
    uint8_t frame[ETH_HEADER_SIZE + sizeof(arp_header_t)];

    eth_header_t *eth = (eth_header_t *)frame;
    for (int i = 0; i < 6; i++) eth->dest[i] = 0xFF;
    for (int i = 0; i < 6; i++) eth->src[i]  = our_mac[i];
    eth->type = htons(ETH_TYPE_ARP);

    arp_header_t *arp = (arp_header_t *)(frame + ETH_HEADER_SIZE);
    arp->hw_type    = htons(1);
    arp->proto_type = htons(ETH_TYPE_IP);
    arp->hw_len     = 6;
    arp->proto_len  = 4;
    arp->opcode     = htons(ARP_REQUEST);
    for (int i = 0; i < 6; i++) arp->sender_mac[i] = our_mac[i];
    arp->sender_ip  = our_ip;           /* already network order */
    for (int i = 0; i < 6; i++) arp->target_mac[i] = 0x00;
    arp->target_ip  = target_ip_net;    /* already network order */

    net_send(frame, sizeof(frame));
}

static int net_arp_resolve(uint32_t target_ip_net) {
    static uint8_t pkt[1536];

    for (int retry = 0; retry < 3; retry++) {
        net_send_arp_request(target_ip_net);

        for (int attempt = 0; attempt < 500; attempt++) {
            int len = net_recv(pkt, sizeof(pkt));
            if (len >= (int)(ETH_HEADER_SIZE + sizeof(arp_header_t))) {
                eth_header_t *eth = (eth_header_t *)pkt;
                if (ntohs(eth->type) == ETH_TYPE_ARP) {
                    arp_header_t *a = (arp_header_t *)(pkt + ETH_HEADER_SIZE);
                    if (ntohs(a->opcode) == ARP_REPLY &&
                        a->sender_ip == target_ip_net) {
                        for (int i = 0; i < 6; i++)
                            gateway_mac[i] = a->sender_mac[i];
                        gateway_mac_known = 1;
                        return 0;
                    }
                }
            }
            net_delay_ms();
        }
    }
    return -1;
}

/* ===== IP send ===== */

static void net_send_ip(uint32_t dest_ip_net, uint8_t protocol,
                         const void *payload, uint16_t payload_len) {
    if (!gateway_mac_known) {
        if (net_arp_resolve(gateway_ip) < 0) {
            kprint("[net] ARP failed - can't reach gateway");
            kprint_newline();
            return;
        }
    }

    uint16_t ip_total = (uint16_t)(20 + payload_len);
    uint16_t frame_len = (uint16_t)(ETH_HEADER_SIZE + ip_total);
    static uint8_t frame[1536];

    eth_header_t *eth = (eth_header_t *)frame;
    for (int i = 0; i < 6; i++) eth->dest[i] = gateway_mac[i];
    for (int i = 0; i < 6; i++) eth->src[i]  = our_mac[i];
    eth->type = htons(ETH_TYPE_IP);

    ip_header_t *ip = (ip_header_t *)(frame + ETH_HEADER_SIZE);
    ip->ver_ihl   = 0x45;
    ip->tos        = 0;
    ip->total_len  = htons(ip_total);
    ip->id         = htons(ip_id_counter++);
    ip->frag_off   = 0;
    ip->ttl        = 64;
    ip->protocol   = protocol;
    ip->checksum   = 0;
    ip->src_ip     = our_ip;       /* network order */
    ip->dst_ip     = dest_ip_net;  /* network order */
    ip->checksum   = ip_checksum(ip, 20);

    uint8_t *body = frame + ETH_HEADER_SIZE + 20;
    const uint8_t *src = (const uint8_t *)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        body[i] = src[i];

    net_send(frame, frame_len);
}

/* ===== UDP ===== */

static void net_send_udp(uint32_t dest_ip_net, uint16_t src_port,
                          uint16_t dst_port, const void *data, uint16_t data_len) {
    uint16_t udp_len = (uint16_t)(8 + data_len);
    static uint8_t udp_pkt[1536];

    udp_pkt[0] = (uint8_t)(src_port >> 8);
    udp_pkt[1] = (uint8_t)(src_port & 0xFF);
    udp_pkt[2] = (uint8_t)(dst_port >> 8);
    udp_pkt[3] = (uint8_t)(dst_port & 0xFF);
    udp_pkt[4] = (uint8_t)(udp_len >> 8);
    udp_pkt[5] = (uint8_t)(udp_len & 0xFF);
    udp_pkt[6] = 0;
    udp_pkt[7] = 0;

    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < data_len; i++)
        udp_pkt[8 + i] = src[i];

    net_send_ip(dest_ip_net, IP_PROTO_UDP, udp_pkt, udp_len);
}

/* ===== DNS resolver ===== */

static uint32_t dns_resolve(const char *hostname) {
    static uint8_t dns_pkt[512];
    int pos = 0;

    uint16_t txid = ip_id_counter++;

    /* DNS header (12 bytes) */
    dns_pkt[pos++] = (uint8_t)(txid >> 8);
    dns_pkt[pos++] = (uint8_t)(txid & 0xFF);
    dns_pkt[pos++] = 0x01;  /* RD flag */
    dns_pkt[pos++] = 0x00;
    dns_pkt[pos++] = 0x00;  /* QDCOUNT = 1 */
    dns_pkt[pos++] = 0x01;
    dns_pkt[pos++] = 0x00;  /* ANCOUNT */
    dns_pkt[pos++] = 0x00;
    dns_pkt[pos++] = 0x00;  /* NSCOUNT */
    dns_pkt[pos++] = 0x00;
    dns_pkt[pos++] = 0x00;  /* ARCOUNT */
    dns_pkt[pos++] = 0x00;

    /* Encode hostname: "google.com" -> [6]google[3]com[0] */
    const char *p = hostname;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int label_len = (int)(dot - p);
        if (label_len <= 0 || label_len > 63 || pos >= 490) return 0;
        dns_pkt[pos++] = (uint8_t)label_len;
        for (int i = 0; i < label_len; i++)
            dns_pkt[pos++] = (uint8_t)p[i];
        p = dot;
        if (*p == '.') p++;
    }
    dns_pkt[pos++] = 0x00; /* end of name */
    dns_pkt[pos++] = 0x00; /* QTYPE = A (1) */
    dns_pkt[pos++] = 0x01;
    dns_pkt[pos++] = 0x00; /* QCLASS = IN (1) */
    dns_pkt[pos++] = 0x01;

    static uint8_t pkt[1536];

    for (int retry = 0; retry < 3; retry++) {
        net_send_udp(dns_ip, 1053, 53, dns_pkt, (uint16_t)pos);

        for (int attempt = 0; attempt < 1500; attempt++) {
            int len = net_recv(pkt, sizeof(pkt));
            if (len > (int)(ETH_HEADER_SIZE + 20 + 8 + 12)) {
                ip_header_t *ip = (ip_header_t *)(pkt + ETH_HEADER_SIZE);
                if (ip->protocol == IP_PROTO_UDP) {
                    int udp_off = ETH_HEADER_SIZE + 20;
                    uint16_t sport = (uint16_t)((pkt[udp_off] << 8) | pkt[udp_off + 1]);
                    if (sport != 53) { net_delay_short(); continue; }

                    int dns_off = udp_off + 8;
                    int dns_len = len - dns_off;
                    if (dns_len < 12) { net_delay_short(); continue; }

                    uint8_t *dns = pkt + dns_off;
                    uint16_t rid = (uint16_t)((dns[0] << 8) | dns[1]);
                    if (rid != txid) { net_delay_short(); continue; }

                    uint16_t ancount = (uint16_t)((dns[6] << 8) | dns[7]);
                    if (ancount == 0) return 0;

                    /* Skip question section */
                    int off = 12;
                    while (off < dns_len && dns[off] != 0) {
                        if ((dns[off] & 0xC0) == 0xC0) { off += 2; goto qname_done; }
                        off += dns[off] + 1;
                    }
                    off++; /* skip 0 terminator */
                    qname_done:
                    off += 4; /* QTYPE + QCLASS */

                    /* Scan answers for type A */
                    for (int a = 0; a < ancount && off + 10 <= dns_len; a++) {
                        if ((dns[off] & 0xC0) == 0xC0) {
                            off += 2;
                        } else {
                            while (off < dns_len && dns[off] != 0) off += dns[off] + 1;
                            off++;
                        }
                        if (off + 10 > dns_len) break;
                        uint16_t rtype = (uint16_t)((dns[off] << 8) | dns[off+1]);
                        uint16_t rdlen = (uint16_t)((dns[off+8] << 8) | dns[off+9]);
                        off += 10;
                        if (rtype == 1 && rdlen == 4 && off + 4 <= dns_len) {
                            uint32_t resolved;
                            uint8_t *rp = (uint8_t *)&resolved;
                            rp[0] = dns[off];
                            rp[1] = dns[off+1];
                            rp[2] = dns[off+2];
                            rp[3] = dns[off+3];
                            return resolved; /* already network order */
                        }
                        off += rdlen;
                    }
                    return 0;
                }
            }
            net_delay_ms();
        }
    }
    return 0;
}

/* Check if string is a dotted IP address */
static int is_ip_address(const char *s) {
    int dots = 0, digits = 0;
    while (*s) {
        if (*s == '.') {
            if (digits == 0) return 0;
            dots++;
            digits = 0;
        } else if (*s >= '0' && *s <= '9') {
            digits++;
        } else {
            return 0;
        }
        s++;
    }
    return (dots == 3 && digits > 0);
}

/* Resolve hostname or IP string to network-order IP. Returns 0 on failure. */
static uint32_t resolve_host(const char *host) {
    if (is_ip_address(host))
        return ip_parse(host);

    kprint("Resolving ");
    kprint(host);
    kprint(" ... ");

    uint32_t ip = dns_resolve(host);
    if (ip == 0) {
        kprint("failed!");
        kprint_newline();
        return 0;
    }
    ip_print(ip);
    kprint_newline();
    return ip;
}

/* ===== ICMP ping ===== */

int net_ping(const char *host_str) {
    uint32_t target = resolve_host(host_str);
    if (target == 0) return -1;

    kprint("Pinging ");
    ip_print(target);
    kprint(" ...");
    kprint_newline();

    uint32_t start_tick = get_uptime_seconds();

    icmp_header_t icmp;
    icmp.type     = ICMP_ECHO_REQUEST;
    icmp.code     = 0;
    icmp.checksum = 0;
    icmp.id       = htons(0x1234);
    icmp.seq      = htons(1);
    icmp.checksum = ip_checksum(&icmp, sizeof(icmp));

    static uint8_t pkt[1536];

    for (int retry = 0; retry < 3; retry++) {
        net_send_ip(target, IP_PROTO_ICMP, &icmp, sizeof(icmp));

        for (int attempt = 0; attempt < 1500; attempt++) {
            int len = net_recv(pkt, sizeof(pkt));
            if (len > (int)(ETH_HEADER_SIZE + 20 + (int)sizeof(icmp_header_t))) {
                ip_header_t *ip = (ip_header_t *)(pkt + ETH_HEADER_SIZE);
                if (ip->protocol == IP_PROTO_ICMP) {
                    int ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
                    icmp_header_t *reply = (icmp_header_t *)(pkt + ETH_HEADER_SIZE + ip_hdr_len);
                    if (reply->type == ICMP_ECHO_REPLY) {
                        uint32_t elapsed = get_uptime_seconds() - start_tick;
                        kprint("Reply from ");
                        ip_print(target);
                        kprint(" - host is up! (");
                        if (elapsed == 0) kprint("<1s");
                        else { print_num(elapsed); kprint("s"); }
                        kprint(")");
                        kprint_newline();
                        return 0;
                    }
                }
            }
            net_delay_ms();
        }
    }

    kprint("Request timed out - no reply from ");
    ip_print(target);
    kprint_newline();
    return -1;
}

/* ===== Minimal TCP for HTTP ===== */

/* TCP pseudo-header for checksum calculation */
typedef struct __attribute__((packed)) {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} tcp_pseudo_t;

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const void *tcp_seg, uint16_t tcp_len) {
    tcp_pseudo_t pseudo;
    pseudo.src_ip   = src_ip;
    pseudo.dst_ip   = dst_ip;
    pseudo.zero     = 0;
    pseudo.protocol = IP_PROTO_TCP;
    pseudo.tcp_len  = htons(tcp_len);

    uint32_t sum = 0;
    const uint16_t *p;

    /* Sum pseudo-header */
    p = (const uint16_t *)&pseudo;
    for (int i = 0; i < (int)sizeof(pseudo) / 2; i++)
        sum += p[i];

    /* Sum TCP segment */
    p = (const uint16_t *)tcp_seg;
    int len = tcp_len;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len == 1) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static uint16_t local_port_counter = 49152;

static void net_send_tcp(uint32_t dest_ip_net, uint16_t src_port, uint16_t dst_port,
                          uint32_t seq, uint32_t ack, uint8_t flags,
                          const void *data, uint16_t data_len) {
    static uint8_t seg[1536];
    uint16_t tcp_hdr_len = 20;
    uint16_t tcp_total = tcp_hdr_len + data_len;

    tcp_header_t *tcp = (tcp_header_t *)seg;
    tcp->src_port = htons(src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq_num  = htonl(seq);
    tcp->ack_num  = htonl(ack);
    tcp->data_off = (uint8_t)((tcp_hdr_len / 4) << 4);
    tcp->flags    = flags;
    tcp->window   = htons(8192);
    tcp->checksum = 0;
    tcp->urgent   = 0;

    /* Copy payload after header */
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < data_len; i++)
        seg[tcp_hdr_len + i] = src[i];

    tcp->checksum = tcp_checksum(our_ip, dest_ip_net, seg, tcp_total);

    net_send_ip(dest_ip_net, IP_PROTO_TCP, seg, tcp_total);
}

/*
 * Wait for a TCP packet from dest_ip on the given ports.
 * Fills *out_seq, *out_ack, *out_flags with the received TCP header values.
 * Returns payload length, or -1 on timeout.
 * If payload_buf is non-NULL, copies payload data into it.
 */
static int tcp_recv(uint32_t dest_ip_net, uint16_t local_port, uint16_t remote_port,
                     uint32_t *out_seq, uint32_t *out_ack, uint8_t *out_flags,
                     uint8_t *payload_buf, uint16_t payload_max, int timeout_iters) {
    static uint8_t pkt[1536];

    for (int attempt = 0; attempt < timeout_iters; attempt++) {
        int len = net_recv(pkt, sizeof(pkt));
        if (len > (int)(ETH_HEADER_SIZE + 20 + 20)) {
            ip_header_t *ip = (ip_header_t *)(pkt + ETH_HEADER_SIZE);
            if (ip->protocol == IP_PROTO_TCP && ip->src_ip == dest_ip_net) {
                int ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
                tcp_header_t *tcp = (tcp_header_t *)(pkt + ETH_HEADER_SIZE + ip_hdr_len);

                if (ntohs(tcp->src_port) == remote_port &&
                    ntohs(tcp->dst_port) == local_port) {
                    *out_seq   = ntohl(tcp->seq_num);
                    *out_ack   = ntohl(tcp->ack_num);
                    *out_flags = tcp->flags;

                    int tcp_hdr = ((tcp->data_off >> 4) & 0x0F) * 4;
                    int ip_total = ntohs(ip->total_len);
                    int payload_len = ip_total - ip_hdr_len - tcp_hdr;
                    if (payload_len < 0) payload_len = 0;

                    if (payload_buf && payload_len > 0) {
                        int copy = payload_len;
                        if (copy > payload_max) copy = payload_max;
                        uint8_t *src = pkt + ETH_HEADER_SIZE + ip_hdr_len + tcp_hdr;
                        for (int i = 0; i < copy; i++)
                            payload_buf[i] = src[i];
                    }
                    return payload_len;
                }
            }
        }
        net_delay_short();
    }
    return -1;
}

/* ===== HTTP GET (over TCP) ===== */

int net_http_get(const char *host_str, const char *path) {
    uint32_t target = resolve_host(host_str);
    if (target == 0) return -1;

    kprint("Connecting to ");
    ip_print(target);
    kprint(":80");
    kprint(path);
    kprint(" ...");
    kprint_newline();

    uint16_t lport = local_port_counter++;
    if (local_port_counter > 60000) local_port_counter = 49152;

    uint32_t our_seq = 1000 + (ip_id_counter * 37);  /* simple pseudo-random ISN */
    uint32_t srv_seq = 0, srv_ack = 0;
    uint8_t  flags = 0;

    /* === TCP 3-way handshake === */

    /* 1. Send SYN */
    net_send_tcp(target, lport, 80, our_seq, 0, TCP_SYN, 0, 0);

    /* 2. Wait for SYN-ACK */
    int r = tcp_recv(target, lport, 80, &srv_seq, &srv_ack, &flags, 0, 0, 3000);
    if (r < 0 || !(flags & TCP_SYN) || !(flags & TCP_ACK)) {
        kprint("[net] TCP handshake failed (no SYN-ACK).");
        kprint_newline();
        return -1;
    }

    our_seq++;  /* SYN consumed one sequence number */
    uint32_t next_ack = srv_seq + 1;

    /* 3. Send ACK (completes handshake) */
    net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK, 0, 0);

    /* === Send HTTP request === */
    static char http_req[512];
    int pos = 0;
    const char *s;

    s = "GET ";
    while (*s) http_req[pos++] = *s++;
    while (*path) http_req[pos++] = *path++;
    s = " HTTP/1.0\r\nHost: ";
    while (*s) http_req[pos++] = *s++;
    s = host_str;
    while (*s && pos < 480) http_req[pos++] = *s++;
    s = "\r\nConnection: close\r\n\r\n";
    while (*s) http_req[pos++] = *s++;
    http_req[pos] = '\0';

    net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK | TCP_PSH,
                 http_req, (uint16_t)pos);
    our_seq += (uint32_t)pos;

    /* === Receive response === */
    kprint_newline();
    toast_shell_color("--- Response ---", LIGHT_CYAN);
    kprint_newline();

    static uint8_t data_buf[1460];
    int got_data = 0;
    int done = 0;
    int past_headers = 0;

    for (int wait = 0; wait < 8000 && !done; wait++) {
        r = tcp_recv(target, lport, 80, &srv_seq, &srv_ack, &flags,
                     data_buf, sizeof(data_buf), 50);
        if (r > 0) {
            /* ACK the received data */
            next_ack = srv_seq + (uint32_t)r;
            if (flags & TCP_FIN) next_ack++;  /* FIN consumes a seq number */

            /* Print received data, skipping HTTP headers if not past them yet */
            int start = 0;
            if (!past_headers) {
                /* Look for \r\n\r\n to skip headers */
                for (int i = 0; i < r - 3; i++) {
                    if (data_buf[i] == '\r' && data_buf[i+1] == '\n' &&
                        data_buf[i+2] == '\r' && data_buf[i+3] == '\n') {
                        start = i + 4;
                        past_headers = 1;
                        break;
                    }
                }
                if (!past_headers) {
                    net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK, 0, 0);
                    continue;
                }
            }

            for (int i = start; i < r; i++) {
                char c = (char)data_buf[i];
                if (c == '\n') kprint_newline();
                else if (c == '\r') { /* skip */ }
                else if (c >= 0x20 && c < 0x7F) {
                    char buf[2] = { c, '\0' };
                    kprint(buf);
                }
            }
            got_data = 1;

            if (flags & TCP_FIN) {
                net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK | TCP_FIN, 0, 0);
                done = 1;
            } else {
                net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK, 0, 0);
            }
        } else if (r == 0) {
            /* Packet with no payload (could be a FIN) */
            if (flags & TCP_FIN) {
                next_ack = srv_seq + 1;
                net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK | TCP_FIN, 0, 0);
                done = 1;
            }
        }
        /* r < 0 means timeout on this iteration, just loop again */
    }

    if (!got_data) {
        toast_shell_color("No response received.", YELLOW);
        kprint_newline();
        return -1;
    }

    kprint_newline();
    toast_shell_color("--- End ---", LIGHT_CYAN);
    kprint_newline();
    return 0;
}

/* ===== Simple terminal web browser ===== */

/*
 * Parse a URL into host and path components.
 * Handles "http://host/path", "host/path", and "host".
 * Writes into caller-supplied buffers.
 */
static void browse_parse_url(const char *url, char *host, int host_max,
                              char *path, int path_max) {
    const char *p = url;

    /* Skip "http://" or "https://" prefix */
    if (strncmp(p, "http://", 7) == 0)  p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;

    /* Copy host (up to '/' or end) */
    int i = 0;
    while (*p && *p != '/' && i < host_max - 1) {
        host[i++] = *p++;
    }
    host[i] = '\0';

    /* Copy path (rest of URL, or "/" if none) */
    if (*p == '/') {
        int j = 0;
        while (*p && j < path_max - 1) {
            path[j++] = *p++;
        }
        path[j] = '\0';
    } else {
        path[0] = '/';
        path[1] = '\0';
    }
}

/*
 * HTML entity decoder.  Tries to decode the entity at *p (just after '&').
 * Returns the decoded character, or 0 if unknown.
 * Advances *p past the entity (including trailing ';').
 */
static char decode_entity(const char **pp, int remaining) {
    const char *p = *pp;
    char decoded = 0;

    /* Numeric entities: &#123; or &#x1F; */
    if (*p == '#') {
        p++;
        int val = 0;
        if (*p == 'x' || *p == 'X') {
            p++;
            for (int i = 0; i < 6 && *p && *p != ';'; i++, p++) {
                val <<= 4;
                if (*p >= '0' && *p <= '9') val |= (*p - '0');
                else if (*p >= 'a' && *p <= 'f') val |= (10 + *p - 'a');
                else if (*p >= 'A' && *p <= 'F') val |= (10 + *p - 'A');
            }
        } else {
            for (int i = 0; i < 8 && *p && *p != ';'; i++, p++)
                val = val * 10 + (*p - '0');
        }
        if (*p == ';') p++;
        decoded = (val >= 0x20 && val < 0x7F) ? (char)val : '?';
        *pp = p;
        return decoded;
    }

    /* Named entities */
    struct { const char *name; int len; char ch; } ents[] = {
        {"lt;",    3, '<'},   {"gt;",    3, '>'},
        {"amp;",   4, '&'},   {"quot;",  5, '"'},
        {"apos;",  5, '\''},  {"nbsp;",  5, ' '},
        {"mdash;", 6, '-'},   {"ndash;", 6, '-'},
        {"copy;",  5, 'c'},   {"reg;",   4, 'r'},
        {"laquo;", 6, '<'},   {"raquo;", 6, '>'},
        {"bull;",  5, '*'},   {"hellip;",7, '.'},
    };
    int nents = (int)(sizeof(ents) / sizeof(ents[0]));

    for (int i = 0; i < nents; i++) {
        if (strncmp(p, ents[i].name, (size_t)ents[i].len) == 0) {
            *pp = p + ents[i].len;
            return ents[i].ch;
        }
    }

    /* Unknown entity - just return '&' and don't consume */
    return 0;
}

/*
 * Check if a tag name matches (case-insensitive, stops at space or >).
 */
static int tag_is(const char *tag, const char *name) {
    while (*name) {
        char a = *tag, b = *name;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
        tag++;
        name++;
    }
    /* Must be followed by space, >, or / */
    return (*tag == ' ' || *tag == '>' || *tag == '/' || *tag == '\0');
}

/*
 * Render a chunk of HTML as readable terminal text.
 * Maintains state across calls via static variables.
 */
static int  html_in_tag      = 0;
static int  html_in_script   = 0;
static int  html_in_style    = 0;
static int  html_in_title    = 0;
static int  html_col         = 0;
static int  html_last_was_nl = 1;
static int  html_line_count  = 0;
static char html_tag_buf[64];
static int  html_tag_pos     = 0;

static void browse_reset_state(void) {
    html_in_tag = 0;
    html_in_script = 0;
    html_in_style = 0;
    html_in_title = 0;
    html_col = 0;
    html_last_was_nl = 1;
    html_line_count = 0;
    html_tag_pos = 0;
}

static void browse_newline(void) {
    if (!html_last_was_nl) {
        kprint_newline();
        html_col = 0;
        html_last_was_nl = 1;
        html_line_count++;
    }
}

static void browse_putchar(char c) {
    if (c == '\0') return;

    /* Word wrap at column 78 */
    if (html_col >= 78) {
        kprint_newline();
        html_col = 0;
        html_line_count++;
    }

    char buf[2] = { c, '\0' };
    kprint(buf);
    html_col++;
    html_last_was_nl = 0;
}

static void browse_render_chunk(const uint8_t *data, int len) {
    for (int i = 0; i < len; i++) {
        char c = (char)data[i];

        /* Inside a tag: collect tag name and wait for '>' */
        if (html_in_tag) {
            if (c == '>') {
                html_tag_buf[html_tag_pos] = '\0';
                html_in_tag = 0;

                /* Process tag by name */
                const char *t = html_tag_buf;

                /* Closing tags */
                if (t[0] == '/') {
                    t++;
                    if (tag_is(t, "script"))      html_in_script = 0;
                    else if (tag_is(t, "style"))   html_in_style = 0;
                    else if (tag_is(t, "title")) {
                        html_in_title = 0;
                        browse_newline();
                    }
                    else if (tag_is(t, "p") || tag_is(t, "div") ||
                             tag_is(t, "section") || tag_is(t, "article") ||
                             tag_is(t, "li") || tag_is(t, "blockquote") ||
                             tag_is(t, "table") || tag_is(t, "tr")) {
                        browse_newline();
                    }
                    else if (tag_is(t, "h1") || tag_is(t, "h2") ||
                             tag_is(t, "h3") || tag_is(t, "h4") ||
                             tag_is(t, "h5") || tag_is(t, "h6")) {
                        browse_newline();
                    }
                }
                /* Opening tags */
                else {
                    if (tag_is(t, "script"))       html_in_script = 1;
                    else if (tag_is(t, "style"))    html_in_style = 1;
                    else if (tag_is(t, "title")) {
                        html_in_title = 1;
                        browse_newline();
                        toast_shell_color("TITLE: ", YELLOW);
                        html_col += 7;
                    }
                    else if (tag_is(t, "br") || tag_is(t, "br/") || tag_is(t, "br /")) {
                        browse_newline();
                    }
                    else if (tag_is(t, "hr")) {
                        browse_newline();
                        toast_shell_color("----------------------------------------", DARK_GREY);
                        browse_newline();
                    }
                    else if (tag_is(t, "p") || tag_is(t, "div") ||
                             tag_is(t, "section") || tag_is(t, "article") ||
                             tag_is(t, "blockquote") || tag_is(t, "table")) {
                        browse_newline();
                    }
                    else if (tag_is(t, "tr")) {
                        browse_newline();
                    }
                    else if (tag_is(t, "td") || tag_is(t, "th")) {
                        browse_putchar(' ');
                        browse_putchar('|');
                        browse_putchar(' ');
                    }
                    else if (tag_is(t, "li")) {
                        browse_newline();
                        browse_putchar(' ');
                        browse_putchar('*');
                        browse_putchar(' ');
                    }
                    else if (tag_is(t, "h1")) {
                        browse_newline();
                        toast_shell_color("# ", LIGHT_CYAN);
                        html_col += 2;
                    }
                    else if (tag_is(t, "h2")) {
                        browse_newline();
                        toast_shell_color("## ", LIGHT_CYAN);
                        html_col += 3;
                    }
                    else if (tag_is(t, "h3") || tag_is(t, "h4") ||
                             tag_is(t, "h5") || tag_is(t, "h6")) {
                        browse_newline();
                        toast_shell_color("### ", LIGHT_CYAN);
                        html_col += 4;
                    }
                    /* links: try to show href */
                    else if (tag_is(t, "a")) {
                        browse_putchar('[');
                    }
                }

                html_tag_pos = 0;
            } else {
                if (html_tag_pos < 63)
                    html_tag_buf[html_tag_pos++] = c;
            }
            continue;
        }

        /* Skip content inside <script> and <style> blocks */
        if (html_in_script || html_in_style) {
            /* But still look for the closing tag */
            if (c == '<') {
                html_in_tag = 1;
                html_tag_pos = 0;
            }
            continue;
        }

        /* Start of a new tag */
        if (c == '<') {
            html_in_tag = 1;
            html_tag_pos = 0;
            continue;
        }

        /* HTML entity */
        if (c == '&') {
            const char *p = (const char *)&data[i + 1];
            int remain = len - i - 1;
            if (remain > 0) {
                char decoded = decode_entity(&p, remain);
                if (decoded) {
                    i = (int)(p - (const char *)data) - 1; /* -1 because loop does i++ */
                    browse_putchar(decoded);
                } else {
                    browse_putchar('&');
                }
            } else {
                browse_putchar('&');
            }
            continue;
        }

        /* Collapse whitespace */
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!html_last_was_nl && html_col > 0) {
                browse_putchar(' ');
            }
            continue;
        }

        /* Regular printable character */
        if (c >= 0x20 && c < 0x7F) {
            browse_putchar(c);
        }
    }
}

/*
 * net_browse - the main terminal browser entry point.
 * Parses a URL, does TCP HTTP GET, and renders HTML as readable text.
 */
int net_browse(const char *url) {
    static char host[128];
    static char path[256];
    browse_parse_url(url, host, sizeof(host), path, sizeof(path));

    if (host[0] == '\0') {
        kprint("Usage: browse <url>");
        kprint_newline();
        return -1;
    }

    uint32_t target = resolve_host(host);
    if (target == 0) return -1;

    toast_shell_color("toastBrowse", LIGHT_GREEN);
    kprint(" - loading ");
    kprint(host);
    kprint(path);
    kprint_newline();

    uint16_t lport = local_port_counter++;
    if (local_port_counter > 60000) local_port_counter = 49152;

    uint32_t our_seq = 1000 + (ip_id_counter * 37);
    uint32_t srv_seq = 0, srv_ack = 0;
    uint8_t  flags = 0;

    /* === TCP 3-way handshake === */
    net_send_tcp(target, lport, 80, our_seq, 0, TCP_SYN, 0, 0);

    int r = tcp_recv(target, lport, 80, &srv_seq, &srv_ack, &flags, 0, 0, 3000);
    if (r < 0 || !(flags & TCP_SYN) || !(flags & TCP_ACK)) {
        toast_shell_color("[browse] connection failed.", LIGHT_RED);
        kprint_newline();
        return -1;
    }

    our_seq++;
    uint32_t next_ack = srv_seq + 1;
    net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK, 0, 0);

    /* === Send HTTP GET === */
    static char http_req[512];
    int pos = 0;
    const char *s;

    s = "GET ";
    while (*s) http_req[pos++] = *s++;
    s = path;
    while (*s) http_req[pos++] = *s++;
    s = " HTTP/1.0\r\nHost: ";
    while (*s) http_req[pos++] = *s++;
    s = host;
    while (*s && pos < 440) http_req[pos++] = *s++;
    s = "\r\nUser-Agent: toastBrowse/1.0\r\nAccept: text/html\r\nConnection: close\r\n\r\n";
    while (*s) http_req[pos++] = *s++;
    http_req[pos] = '\0';

    net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK | TCP_PSH,
                 http_req, (uint16_t)pos);
    our_seq += (uint32_t)pos;

    /* === Receive and render === */
    browse_reset_state();

    static uint8_t data_buf[1460];
    int got_data = 0;
    int done = 0;
    int past_headers = 0;

    toast_shell_color("----------------------------------------", DARK_GREY);
    kprint_newline();

    for (int wait = 0; wait < 8000 && !done; wait++) {
        r = tcp_recv(target, lport, 80, &srv_seq, &srv_ack, &flags,
                     data_buf, sizeof(data_buf), 50);
        if (r > 0) {
            next_ack = srv_seq + (uint32_t)r;
            if (flags & TCP_FIN) next_ack++;

            int start = 0;
            if (!past_headers) {
                for (int j = 0; j < r - 3; j++) {
                    if (data_buf[j] == '\r' && data_buf[j+1] == '\n' &&
                        data_buf[j+2] == '\r' && data_buf[j+3] == '\n') {
                        start = j + 4;
                        past_headers = 1;
                        break;
                    }
                }
                if (!past_headers) {
                    net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK, 0, 0);
                    continue;
                }
            }

            /* Render this chunk of HTML */
            if (start < r) {
                browse_render_chunk(data_buf + start, r - start);
            }
            got_data = 1;

            if (flags & TCP_FIN) {
                net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK | TCP_FIN, 0, 0);
                done = 1;
            } else {
                net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK, 0, 0);
            }
        } else if (r == 0) {
            if (flags & TCP_FIN) {
                next_ack = srv_seq + 1;
                net_send_tcp(target, lport, 80, our_seq, next_ack, TCP_ACK | TCP_FIN, 0, 0);
                done = 1;
            }
        }
    }

    browse_newline();
    toast_shell_color("----------------------------------------", DARK_GREY);
    kprint_newline();

    if (!got_data) {
        toast_shell_color("[browse] no response from server.", YELLOW);
        kprint_newline();
        return -1;
    }

    toast_shell_color("toastBrowse", LIGHT_GREEN);
    kprint(" - done.");
    kprint_newline();
    return 0;
}

/* ===== Print local IP ===== */

void net_print_local_ip(void) {
    if (!nic_initialized) {
        kprint("Network not initialised.");
        kprint_newline();
        return;
    }
    kprint("IP address : ");
    ip_print(our_ip);
    kprint_newline();

    kprint("Gateway    : ");
    ip_print(gateway_ip);
    kprint_newline();

    kprint("DNS        : ");
    ip_print(dns_ip);
    kprint_newline();

    kprint("MAC address: ");
    for (int i = 0; i < 6; i++) {
        static const char hex[] = "0123456789ABCDEF";
        char s[3] = { hex[our_mac[i] >> 4], hex[our_mac[i] & 0xF], '\0' };
        kprint(s);
        if (i < 5) kprint(":");
    }
    kprint_newline();
}

/* ===== Initialise the NIC ===== */

int net_init(void) {
    /* Only init once - don't re-reset the NIC on every command */
    if (nic_initialized) return 0;

    if (pci_find_rtl8139() < 0) {
        kprint("[net] RTL8139 NIC not found on PCI bus.");
        kprint_newline();
        kprint("[net] Start QEMU with: -netdev user,id=n0 -device rtl8139,netdev=n0");
        kprint_newline();
        return -1;
    }

    /* Power on */
    nic_write8(REG_CONFIG1, 0x00);

    /* Software reset with timeout */
    nic_write8(REG_CMD, CMD_RESET);
    for (int t = 0; t < 100000; t++) {
        if (!(nic_read8(REG_CMD) & CMD_RESET)) break;
        net_delay_short();
    }
    if (nic_read8(REG_CMD) & CMD_RESET) {
        kprint("[net] RTL8139 reset timed out.");
        kprint_newline();
        return -1;
    }

    /* Read MAC */
    for (int i = 0; i < 6; i++)
        our_mac[i] = nic_read8(REG_MAC0 + i);

    /* Set up receive buffer (zeroed) */
    for (int i = 0; i < (int)sizeof(rx_buffer); i++) rx_buffer[i] = 0;
    nic_write32(REG_RX_BUF, (uint32_t)(uintptr_t)rx_buffer);
    rx_read_ptr = 0;

    /* RX config */
    nic_write32(REG_RX_CONFIG, RX_CFG_AAP | RX_CFG_APM | RX_CFG_AM | RX_CFG_AB | RX_CFG_WRAP);

    /* Enable TX + RX */
    nic_write8(REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    /* Enable all interrupts for polling */
    nic_write16(REG_IMR, 0xFFFF);
    nic_write16(REG_ISR, 0xFFFF);

    /* IP addresses (QEMU user-mode defaults) */
    our_ip     = ip_parse("10.0.2.15");
    gateway_ip = ip_parse("10.0.2.2");
    dns_ip     = ip_parse("10.0.2.3");

    nic_initialized = 1;

    kprint("[net] RTL8139 ready  I/O=0x");
    static const char hex[] = "0123456789ABCDEF";
    char h[5];
    h[0] = hex[(nic_iobase >> 12) & 0xF];
    h[1] = hex[(nic_iobase >> 8)  & 0xF];
    h[2] = hex[(nic_iobase >> 4)  & 0xF];
    h[3] = hex[(nic_iobase)       & 0xF];
    h[4] = '\0';
    kprint(h);
    kprint("  IP=");
    ip_print(our_ip);
    kprint("  GW=");
    ip_print(gateway_ip);
    kprint("  DNS=");
    ip_print(dns_ip);
    kprint_newline();

    return 0;
}

/* Query NIC status */
void net_get_info(net_info_t *info) {
    if (nic_initialized) {
        /* Connection type */
        info->connection_type[0] = 'E'; info->connection_type[1] = 't';
        info->connection_type[2] = 'h'; info->connection_type[3] = 'e';
        info->connection_type[4] = 'r'; info->connection_type[5] = 'n';
        info->connection_type[6] = 'e'; info->connection_type[7] = 't';
        info->connection_type[8] = '\0';

        /* NIC name */
        const char *name = "RTL8139";
        int i = 0;
        while (name[i] && i < 31) { info->nic_name[i] = name[i]; i++; }
        info->nic_name[i] = '\0';

        info->link_up = 1;
    } else {
        const char *none = "None";
        int i = 0;
        while (none[i] && i < 15) { info->connection_type[i] = none[i]; i++; }
        info->connection_type[i] = '\0';

        info->nic_name[0] = '\0';
        info->link_up = 0;
    }
}
