#include "idt.h"
#include <string.h>

extern void idt_load(uint32_t);

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr idtp;

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
    idt[num].base_high = (base >> 16) & 0xFFFF;
}

void idt_init() {
    memset(&idt, 0, sizeof(struct idt_entry) * IDT_ENTRIES);
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base = (uint32_t)&idt;
    // Example: set divide-by-zero handler (interrupt 0)
    extern void isr0();
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_load((uint32_t)&idtp);
}
