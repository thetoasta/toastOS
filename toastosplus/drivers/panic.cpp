/*
 * toastOS++ Panic Handler
 * Namespace: toast::sys
 */

#include "panic.hpp"
#include "kio.hpp"
#include "funcs.hpp"

namespace toast {
namespace sys {

namespace {  // anonymous namespace for internal state

int crashlevel = 0;

/* IDT structures */
struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char always0;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

idt_entry idt[256];
idt_ptr idtp;

/* Exception messages */
const char *exception_messages[] = {
    "IMPOSSIBLE_OPERATION",
    "DEBUG",
    "NON_MASKABLE_INTER",
    "BREAK",
    "IN_OVERFLOW",
    "ILLEGAL_SPOT",
    "INVALID_OPCODE",
    "MISSING_COPROCESSOR",
    "DOUBLE_FAULT",
    "COPROCESSOR_OVERRUN",
    "BAD_TSS",
    "SEG_NOT_PRESENT",
    "STACK_FAULT",
    "GENERAL_PROT_FAULT",
    "PAGE_FAULT",
    "UNKNOWN_INTER",
    "COPROCESSOR_FAULT",
    "ALIGN_CHECK",
    "LOCAL_CHECK",
    "SIMD_FLOATING_POINT_EXCEPTION"
};

/* Registers structure passed from assembly */
struct registers_t {
    unsigned int ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;
    unsigned int eip, cs, eflags, useresp, ss;
};

void set_idt_gate(int n, unsigned int handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].always0 = 0;
    idt[n].flags = 0x8E;
}

} // anonymous namespace

void warn(const char* message) {
    if (crashlevel >= 2) {
        panic("The system has crashed more than 1 time and can't continue running.");
    }
    clear_screen_color(LIGHT_RED);
    kprint_newline();
    kprint_newline();
    kprint("    :O");
    kprint_newline();
    kprint_newline();
    kprint("    toastOS ran into a problem and CAN recover. Tap enter to recover.");
    kprint_newline();
    kprint_newline();
    kprint("    Here's what went wrong:");
    kprint_newline();
    kprint("    ");
    kprint(message);
    kprint_newline();
    kprint_newline();
    kprint_newline();
    kprint("    For more information, visit: github.com/thetoasta/toastOS");
    crashlevel++;
    rec_input();
    clear_screen_color(BLACK);
}

[[noreturn]] void panic(const char* message) {
    clear_screen_color(TOASTOS_BLUE);
    kprint_newline();
    kprint_newline();
    kprint("    :(");
    kprint_newline();
    kprint_newline();
    kprint("    toastOS ran into a problem and can't recover.");
    kprint_newline();
    kprint_newline();
    kprint("    Here's what went wrong:");
    kprint_newline();
    kprint("    ");
    kprint(message);
    kprint_newline();
    kprint_newline();
    kprint_newline();
    kprint("    For more information, visit: github.com/thetoasta/toastOS");
    __asm__ volatile ("cli; hlt");
    while(1);  // Never reached
}

} // namespace sys
} // namespace toast

/* External ISR stubs defined in kernel.asm - need C linkage */
extern "C" {
    extern void isr0();
    extern void isr1();
    extern void isr2();
    extern void isr3();
    extern void isr4();
    extern void isr5();
    extern void isr6();
    extern void isr7();
    extern void isr8();
    extern void isr9();
    extern void isr10();
    extern void isr11();
    extern void isr12();
    extern void isr13();
    extern void isr14();
    extern void isr15();
    extern void isr16();
    extern void isr17();
    extern void isr18();
    extern void isr19();

    extern void irq0_handler();
    extern void keyboard_handler();
    extern void syscall_isr();
}

/* Common ISR handler called from assembly */
extern "C" void isr_handler(toast::sys::registers_t *regs) {
    switch (regs->int_no) {
        case 0:
            kprint_newline();
            toast_shell_color("Kernel fault: Can't divide by 0 or do a impossible operation.", TOASTOS_BLUE);
            kprint_newline();
            kprint("This is a internal error, so toastOS recovered.");
            kprint_newline();
            regs->eip += 2;
            break;
        case 13:
            kprint_newline();
            toast_shell_color("Kernel fault: General protection fault, can continue running.", TOASTOS_BLUE);
            kprint_newline();
            kprint("This is a internal error, so toastOS recovered.");
            kprint_newline();
            regs->eip += 2;
            break;
        case 14:
            toast::sys::panic("PAGE_FAULT");
            break;
        default:
            if (regs->int_no < 20) {
                toast::sys::panic(toast::sys::exception_messages[regs->int_no]);
            } else {
                toast::sys::panic("UNKNOWN_EXCEPTION");
            }
            break;
    }
}

namespace toast {
namespace sys {

void init() {
    /* Clear the IDT */
    for (int i = 0; i < 256; i++) {
        idt[i].base_low = 0;
        idt[i].sel = 0;
        idt[i].always0 = 0;
        idt[i].flags = 0;
        idt[i].base_high = 0;
    }

    /* Set up CPU exception handlers (ISR 0-19) */
    set_idt_gate(0, (unsigned int)isr0);
    set_idt_gate(1, (unsigned int)isr1);
    set_idt_gate(2, (unsigned int)isr2);
    set_idt_gate(3, (unsigned int)isr3);
    set_idt_gate(4, (unsigned int)isr4);
    set_idt_gate(5, (unsigned int)isr5);
    set_idt_gate(6, (unsigned int)isr6);
    set_idt_gate(7, (unsigned int)isr7);
    set_idt_gate(8, (unsigned int)isr8);
    set_idt_gate(9, (unsigned int)isr9);
    set_idt_gate(10, (unsigned int)isr10);
    set_idt_gate(11, (unsigned int)isr11);
    set_idt_gate(12, (unsigned int)isr12);
    set_idt_gate(13, (unsigned int)isr13);
    set_idt_gate(14, (unsigned int)isr14);
    set_idt_gate(15, (unsigned int)isr15);
    set_idt_gate(16, (unsigned int)isr16);
    set_idt_gate(17, (unsigned int)isr17);
    set_idt_gate(18, (unsigned int)isr18);
    set_idt_gate(19, (unsigned int)isr19);
    
    /* Set up IRQ0 (Timer) */
    set_idt_gate(0x20, (unsigned int)irq0_handler);

    /* Set up keyboard interrupt (IRQ1 = INT 0x21) */
    set_idt_gate(0x21, (unsigned int)keyboard_handler);

    /* INT 0x80 - Syscall interface (ring 3 callable) */
    idt[0x80].base_low = ((unsigned int)syscall_isr) & 0xFFFF;
    idt[0x80].base_high = (((unsigned int)syscall_isr) >> 16) & 0xFFFF;
    idt[0x80].sel = 0x08;
    idt[0x80].always0 = 0;
    idt[0x80].flags = 0xEE;

    /* PIC Initialization */
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);
    write_port(0x21, 0x04);
    write_port(0xA1, 0x02);
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);

    /* Mask all interrupts except IRQ0 (Timer) and IRQ1 (keyboard) */
    write_port(0x21, 0xFC);
    write_port(0xA1, 0xFF);

    /* Set up and load the IDT pointer */
    idtp.limit = (sizeof(idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    load_idt((unsigned long *)&idtp);
}

} // namespace sys
} // namespace toast

/* Legacy C-style aliases */
void l1_panic(const char* message) { toast::sys::warn(message); }
void l3_panic(const char* message) { toast::sys::panic(message); }
void init_idt() { toast::sys::init(); }
