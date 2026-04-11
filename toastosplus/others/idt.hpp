/*
 * toastOS++ Idt
 * Converted to C++ from toastOS
 */

#ifndef IDT_HPP
#define IDT_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.hpp"

#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void idt_init();

#ifdef __cplusplus
}
#endif

#endif /* IDT_HPP */
