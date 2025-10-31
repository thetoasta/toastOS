#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

/* screen configuration */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define KEYBOARD_INPUT_LENGTH 256 /* hey i made a input system :) */

/* I/O ports and constants */
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08
#define ENTER_KEY_CODE 0x1C

/* external symbols */
extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

/* IDT entry structure */
struct IDT_entry {
    unsigned short int offset_lowerbits;
    unsigned short int selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short int offset_higherbits;
};

/* global variables */
extern unsigned int current_loc;
extern char *vidptr;
extern struct IDT_entry IDT[IDT_SIZE];

/* function declarations */
void idt_init(void);
void kb_init(void);
void kprint(const char *str);
void kprint_newline(void);
void clear_screen(void);
void keyboard_handler_main(void);
void init_shell();




#endif /* KERNEL_KEYBOARD_H */