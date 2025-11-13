/* show me carfax.com */
#ifndef FUNCS_H
#define FUNCS_H

#include "stdint.h"

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
    /* outb is sourced from https://osdev.org */
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ( "inb %w1, %b0"
                   : "=a"(ret)
                   : "Nd"(port)
                   : "memory");
    return ret;
    /* same as well for inb */
}

// 16-bit port I/O for disk operations
static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile ( "outw %w0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile ( "inw %w1, %w0"
                   : "=a"(ret)
                   : "Nd"(port)
                   : "memory");
    return ret;
}

// 32-bit port I/O for PCI and network operations
static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile ( "outl %0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile ( "inl %w1, %0"
                   : "=a"(ret)
                   : "Nd"(port)
                   : "memory");
    return ret;
}

// Serial port functions for debug output
#define SERIAL_PORT 0x3F8

static inline void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00);    // Disable interrupts
    outb(SERIAL_PORT + 3, 0x80);    // Enable DLAB
    outb(SERIAL_PORT + 0, 0x03);    // Set divisor to 3 (38400 baud)
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(SERIAL_PORT + 2, 0xC7);    // Enable FIFO
    outb(SERIAL_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static inline int serial_is_transmit_empty(void) {
    return inb(SERIAL_PORT + 5) & 0x20;
}

static inline void serial_write_char(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(SERIAL_PORT, c);
}

static inline void serial_write_string(const char *str) {
    while (*str) {
        serial_write_char(*str++);
    }
}

static void kprint_int(int n) {
    if (n < 0) {
        serial_write_string("-");
        n = -n;
    }
    if (n / 10) {
        kprint_int(n / 10);
    }
    char digit = (n % 10) + '0';
    serial_write_char(digit);
}

#endif // FUNCS_H