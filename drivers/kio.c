#include "kio.h"
#include "funcs.h"
#include "panic.h"
#include "stdint.h"

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_PER_ELEMENT 2
#define SCREENSIZE (BYTES_PER_ELEMENT * COLUMNS_IN_LINE * LINES)

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08
#define KEYBOARD_INPUT_LENGTH 256
#define ENTER_KEY_CODE 0x1C

char input_buffer[KEYBOARD_INPUT_LENGTH];
unsigned int input_index = 0;

unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

struct IDT_entry IDT[IDT_SIZE];

static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}



void update_cursor(int x, int y) {
    uint16_t pos = y * COLUMNS_IN_LINE + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void disable_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void shutdown() {
    outw(0x604, 0x2000);  // QEMU / Bochs
    outw(0xB004, 0x2000); // Older Bochs
    __asm__ volatile("cli; hlt"); // Halt CPU if shutdown ports fail
}

void idt_init(void) {
    unsigned long keyboard_address = (unsigned long)keyboard_handler;
    unsigned long idt_address;
    unsigned long idt_ptr[2];

    IDT[0x21].offset_lowerbits = keyboard_address & 0xFFFF;
    IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[0x21].zero = 0;
    IDT[0x21].type_attr = INTERRUPT_GATE;
    IDT[0x21].offset_higherbits = (keyboard_address >> 16) & 0xFFFF;

    // PIC Initialization
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);
    write_port(0x21, 0x00);
    write_port(0xA1, 0x00);
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);

    // Mask all interrupts initially
    write_port(0x21, 0xFF);
    write_port(0xA1, 0xFF);

    idt_address = (unsigned long)IDT;
    idt_ptr[0] = (sizeof(struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xFFFF) << 16);
    idt_ptr[1] = idt_address >> 16;

    load_idt(idt_ptr);
}

void kb_init(void) {
    // Enable only IRQ1 (keyboard)
    write_port(0x21, 0xFD);
}

void kprint(const char *str) {
    unsigned int i = 0;
    while (str[i]) {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = 0x07; // attribute byte
    }
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

void kprint_newline(void) {
    unsigned int line_size = BYTES_PER_ELEMENT * COLUMNS_IN_LINE;
    current_loc += (line_size - (current_loc % line_size));
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

void clear_screen(void) {
    for (unsigned int i = 0; i < SCREENSIZE; i += 2) {
        vidptr[i] = ' ';
        vidptr[i + 1] = 0x07;
    }
    current_loc = 0;
    update_cursor(0, 0);
}

void keyboard_handler_main(void) {
    unsigned char status = read_port(KEYBOARD_STATUS_PORT);
    write_port(0x20, 0x20); // End of interrupt signal

    if (status & 0x01) {
        char keycode = read_port(KEYBOARD_DATA_PORT);
        
        if (keycode == ENTER_KEY_CODE) {
            kprint_newline();
            input_buffer[input_index] = '\0';
            if (current_loc >= SCREENSIZE) {
                clear_screen();
            }
            if (strcmp(input_buffer, "hai") == 0) {
                kprint("hai");
            } else if (strcmp(input_buffer, "clear") == 0) {
                clear_screen();
            } else if (strcmp(input_buffer, "help") == 0) {
                kprint("Several commands in toastOS 1.0. Try 'system-quickinfo' for info.");
            } else if (strcmp(input_buffer, "system-quickinfo") == 0) {
                kprint("toastOS by thetoasta, version 1.0 - 2025");
                kprint_newline();
                kprint("Report issues on thetoasta/toastOS.");
            } else if (strcmp(input_buffer, "panic") == 0) {
                kprint("PCI.");
                l1_panic("Manual panic triggered.");
            } else if (strcmp(input_buffer, "mpanic") == 0) {
                l3_panic("fatal panic");
            } else if (strcmp(input_buffer, "fs-testfile") == 0) {
                kprint("testfile is being written.");
            } else if (strcmp(input_buffer, "cursor-disable") == 0) {
                kprint("Disabling cursor.");
                disable_cursor();
            } else if (strcmp(input_buffer, "shutdown") == 0) {
                kprint("shutting down...");
                shutdown();
            } else if (strcmp(input_buffer, "") != 0) {
                kprint("Command not recognized. Open a PR or issue to add it!");
            }

            input_index = 0;
            kprint_newline();
            kprint("toastOS > ");
            return;
        }

        char c = keyboard_map[(unsigned char)keycode];
        if (c == '\b' && input_index > 0) {
            // Handle backspace: remove the last character from the buffer and screen
            input_index--;
            current_loc -= 2; // Move cursor back
            vidptr[current_loc] = ' '; // Clear the character on screen
            vidptr[current_loc + 1] = 0x07; // Reset attribute byte
            update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
        } else if (c && input_index < KEYBOARD_INPUT_LENGTH - 1) {
            input_buffer[input_index++] = c;
            vidptr[current_loc++] = c;
            vidptr[current_loc++] = 0x07;
            update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
        }
    }
}

void init_shell(void) {
    clear_screen();
    kprint("Welcome to toastOS!");
    kprint_newline();
    kprint("toastOS > ");
    enable_cursor(0, 15);
    idt_init();
    kb_init();
}

void panic_init(void) {
    kprint("Welcome BACK to toastOS!");
    kprint_newline();
    kprint("toastOS > ");
    enable_cursor(0, 15);
    idt_init();
    kb_init();
}

void toast_shell_color(const char* str, uint8_t color) {
    unsigned int i = 0;
    while (str[i]) {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = color;
    }
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 9 */
    '9', '0', '-', '=', '\b',    /* Backspace */
    '\t',            /* Tab */
    'q', 'w', 'e', 'r',  /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',    /* Enter */
    0,         /* Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 39 */
    '\'', '`',   0,       /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',           /* 49 */
    'm', ',', '.', '/',   0,             /* Right shift */
    '*',
    0, /* Alt */
    ' ', /* Space */
    0, /* Caps lock */
    0, /* F1 - F10 keys */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, /* F10 */
    0, /* Num lock */
    0, /* Scroll lock */
    0, /* Home */
    0, /* Up Arrow */
    0, /* Page Up */
    '-',
    0, /* Left Arrow */
    0,
    0, /* Right Arrow */
    '+',
    0, /* End */
    0, /* Down Arrow */
    0, /* Page Down */
    0, /* Insert */
    0, /* Delete */
    0, 0, 0,
    0, /* F11 */
    0, /* F12 */
    0  /* Undefined keys */
};
