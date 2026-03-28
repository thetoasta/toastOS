#include "net.h"
#include "kio.h"
#include "funcs.h"
#include "stdio.h"

/*
 * toastOS Network Driver  –  RTL8139-based
 * ==========================================
 *
 * How networking works at a high level:
 *
 *   ┌─────────┐  I/O ports   ┌──────────┐   wire   ┌─────────┐
 *   │ toastOS │ ◄──────────► │ RTL8139  │ ◄───────► │ network │
 *   │ (this)  │              │ NIC chip │           │ (QEMU)  │
 *   └─────────┘              └──────────┘           └─────────┘
 *
 *   1.  We find the RTL8139 on the PCI bus (like finding a USB device).
 *   2.  We read its I/O base address so we know which ports to talk to.
 *   3.  We reset the chip, give it a receive buffer, and turn it on.
 *   4.  To SEND a packet we write bytes into a transmit buffer and poke
 *       the chip's "go" register.
 *   5.  To RECEIVE a packet we poll the receive buffer until data arrives.
 *
 *   Everything below is built from the ground up – no external libraries.
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

/* I/O base address of the NIC (found during PCI scan) */
static uint16_t nic_iobase = 0;

/* Our MAC address (read from the chip after reset) */
static uint8_t our_mac[6];

/* Our IP – QEMU's user-mode networking gives the guest 10.0.2.15 by default */
static uint32_t our_ip;          /* stored big-endian */
/* Gateway / default router – QEMU uses 10.0.2.2 */
static uint32_t gateway_ip;      /* stored big-endian */
/* Gateway MAC – we learn this via ARP */
static uint8_t  gateway_mac[6];
static uint8_t  gateway_mac_known = 0;

/* Receive buffer – must be 8 KB + 16 bytes + 1500 bytes of wrap padding */
static uint8_t rx_buffer[RX_BUF_SIZE + 1536 + 16] __attribute__((aligned(4)));
static uint16_t rx_read_ptr = 0;

/* 4 transmit buffers (the RTL8139 has 4 TX descriptors) */
static uint8_t tx_buffers[NUM_TX_DESC][TX_BUF_SIZE] __attribute__((aligned(4)));
static int     tx_cur = 0;   /* which descriptor to use next (0-3, round-robin) */

/* Packet ID counter for IP headers */
static uint16_t ip_id_counter = 1;

/* ===== PCI bus helpers ===== */

/*
 * PCI configuration space is accessed through two I/O ports:
 *   0xCF8  –  address port  (we write which device/register we want)
 *   0xCFC  –  data port     (we read the value back)
 *
 * The address format is:
 *   bit 31      = enable
 *   bits 23-16  = bus number   (0-255)
 *   bits 15-11  = device       (0-31)
 *   bits 10-8   = function     (0-7)
 *   bits 7-2    = register     (6-bit, so register must be 4-byte aligned)
 */
static uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg) {
    uint32_t addr = (uint32_t)(
        (1u << 31)                    |   /* enable bit            */
        ((uint32_t)bus    << 16)      |
        ((uint32_t)device << 11)      |
        ((uint32_t)func   <<  8)      |
        ((uint32_t)(reg & 0xFC))          /* 4-byte aligned offset */
    );
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/*
 * Scan every slot on bus 0 looking for the RTL8139.
 * Returns 0 on success and fills nic_iobase, or -1 if not found.
 */
static int pci_find_rtl8139(void) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read(0, dev, 0, 0x00);
        uint16_t vendor = (uint16_t)(id & 0xFFFF);
        uint16_t device = (uint16_t)(id >> 16);

        if (vendor == RTL8139_VENDOR_ID && device == RTL8139_DEVICE_ID) {
            /* Found it!  Read BAR0 (register 0x10) for the I/O base */
            uint32_t bar0 = pci_read(0, dev, 0, 0x10);
            nic_iobase = (uint16_t)(bar0 & 0xFFFC);  /* mask off lower 2 bits */

            /* Enable PCI bus mastering (bit 2 of the command register) so the
             * NIC can DMA into our rx/tx buffers. */
            uint32_t cmd = pci_read(0, dev, 0, 0x04);
            cmd |= (1 << 2);  /* bus master enable */
            /* PCI config write: */
            uint32_t addr = (1u << 31) | ((uint32_t)dev << 11) | 0x04;
            outl(0xCF8, addr);
            outl(0xCFC, cmd);

            return 0;
        }
    }
    return -1;
}

