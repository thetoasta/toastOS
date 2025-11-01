#include "kio.h"
#include "funcs.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES
/* needed stuff for a ib? */

char input_buffer[KEYBOARD_INPUT_LENGTH];
unsigned int input_index = 0;

static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void shutdown() {
    // Try all common emulator shutdown ports
    outw(0x604, 0x2000); // QEMU/Bochs
    outw(0xB004, 0x2000); // Older Bochs
    __asm__ volatile ("cli; hlt"); // Halt CPU if above doesn't work
}

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08
#define KEYBOARD_INPUT_LENGTH 256 /* hey i made a input system :) */
#define ENTER_KEY_CODE 0x1C /* enter code that is a key */

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

/* current cursor location */
unsigned int current_loc = 0;
/* video memory begins at address 0xb8000 */
char *vidptr = (char*)0xb8000;

struct IDT_entry IDT[IDT_SIZE];


void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*     Ports
	*	 PIC1	PIC2
	*Command 0x20	0xA0
	*Data	 0x21	0xA1
	*/

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	* In x86 protected mode, we have to remap the PICs beyond 0x20 because
	* Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	*/
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);
	write_port(0xA1 , 0x00);

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
	}
}

void keyboard_handler_main(void)
{
    unsigned char status;
    char keycode;

    write_port(0x20, 0x20);

    status = read_port(KEYBOARD_STATUS_PORT);
    if (status & 0x01) {
        keycode = read_port(KEYBOARD_DATA_PORT);
        if (keycode < 0)
            return;

        if (keycode == ENTER_KEY_CODE) {
            // finish the current input line
            kprint_newline();
            input_buffer[input_index] = '\0';  // null-terminate

            // process the input (print it, compare, etc)
            if (strcmp(input_buffer, "hai") == 0) {
                kprint("hai");
                current_loc = 0;
            } else if (strcmp(input_buffer, "clear") == 0) {
                clear_screen();
                current_loc = 0;
            } else if (strcmp(input_buffer, "help") == 0) {
                kprint("there is several commands thast is in toastOS 1.0. try 'system-quickinfo' for quick info and system data");
            } else if (strcmp(input_buffer, "system-quickinfo") == 0) {
                kprint("hey! toastOS is a simple os made by thetoasta! heavy dev rn, and there is more to come. ");
                kprint_newline();
                kprint("version 1.0 - thetoasta - 2025 report issues on thetoasta/toastOS.");
            } else if (strcmp(input_buffer, "fs-testfile") == 0) {
                kprint("testfile is being written.");
            } else if (strcmp(input_buffer, "") == 0) {
                // do nothing for empty input
            } else if (strcmp(input_buffer, "shutdown") == 0) {
                kprint("shutting down...");
                shutdown();
            } else {
                kprint("toastOS doesn't know that command yet :( if you'd like to add it, open a pr with the command and or add a issue!");
            }

            // reset buffer and show a fresh prompt
            input_index = 0; // reset buffer
            kprint_newline();
            kprint("toastOS > ");
            return;
        }

        char c = keyboard_map[(unsigned char)keycode];
        if (c) {
            if (input_index < KEYBOARD_INPUT_LENGTH - 1) {
                input_buffer[input_index++] = c;
                vidptr[current_loc++] = c;
                vidptr[current_loc++] = 0x07;
            }
        }
    }
}

void init_shell() {
  clear_screen();
	kprint("Welcome to toastOS! ");
	kprint_newline();
  kprint("toastOS > ");

	idt_init();
	kb_init();
}

unsigned char keyboard_map[128] =
 {
     0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
   '9', '0', '-', '=', '\b',	/* Backspace */
   '\t',			/* Tab */
   'q', 'w', 'e', 'r',	/* 19 */
   't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
     0,			/* 29   - Control */
   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
  '\'', '`',   0,		/* Left shift */
  '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
   'm', ',', '.', '/',   0,				/* Right shift */
   '*',
     0,	/* Alt */
   ' ',	/* Space bar */
     0,	/* Caps lock */
     0,	/* 59 - F1 key ... > */
     0,   0,   0,   0,   0,   0,   0,   0,
     0,	/* < ... F10 */
     0,	/* 69 - Num lock*/
     0,	/* Scroll Lock */
     0,	/* Home key */
     0,	/* Up Arrow */
     0,	/* Page Up */
   '-',
     0,	/* Left Arrow */
     0,
     0,	/* Right Arrow */
   '+',
     0,	/* 79 - End key*/
     0,	/* Down Arrow */
     0,	/* Page Down */
     0,	/* Insert Key */
     0,	/* Delete Key */
     0,   0,   0,
     0,	/* F11 Key */
     0,	/* F12 Key */
     0,	/* All other keys are undefined */
 };
