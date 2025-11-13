#ifndef NETCMD_H
#define NETCMD_H

// Network shell command implementations
void network_ifconfig(void);
void network_ping(const char* ip_str);
void network_setip(const char* args);
void network_netstat(void);

#endif