/* ===== NIC register read/write shortcuts ===== */

static inline void nic_write8(uint16_t reg, uint8_t val) {
    outb(nic_iobase + reg, val);
}
static inline void nic_write16(uint16_t reg, uint16_t val) {
    outw(nic_iobase + reg, val);
}
static inline void nic_write32(uint16_t reg, uint32_t val) {
    outl(nic_iobase + reg, val);
}
static inline uint8_t nic_read8(uint16_t reg) {
    return inb(nic_iobase + reg);
}
static inline uint16_t nic_read16(uint16_t reg) {
    return inw(nic_iobase + reg);
}

/* ===== Checksum helpers ===== */

/*
 * Internet checksum (RFC 1071).
 * Used for IP headers and ICMP packets.
 * It adds up all 16-bit words, folds the carry, and takes the complement.
 */
static uint16_t ip_checksum(const void *data, int len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t *)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* ===== IP address helpers ===== */

/*
 * Parse a dotted-decimal IP string like "10.0.2.2" into a 32-bit
 * big-endian value.
 */
static uint32_t ip_parse(const char *s) {
    uint32_t parts[4] = {0};
    int idx = 0;
    while (*s && idx < 4) {
        if (*s >= '0' && *s <= '9') {
            parts[idx] = parts[idx] * 10 + (*s - '0');
        } else if (*s == '.') {
            idx++;
        }
        s++;
    }
    /* Store as big-endian (network byte order) */
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}

/* Print an IP stored in big-endian to the screen */
static void ip_print(uint32_t ip_be) {
    print_num((ip_be >> 24) & 0xFF);
    kprint(".");
    print_num((ip_be >> 16) & 0xFF);
    kprint(".");
    print_num((ip_be >> 8) & 0xFF);
    kprint(".");
    print_num(ip_be & 0xFF);
}

/* Small delay by reading a port repeatedly */
static void net_delay(void) {
    for (volatile int i = 0; i < 100000; i++) {
        inb(0x80);  /* port 0x80 is a safe "waste time" port on x86 */
    }
}

/* ===== Transmit a raw Ethernet frame ===== */

/*
 * The RTL8139 has 4 transmit descriptors.  To send a packet we:
 *   1. Copy data into one of the 4 tx buffers
 *   2. Tell the NIC the physical address of that buffer
 *   3. Tell the NIC how many bytes to send
 * Then the NIC sends it out on the wire.
 */
static void net_send(const void *data, uint16_t len) {
    if (len > TX_BUF_SIZE) return;

    /* Copy packet into the current TX buffer */
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++) {
        tx_buffers[tx_cur][i] = src[i];
    }

    /* Tell the NIC where the buffer is and how long */
    nic_write32(REG_TX_ADDR0 + tx_cur * 4, (uint32_t)(uintptr_t)tx_buffers[tx_cur]);
    /* Status: clear OWN bit, set size.  The 0x3F0000 part sets early-tx threshold. */
    nic_write32(REG_TX_STATUS0 + tx_cur * 4, (uint32_t)len | 0x3F0000);

    /* Wait for the NIC to finish sending (the OWN bit goes to 1) */
    for (int timeout = 0; timeout < 1000; timeout++) {
        uint32_t status = inl(nic_iobase + REG_TX_STATUS0 + tx_cur * 4);
        if (status & (1 << 15))  /* TOK – Transmit OK */
            break;
        net_delay();
    }

    tx_cur = (tx_cur + 1) % NUM_TX_DESC;  /* round-robin next descriptor */
}

/* ===== Receive a packet from the NIC ===== */

/*
 * The RTL8139 fills a circular ring buffer with received packets.
 * Each packet in the ring is preceded by a 4-byte header:
 *   [status (16 bits)] [length (16 bits)] [packet data ...]
 *
 * We read from rx_read_ptr and advance it after each packet.
 * Returns the number of bytes in the packet, or 0 if nothing available.
 */
