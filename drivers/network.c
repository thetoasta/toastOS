#include "network.h"
#include "ethernet.h"
#include "kio.h"
#include "funcs.h"

// RTL8139 register offsets
#define RTL8139_VENDOR_ID    0x10EC
#define RTL8139_DEVICE_ID    0x8139

#define RTL8139_IDR0         0x00  // MAC address
#define RTL8139_MAR0         0x08  // Multicast
#define RTL8139_TXSTATUS0    0x10  // TX status (4 descriptors)
#define RTL8139_TXADDR0      0x20  // TX buffer address
#define RTL8139_RXBUF        0x30  // RX buffer address
#define RTL8139_CMD          0x37  // Command register
#define RTL8139_CAPR         0x38  // Current address of packet read
#define RTL8139_IMR          0x3C  // Interrupt mask
#define RTL8139_ISR          0x3E  // Interrupt status
#define RTL8139_TXCONFIG     0x40  // TX config
#define RTL8139_RXCONFIG     0x44  // RX config
#define RTL8139_CONFIG1      0x52  // Config register 1

// Commands
#define RTL8139_CMD_RESET    0x10
#define RTL8139_CMD_RX_EN    0x08
#define RTL8139_CMD_TX_EN    0x04

static uint16_t rtl8139_iobase = 0;
static net_interface_t net_interface;
static uint8_t rx_buffer[8192 + 16 + 1500] __attribute__((aligned(4)));
static uint8_t tx_buffer[4][1536] __attribute__((aligned(4)));
static uint8_t current_tx = 0;

extern void serial_write_string(const char* str);

// Simple PCI scan for RTL8139
static uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    return (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

static uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

void rtl8139_init(void) {
    serial_write_string("[NET] Scanning for RTL8139...\n");
    
    // Simple PCI scan
    for (uint8_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_config_read_word(bus, slot, 0, 0);
            if (vendor == 0xFFFF) continue;
            
            uint16_t device = pci_config_read_word(bus, slot, 0, 2);
            if (vendor == RTL8139_VENDOR_ID && device == RTL8139_DEVICE_ID) {
                serial_write_string("[NET] Found RTL8139 at bus ");
                serial_write_string(", slot ");
                serial_write_string("\n");
                
                // Enable bus mastering
                uint16_t command = pci_config_read_word(bus, slot, 0, 0x04);
                command |= 0x05;  // I/O space + Bus master
                pci_config_write_dword(bus, slot, 0, 0x04, command);
                
                // Get I/O base address
                uint32_t bar0 = pci_config_read_dword(bus, slot, 0, 0x10);
                rtl8139_iobase = (uint16_t)(bar0 & 0xFFFFFFFC);
                
                serial_write_string("[NET] I/O base: 0x");
                serial_write_string("\n");
                
                // Power on
                outb(rtl8139_iobase + RTL8139_CONFIG1, 0x00);
                
                // Software reset
                outb(rtl8139_iobase + RTL8139_CMD, RTL8139_CMD_RESET);
                while ((inb(rtl8139_iobase + RTL8139_CMD) & RTL8139_CMD_RESET) != 0);
                
                // Read MAC address
                for (int i = 0; i < 6; i++) {
                    net_interface.mac[i] = inb(rtl8139_iobase + RTL8139_IDR0 + i);
                }
                
                serial_write_string("[NET] MAC: ");
                for (int i = 0; i < 6; i++) {
                    // Would print MAC here
                }
                serial_write_string("\n");
                
                // Set RX buffer
                outl(rtl8139_iobase + RTL8139_RXBUF, (uint32_t)rx_buffer);
                
                // Set interrupt mask
                outw(rtl8139_iobase + RTL8139_IMR, 0x0005);  // RX OK + RX error
                
                // Configure RX: accept broadcast + physical match
                outl(rtl8139_iobase + RTL8139_RXCONFIG, 0x0000000F);
                
                // Configure TX
                outl(rtl8139_iobase + RTL8139_TXCONFIG, 0x03000700);
                
                // Enable RX and TX
                outb(rtl8139_iobase + RTL8139_CMD, RTL8139_CMD_RX_EN | RTL8139_CMD_TX_EN);
                
                net_interface.active = 1;
                net_interface.ip = 0;
                net_interface.netmask = 0;
                net_interface.gateway = 0;
                
                serial_write_string("[NET] RTL8139 initialized\n");
                return;
            }
        }
    }
    
    serial_write_string("[NET] No RTL8139 found\n");
}

void rtl8139_send(const uint8_t* data, uint16_t length) {
    if (!rtl8139_iobase || length > 1536) return;
    
    // Copy to TX buffer
    for (uint16_t i = 0; i < length; i++) {
        tx_buffer[current_tx][i] = data[i];
    }
    
    // Set TX buffer address
    outl(rtl8139_iobase + RTL8139_TXADDR0 + (current_tx * 4), (uint32_t)tx_buffer[current_tx]);
    
    // Set TX length and start transmission
    outl(rtl8139_iobase + RTL8139_TXSTATUS0 + (current_tx * 4), length);
    
    current_tx = (current_tx + 1) % 4;
}

int rtl8139_receive(uint8_t* buffer, uint16_t* length) {
    if (!rtl8139_iobase) return 0;
    
    // Check command register
    uint8_t cmd = inb(rtl8139_iobase + RTL8139_CMD);
    if (cmd & 0x01) {  // Buffer empty
        return 0;
    }
    
    // Read packet (simplified - real driver needs proper buffer management)
    uint16_t capr = inw(rtl8139_iobase + RTL8139_CAPR);
    
    // Would implement proper RX logic here
    return 0;
}

void network_init(void) {
    serial_write_string("[NET] Initializing network stack...\n");
    rtl8139_init();
}

int network_send_packet(const uint8_t* data, uint16_t length) {
    if (!net_interface.active) return -1;
    rtl8139_send(data, length);
    return 0;
}

int network_receive_packet(uint8_t* buffer, uint16_t* length) {
    if (!net_interface.active) return -1;
    return rtl8139_receive(buffer, length);
}

net_interface_t* network_get_interface(void) {
    return &net_interface;
}

void network_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway) {
    net_interface.ip = ip;
    net_interface.netmask = netmask;
    net_interface.gateway = gateway;
    serial_write_string("[NET] IP configured\n");
}

void network_poll(void) {
    // Poll for incoming packets
    uint8_t buffer[MAX_PACKET_SIZE];
    uint16_t length = 0;
    
    if (network_receive_packet(buffer, &length) == 0 && length > 0) {
        // Process received packet through ethernet layer
        ethernet_receive(buffer, length);
    }
}
