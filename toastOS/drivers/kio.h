#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H
#include "stdint.h"

/* screen configuration */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define KEYBOARD_INPUT_LENGTH 256 /* hey i made a input system :) */

/* Colors, you can print colors */

#define BLACK 0x0
#define BLUE 0x1
#define TOASTOS_BLUE 0x1 /* dark blue for crash screens */
#define GREEN 0x2
#define CYAN 0x3
#define RED 0x4
#define MAGENTA 0x5
#define PURPLE 0x5 /* alias for magenta */
#define BROWN 0x6
#define LIGHT_GREY 0x7
#define DARK_GREY 0x8
#define LIGHT_BLUE 0x9
#define LIGHT_GREEN 0xA
#define LIGHT_CYAN 0xB
#define LIGHT_RED 0xC
#define LIGHT_MAGENTA 0xD
#define YELLOW 0xE
#define WHITE 0xF


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

/* function declarations */
void kb_init(void);
void kprint(const char *str);
void kprint_newline(void);
void clear_screen(void);
void clear_screen_color(uint8_t bg_color);
void keyboard_handler_main(void);
void init_shell();
void panic_init();
void toast_shell_color(const char* str, uint8_t color);
char* rec_input(void);
void shutdown(void);
void reboot(void);
void disk_operations_terminal(void);
void toastos_install(void);
void print_num(uint32_t n);


#endif /* KERNEL_KEYBOARD_H */