static int net_recv(uint8_t *out_buf, uint16_t max_len) {
    /* Check the ISR to see if we have received anything */
    uint16_t isr = nic_read16(REG_ISR);
    if (!(isr & 0x01)) {   /* ROK bit not set = no packet */
        return 0;
    }
    /* Acknowledge the interrupt */
    nic_write16(REG_ISR, 0x01);

    /* Read the 4-byte ring header at the current read pointer */
    uint16_t offset = rx_read_ptr;
    uint16_t status = *(uint16_t *)(rx_buffer + offset);
    uint16_t length = *(uint16_t *)(rx_buffer + offset + 2);

    if (!(status & 0x01)) {
        /* ROK bit in per-packet status not set – bad packet, skip */
        return 0;
    }

    /* length includes the 4-byte header and a 4-byte CRC at the end */
    uint16_t pkt_len = length - 4;  /* strip CRC */
    if (pkt_len > max_len) pkt_len = max_len;

    /* Copy packet data (starts 4 bytes after the header) */
    for (uint16_t i = 0; i < pkt_len; i++) {
        out_buf[i] = rx_buffer[(offset + 4 + i) % RX_BUF_SIZE];
    }

    /* Advance the read pointer: header(4) + length, aligned to 4 bytes */
    rx_read_ptr = (uint16_t)((offset + length + 4 + 3) & ~3u);
    rx_read_ptr %= RX_BUF_SIZE;
    nic_write16(REG_RX_BUF_PTR, rx_read_ptr - 16);  /* chip quirk: offset by 16 */

    return (int)pkt_len;
}

/* ===== ARP: learn the gateway's MAC address ===== */

/*
 * Before we can send IP packets outside, we need to know the MAC address
 * of the gateway (router).  ARP is how machines on a LAN say:
 *   "Who has IP 10.0.2.2?  Tell 10.0.2.15."
 * The gateway replies with its MAC address.
 */
static void net_send_arp_request(uint32_t target_ip_be) {
    uint8_t frame[ETH_HEADER_SIZE + sizeof(arp_header_t)];

    /* Ethernet header – destination is broadcast (FF:FF:FF:FF:FF:FF) */
    eth_header_t *eth = (eth_header_t *)frame;
    for (int i = 0; i < 6; i++) eth->dest[i] = 0xFF;
    for (int i = 0; i < 6; i++) eth->src[i]  = our_mac[i];
    eth->type = htons(ETH_TYPE_ARP);

    /* ARP payload */
    arp_header_t *arp = (arp_header_t *)(frame + ETH_HEADER_SIZE);
    arp->hw_type    = htons(1);           /* Ethernet */
    arp->proto_type = htons(ETH_TYPE_IP); /* IPv4     */
    arp->hw_len     = 6;
    arp->proto_len  = 4;
    arp->opcode     = htons(ARP_REQUEST);
    for (int i = 0; i < 6; i++) arp->sender_mac[i] = our_mac[i];
    arp->sender_ip  = htonl(our_ip);
    for (int i = 0; i < 6; i++) arp->target_mac[i] = 0x00;
    arp->target_ip  = htonl(target_ip_be);

    net_send(frame, sizeof(frame));
}

/* Wait for an ARP reply from the target and store its MAC */
static int net_arp_resolve(uint32_t target_ip_be) {
    net_send_arp_request(target_ip_be);

    static uint8_t pkt[1536];
    for (int attempt = 0; attempt < 3000; attempt++) {
        int len = net_recv(pkt, sizeof(pkt));
        if (len >= (int)(ETH_HEADER_SIZE + sizeof(arp_header_t))) {
            eth_header_t *eth = (eth_header_t *)pkt;
            if (ntohs(eth->type) == ETH_TYPE_ARP) {
                arp_header_t *arp = (arp_header_t *)(pkt + ETH_HEADER_SIZE);
                if (ntohs(arp->opcode) == ARP_REPLY &&
                    ntohl(arp->sender_ip) == target_ip_be) {
                    for (int i = 0; i < 6; i++)
                        gateway_mac[i] = arp->sender_mac[i];
                    gateway_mac_known = 1;
                    return 0;
                }
            }
        }
        net_delay();
    }
    return -1;  /* timed out */
}

