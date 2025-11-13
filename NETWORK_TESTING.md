# toastOS Network Stack Testing Guide

## Network Stack Overview
toastOS now includes a complete TCP/IP stack with:
- **RTL8139 NIC Driver** (automatically detected in QEMU)
- **Ethernet Layer** (frame handling)
- **ARP Protocol** (MAC address resolution with 16-entry cache)
- **IPv4** (with checksum and routing)
- **ICMP** (ping support with echo request/reply)

## Quick Start

### 1. Build and Run
```bash
./build-mac.sh
```

The build script automatically launches QEMU with RTL8139 network card attached.

### 2. Configure Network
In the toastOS shell, type:
```
ifconfig
```

This shows your network interface. If not configured, run:
```
setip 10.0.2.15 255.255.255.0 10.0.2.2
```

### 3. Test Ping
Try pinging the QEMU gateway:
```
ping 10.0.2.2
```

You should see:
- Packet send messages
- ARP resolution attempts (in serial output)
- ICMP echo replies (on screen)

### 4. Check Network Status
```
netstat
```

Shows supported protocols and status.

## QEMU User Networking

The build script uses QEMU's user-mode networking:
- Your toastOS IP: `10.0.2.15`
- Gateway/DNS: `10.0.2.2`
- Network: `10.0.2.0/24`

## Commands Available

- `ifconfig` - Show network interface details
- `setip <ip> <netmask> <gateway>` - Configure IP address
- `ping <ip>` - Send ICMP echo requests (4 packets)
- `netstat` - Show network statistics
- `help` - Full command list

## Debugging

All network activity is logged to serial output (stdio).
Look for messages like:
- `[NET] RTL8139 found at PCI...`
- `[ARP] Received ARP packet...`
- `[IP] Sending packet...`
- `[NET] Received ICMP echo reply...`

## Known Limitations

- UDP and TCP are stub implementations (coming soon)
- No packet queuing (ARP must resolve before send)
- No socket API yet
- Ping waits are CPU-based (no proper timers)

## Troubleshooting

**No network interface found:**
- Check QEMU is running with `-device rtl8139,netdev=net0`
- Verify PCI device detection in serial output

**Ping fails:**
- Make sure you ran `setip` first
- Try pinging gateway (10.0.2.2) first
- Check serial output for ARP resolution messages

**ARP resolution fails:**
- QEMU user networking should respond automatically
- Verify IP is in 10.0.2.0/24 range
- Check network is polling (should happen in main loop)
