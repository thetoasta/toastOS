/*
 * toastOS++ Funcs
 * Converted to C++ from toastOS
 */

#ifndef FUNCS_HPP
#define FUNCS_HPP

#ifdef __cplusplus
extern "C" {
#endif

/* show me carfax.com */
#include "stdint.hpp"

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

#ifdef __cplusplus
}
#endif

#endif /* FUNCS_HPP */