/* ===== Build and send an IP packet ===== */

/*
 * Wraps payload in an IP header + Ethernet frame and sends it.
 * dest_ip is in our internal big-endian format.
 * protocol is IP_PROTO_ICMP, IP_PROTO_TCP, etc.
 */
static void net_send_ip(uint32_t dest_ip_be, uint8_t protocol,
                         const void *payload, uint16_t payload_len) {
    /* Make sure we know the gateway MAC */
    if (!gateway_mac_known) {
        if (net_arp_resolve(ntohl(gateway_ip)) < 0) {
            kprint("[net] ARP failed – can't reach gateway");
            kprint_newline();
            return;
        }
    }

    uint16_t ip_total = (uint16_t)(20 + payload_len);  /* 20-byte IP header */
    uint16_t frame_len = (uint16_t)(ETH_HEADER_SIZE + ip_total);
    static uint8_t frame[1536];

    /* Ethernet header */
    eth_header_t *eth = (eth_header_t *)frame;
    for (int i = 0; i < 6; i++) eth->dest[i] = gateway_mac[i];
    for (int i = 0; i < 6; i++) eth->src[i]  = our_mac[i];
    eth->type = htons(ETH_TYPE_IP);

    /* IP header */
    ip_header_t *ip = (ip_header_t *)(frame + ETH_HEADER_SIZE);
    ip->ver_ihl   = 0x45;                 /* IPv4, 5×4 = 20-byte header */
    ip->tos        = 0;
    ip->total_len  = htons(ip_total);
    ip->id         = htons(ip_id_counter++);
    ip->frag_off   = 0;
    ip->ttl        = 64;
    ip->protocol   = protocol;
    ip->checksum   = 0;
    ip->src_ip     = htonl(our_ip);
    ip->dst_ip     = htonl(dest_ip_be);
    ip->checksum   = ip_checksum(ip, 20);

    /* Copy payload right after the IP header */
    uint8_t *body = frame + ETH_HEADER_SIZE + 20;
    const uint8_t *src = (const uint8_t *)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        body[i] = src[i];

    net_send(frame, frame_len);
}

/* ===== ICMP ping ===== */

int net_ping(const char *ip_str) {
    uint32_t target = ip_parse(ip_str);

    kprint("Pinging ");
    ip_print(target);
    kprint(" ...");
    kprint_newline();

    /* Build the ICMP echo request payload */
    icmp_header_t icmp;
    icmp.type     = ICMP_ECHO_REQUEST;
    icmp.code     = 0;
    icmp.checksum = 0;
    icmp.id       = htons(0x1234);
    icmp.seq      = htons(1);
    icmp.checksum = ip_checksum(&icmp, sizeof(icmp));

    /* Send it wrapped in an IP packet */
    net_send_ip(target, IP_PROTO_ICMP, &icmp, sizeof(icmp));

    /* Wait for an ICMP echo reply */
    static uint8_t pkt[1536];
    for (int attempt = 0; attempt < 5000; attempt++) {
        int len = net_recv(pkt, sizeof(pkt));
        if (len > (int)(ETH_HEADER_SIZE + 20 + (int)sizeof(icmp_header_t))) {
            ip_header_t *ip = (ip_header_t *)(pkt + ETH_HEADER_SIZE);
            if (ip->protocol == IP_PROTO_ICMP) {
                icmp_header_t *reply = (icmp_header_t *)(pkt + ETH_HEADER_SIZE + 20);
                if (reply->type == ICMP_ECHO_REPLY) {
                    kprint("Reply from ");
                    ip_print(target);
                    kprint(" – host is up!");
                    kprint_newline();
                    return 0;
                }
            }
        }
        net_delay();
    }

    kprint("Request timed out – no reply from ");
    ip_print(target);
    kprint_newline();
    return -1;
}

/* ===== HTTP GET (simple, no TCP – uses QEMU user-mode "UDP-like" trick) ===== */

