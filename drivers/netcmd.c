#include "network.h"
#include "icmp.h"
#include "kio.h"
#include "funcs.h"

// Parse IP address from string (e.g., "192.168.1.1")
static uint32_t parse_ip(const char* str) {
    uint32_t ip = 0;
    uint8_t octet = 0;
    int shift = 24;
    
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            octet = octet * 10 + (str[i] - '0');
        } else if (str[i] == '.') {
            ip |= ((uint32_t)octet) << shift;
            shift -= 8;
            octet = 0;
        }
    }
    ip |= ((uint32_t)octet) << shift;
    
    return ip;
}

void network_ifconfig(void) {
    net_interface_t* iface = network_get_interface();
    
    if (!iface->active) {
        toast_shell_color("No network interface found", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    toast_shell_color("Network Interface:", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    kprint("  MAC Address: ");
    for (int i = 0; i < 6; i++) {
        if (iface->mac[i] < 16) kprint("0");
        kprint_int(iface->mac[i]);
        if (i < 5) kprint(":");
    }
    kprint_newline();
    
    kprint("  IP Address:  ");
    if (iface->ip == 0) {
        toast_shell_color("Not configured", YELLOW);
    } else {
        uint8_t* ip_bytes = (uint8_t*)&iface->ip;
        kprint_int(ip_bytes[0]);
        kprint(".");
        kprint_int(ip_bytes[1]);
        kprint(".");
        kprint_int(ip_bytes[2]);
        kprint(".");
        kprint_int(ip_bytes[3]);
    }
    kprint_newline();
    
    kprint("  Netmask:     ");
    if (iface->netmask == 0) {
        toast_shell_color("Not configured", YELLOW);
    } else {
        uint8_t* nm_bytes = (uint8_t*)&iface->netmask;
        kprint_int(nm_bytes[0]);
        kprint(".");
        kprint_int(nm_bytes[1]);
        kprint(".");
        kprint_int(nm_bytes[2]);
        kprint(".");
        kprint_int(nm_bytes[3]);
    }
    kprint_newline();
    
    kprint("  Gateway:     ");
    if (iface->gateway == 0) {
        toast_shell_color("Not configured", YELLOW);
    } else {
        uint8_t* gw_bytes = (uint8_t*)&iface->gateway;
        kprint_int(gw_bytes[0]);
        kprint(".");
        kprint_int(gw_bytes[1]);
        kprint(".");
        kprint_int(gw_bytes[2]);
        kprint(".");
        kprint_int(gw_bytes[3]);
    }
    kprint_newline();
    kprint_newline();
    
    if (iface->ip == 0) {
        toast_shell_color("TIP: Configure network with:", YELLOW);
        kprint_newline();
        kprint("     setip 10.0.2.15 255.255.255.0 10.0.2.2");
        kprint_newline();
        kprint("     Then try: ping 10.0.2.2");
        kprint_newline();
    }
}

void network_ping(const char* ip_str) {
    uint32_t dest_ip = parse_ip(ip_str);
    
    if (dest_ip == 0) {
        toast_shell_color("Invalid IP address", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    net_interface_t* iface = network_get_interface();
    if (iface->ip == 0) {
        toast_shell_color("Network not configured. Use 'setip' first.", YELLOW);
        kprint_newline();
        return;
    }
    
    kprint("PING ");
    kprint(ip_str);
    kprint(" - sending 4 packets...");
    kprint_newline();
    
    for (int i = 0; i < 4; i++) {
        kprint("Sending packet ");
        kprint_int(i + 1);
        kprint("...");
        kprint_newline();
        
        icmp_send_echo_request(dest_ip, 1234, i);
        
        // Poll network for responses during wait
        for (volatile int j = 0; j < 5000000; j++) {
            if (j % 100000 == 0) {
                network_poll();
            }
        }
    }
    
    kprint_newline();
    toast_shell_color("Ping complete. Check serial output for replies.", LIGHT_CYAN);
    kprint_newline();
}

void network_setip(const char* args) {
    // Parse: setip 192.168.1.100 255.255.255.0 192.168.1.1
    uint32_t ip = 0, netmask = 0, gateway = 0;
    
    // Simple parsing (space-separated)
    const char* ptr = args;
    int field = 0;
    char buf[16];
    int buf_idx = 0;
    
    while (*ptr != '\0') {
        if (*ptr == ' ' || *(ptr + 1) == '\0') {
            if (*(ptr + 1) == '\0' && *ptr != ' ') {
                buf[buf_idx++] = *ptr;
            }
            buf[buf_idx] = '\0';
            
            if (field == 0) ip = parse_ip(buf);
            else if (field == 1) netmask = parse_ip(buf);
            else if (field == 2) gateway = parse_ip(buf);
            
            field++;
            buf_idx = 0;
            ptr++;
        } else {
            buf[buf_idx++] = *ptr++;
        }
    }
    
    network_set_ip(ip, netmask, gateway);
    toast_shell_color("IP configuration updated. Use 'ifconfig' to view.", LIGHT_GREEN);
    kprint_newline();
}

void network_netstat(void) {
    toast_shell_color("Network Statistics:", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    kprint("  Protocol support:");
    kprint_newline();
    toast_shell_color("    [OK] ", LIGHT_GREEN);
    kprint("Ethernet");
    kprint_newline();
    toast_shell_color("    [OK] ", LIGHT_GREEN);
    kprint("ARP");
    kprint_newline();
    toast_shell_color("    [OK] ", LIGHT_GREEN);
    kprint("IPv4");
    kprint_newline();
    toast_shell_color("    [OK] ", LIGHT_GREEN);
    kprint("ICMP (ping)");
    kprint_newline();
    toast_shell_color("    [ - ] ", YELLOW);
    kprint("UDP (coming soon)");
    kprint_newline();
    toast_shell_color("    [ - ] ", YELLOW);
    kprint("TCP (coming soon)");
    kprint_newline();
}
