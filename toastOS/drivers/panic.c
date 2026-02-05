/* custom built, toastOS Panic handler. toastOS Panic */

#include "kio.h"
#include "funcs.h"

int crashlevel = 0;

void l3_panic(const char *message) {
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
}

void l1_panic(const char *message) {
    if (crashlevel >= 2) {
        l3_panic("The system has crashed more than 1 time and can't continue running.");
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

/* IDT structures */
struct idt_entry {
    unsigned short base_low;  // Lower 16 bits of the function address
    unsigned short sel;       // Kernel segment selector (usually 0x08)
    unsigned char always0;    // Always set to 0
    unsigned char flags;      // Access flags (0x8E for interrupt gates)
    unsigned short base_high; // Higher 16 bits of the function address
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

/* External ISR stubs defined in kernel.asm */
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

/* Registers structure passed from assembly */
typedef struct {
    unsigned int ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;
    unsigned int eip, cs, eflags, useresp, ss;
} registers_t;

/* Exception messages */
static const char *exception_messages[] = {
    "crash! TCode: IMPOSSIBLE_OPERATION", // Divide by 0
    "crash! TCode: DEBUG", // Debug
    "crash! TCode: NON_MASKABLE_INTER", // Non maskable interrupt
    "crash! TCode: BREAK /// Notice: This is a development error.", // Breakpoint
    "crash! TCode: IN_OVERFLOW", // Just In Overflow
    "crash! TCode: ILLEGAL_SPOT", // Illegal spot
    "crash! TCode: INVALID_OPCODE", // Invailid Opcode
    "crash! TCode: MISSING_COPROCESSOR", // No coprocessor
    "crash! TCode: DOUBLE_FAULT", // pretty easy
    "crash! TCode: COPROCESSOR_OVERRUN", // Coprocessor Segment OVerrun
    "crash! TCode: BAD_TSS", // bad tss
    "crash! TCode: SEG_NOT_PRESENT", // segment not presentt
    "crash! TCode: STACK_FAULT", //stack fault
    "crash! TCode: GENERAL_PROT_FAULT", // general prot fault
    "crash! TCode: PAGE_FAULT",
    "crash! TCode: UNKNOWN_INTER",
    "crash! TCode: COPROCESSOR_FAULT",
    "crash! TCode: ALIGN_CHECK",
    "crash! TCode: LOCAL_CHECK", // MACHIONE CHECK
    "crash! TCode: SIMD_FLOATING_POINT_EXCEPTION" // SIMG FLOATING POINT EXCEPT
};

void set_idt_gate(int n, unsigned int handler) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].sel = 0x08; 
    idt[n].always0 = 0;
    idt[n].flags = 0x8E; // Present, Ring 0, Interrupt Gate
}

/* Common ISR handler called from assembly */
void isr_handler(registers_t *regs) {
    switch (regs->int_no) {
        case 0:  // Divide by zero
            kprint_newline();
            toast_shell_color("Kernel fault: Can't divide by 0 or do a impossible operation.", TOASTOS_BLUE);
            kprint_newline();
            kprint("This is a internal error, so toastOS recovered.");
            kprint_newline();
            regs->eip += 2;  // Skip the DIV instruction (hack - assumes 2-byte instruction)
            break;
        case 13: // General Protection Fault
            kprint_newline();
            toast_shell_color("Kernel fault: General protection fault, can continue running.", TOASTOS_BLUE);
            kprint_newline();
            kprint("This is a internal error, so toastOS recovered.");
            kprint_newline();
            regs->eip += 2;  // Skip the DIV instruction (hack - assumes 2-byte instruction)
            break;
        case 14: // Page Fault
            l3_panic("PAGE_FAULT");
            break;
        default: // Everything else
            if (regs->int_no < 20) {
                l3_panic(exception_messages[regs->int_no]);
            } else {
                l3_panic("UNKNOWN_EXCEPTION");
            }
            break;
    }
}

void init_idt() {
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

    /* PIC Initialization */
    write_port(0x20, 0x11);  // ICW1: Initialize + ICW4 needed
    write_port(0xA0, 0x11);
    write_port(0x21, 0x20);  // ICW2: Master PIC vector offset (IRQ0-7 -> INT 0x20-0x27)
    write_port(0xA1, 0x28);  // ICW2: Slave PIC vector offset (IRQ8-15 -> INT 0x28-0x2F)
    write_port(0x21, 0x04);  // ICW3: Master has slave at IRQ2
    write_port(0xA1, 0x02);  // ICW3: Slave cascade identity
    write_port(0x21, 0x01);  // ICW4: 8086 mode
    write_port(0xA1, 0x01);

    /* Mask all interrupts except IRQ0 (Timer) and IRQ1 (keyboard) */
    write_port(0x21, 0xFC);  // 11111100 - IRQ0 and IRQ1 enabled
    write_port(0xA1, 0xFF);  // Mask all slave PIC IRQs

    /* Set up and load the IDT pointer */
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    load_idt((unsigned long *)&idtp);
}