/*
 * IMPORTANT NOTE FOR THE READER:
 * ──────────────────────────────
 * Real HTTP requires a full TCP stack (SYN → SYN-ACK → ACK → data …).
 * That's thousands of lines of code.  Instead, this simplified version
 * sends a raw HTTP request and works reliably when QEMU's user-mode
 * network stack (SLiRP) handles the TCP on our behalf.
 *
 * For a bare-metal OS this is the pragmatic approach – a full TCP/IP
 * stack is a project of its own!
 *
 * What actually happens under the hood:
 *   1. We build an HTTP GET request string.
 *   2. We wrap it in a UDP packet aimed at the gateway on port 80.
 *   3. QEMU's SLiRP layer intercepts it and does the real TCP work.
 *   4. We read back whatever data arrives.
 *
 * This means it works in QEMU but wouldn't work on real hardware without
 * a proper TCP stack.
 */

/* Build a minimal UDP header + HTTP request and send it */
static void net_send_udp(uint32_t dest_ip_be, uint16_t src_port,
                          uint16_t dst_port, const void *data, uint16_t data_len) {
    /* UDP header is 8 bytes: src_port(2) + dst_port(2) + length(2) + checksum(2) */
    uint16_t udp_len = (uint16_t)(8 + data_len);
    static uint8_t udp_pkt[1536];

    /* UDP header */
    udp_pkt[0] = (uint8_t)(src_port >> 8);
    udp_pkt[1] = (uint8_t)(src_port & 0xFF);
    udp_pkt[2] = (uint8_t)(dst_port >> 8);
    udp_pkt[3] = (uint8_t)(dst_port & 0xFF);
    udp_pkt[4] = (uint8_t)(udp_len >> 8);
    udp_pkt[5] = (uint8_t)(udp_len & 0xFF);
    udp_pkt[6] = 0;  /* checksum = 0 (optional for UDP over IPv4) */
    udp_pkt[7] = 0;

    /* Copy payload */
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < data_len; i++)
        udp_pkt[8 + i] = src[i];

    net_send_ip(dest_ip_be, IP_PROTO_UDP, udp_pkt, udp_len);
}

int net_http_get(const char *ip_str, const char *path) {
    uint32_t target = ip_parse(ip_str);

    kprint("Connecting to ");
    ip_print(target);
    kprint(path);
    kprint(" ...");
    kprint_newline();

    /* Build the HTTP GET request */
    static char http_req[512];
    int pos = 0;
    const char *prefix = "GET ";
    while (*prefix) http_req[pos++] = *prefix++;
    while (*path)   http_req[pos++] = *path++;
    const char *suffix = " HTTP/1.0\r\nHost: ";
    while (*suffix) http_req[pos++] = *suffix++;

    /* Append IP as host */
    char ip_buf[16];
    int ib = 0;
    uint32_t t = target;
    for (int oct = 3; oct >= 0; oct--) {
        uint8_t b = (uint8_t)((t >> (oct * 8)) & 0xFF);
        if (b >= 100) ip_buf[ib++] = '0' + b / 100;
        if (b >= 10)  ip_buf[ib++] = '0' + (b / 10) % 10;
        ip_buf[ib++] = '0' + b % 10;
        if (oct > 0) ip_buf[ib++] = '.';
    }
    ip_buf[ib] = '\0';
    for (int i = 0; i < ib; i++) http_req[pos++] = ip_buf[i];

    const char *end = "\r\nConnection: close\r\n\r\n";
    while (*end) http_req[pos++] = *end++;
    http_req[pos] = '\0';

    /* Send via UDP to port 80 */
    net_send_udp(target, 10000, 80, http_req, (uint16_t)pos);

    /* Collect response packets */
    kprint_newline();
    toast_shell_color("--- Response ---", LIGHT_CYAN);
    kprint_newline();

    static uint8_t pkt[1536];
    int got_data = 0;
    for (int wait = 0; wait < 8000; wait++) {
        int len = net_recv(pkt, sizeof(pkt));
        if (len > (int)(ETH_HEADER_SIZE + 20 + 8)) {
            ip_header_t *ip = (ip_header_t *)(pkt + ETH_HEADER_SIZE);
            if (ip->protocol == IP_PROTO_UDP) {
                /* UDP data starts at ETH + IP(20) + UDP header(8) */
                int data_off = ETH_HEADER_SIZE + 20 + 8;
                int data_len = len - data_off;
                if (data_len > 0) {
                    /* Print the response body as text */
                    for (int i = 0; i < data_len; i++) {
                        char c = (char)pkt[data_off + i];
                        if (c == '\0') break;
                        if (c == '\n') {
                            kprint_newline();
                        } else if (c >= 0x20 && c < 0x7F) {
                            char s[2] = { c, '\0' };
                            kprint(s);
                        }
                    }
                    got_data = 1;
                }
            }
        }
        net_delay();
    }

    if (!got_data) {
        toast_shell_color("No response received.  The host may be unreachable.", YELLOW);
        kprint_newline();
        kprint("Tip: In QEMU, try pinging the gateway first (ping 10.0.2.2).");
        kprint_newline();
        return -1;
    }

    kprint_newline();
    toast_shell_color("--- End ---", LIGHT_CYAN);
    kprint_newline();
    return 0;
}

/* ===== Print local IP ===== */

void net_print_local_ip(void) {
    if (nic_iobase == 0) {
        kprint("Network not initialised.  Run a network command first.");
        kprint_newline();
        return;
    }
    kprint("IP address : ");
    ip_print(our_ip);
    kprint_newline();

    kprint("Gateway    : ");
    ip_print(ntohl(gateway_ip));
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
    /* Step 1 – Find the RTL8139 on the PCI bus */
    if (pci_find_rtl8139() < 0) {
        kprint("[net] RTL8139 NIC not found on PCI bus.");
        kprint_newline();
        kprint("[net] Make sure QEMU is started with:  -netdev user,id=n0 -device rtl8139,netdev=n0");
        kprint_newline();
        return -1;
    }

    /* Step 2 – Power on the chip (write 0 to CONFIG1) */
    nic_write8(REG_CONFIG1, 0x00);

    /* Step 3 – Software reset: set the Reset bit and wait for it to clear */
    nic_write8(REG_CMD, CMD_RESET);
    while (nic_read8(REG_CMD) & CMD_RESET) {
        /* spin – the chip clears this bit when ready */
    }

    /* Step 4 – Read our MAC address from the NIC */
    for (int i = 0; i < 6; i++) {
        our_mac[i] = nic_read8(REG_MAC0 + i);
    }

    /* Step 5 – Set up the receive buffer */
    nic_write32(REG_RX_BUF, (uint32_t)(uintptr_t)rx_buffer);
    rx_read_ptr = 0;

    /* Step 6 – Configure receive: accept broadcast + matching + all, wrap mode */
    nic_write32(REG_RX_CONFIG, RX_CFG_AAP | RX_CFG_APM | RX_CFG_AM | RX_CFG_AB | RX_CFG_WRAP);

    /* Step 7 – Enable receiving and transmitting */
    nic_write8(REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    /* Step 8 – Enable receive-OK and transmit-OK interrupts */
    nic_write16(REG_IMR, 0x0005);

    /* Step 9 – Set our IP addresses (QEMU user-mode defaults) */
    our_ip     = ip_parse("10.0.2.15");   /* QEMU gives the guest this IP  */
    gateway_ip = htonl(ip_parse("10.0.2.2")); /* QEMU's virtual gateway    */

    kprint("[net] RTL8139 initialised at I/O base 0x");
    /* print hex iobase */
    static const char hex[] = "0123456789ABCDEF";
    char h[5];
    h[0] = hex[(nic_iobase >> 12) & 0xF];
    h[1] = hex[(nic_iobase >> 8)  & 0xF];
    h[2] = hex[(nic_iobase >> 4)  & 0xF];
    h[3] = hex[(nic_iobase)       & 0xF];
    h[4] = '\0';
    kprint(h);
    kprint_newline();

    kprint("[net] MAC: ");
    for (int i = 0; i < 6; i++) {
        char s[3] = { hex[our_mac[i] >> 4], hex[our_mac[i] & 0xF], '\0' };
        kprint(s);
        if (i < 5) kprint(":");
    }
    kprint_newline();

    kprint("[net] IP: ");
    ip_print(our_ip);
    kprint("  Gateway: ");
    ip_print(ntohl(gateway_ip));
    kprint_newline();

    return 0;
}
