#include "kio.h"
#include "funcs.h"
#include "panic.h"
#include "netcmd.h"
#include "stdint.h"
#include "file.h"
#include "security.h"
#include "services.h"
#include "rtc.h"
#include "apps.h"

int exceptionCount = 0;

void exceptionCheck() {
    if (exceptionCount > 1) {
        l3_panic("Sorry, but toastOS experienced too many exceptions and is automatically shutting down for safety.");
    }
    
}

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
#define HISTORY_SIZE 10

char input_buffer[KEYBOARD_INPUT_LENGTH];
unsigned int input_index = 0;

// Shift and caps lock state
static int shift_pressed = 0;
static int caps_lock = 0;

// Command history
char command_history[HISTORY_SIZE][KEYBOARD_INPUT_LENGTH];
int history_count = 0;
int history_index = -1;
int browsing_history = 0;

unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;

extern unsigned char keyboard_map[128];
extern unsigned char keyboard_map_shifted[128];
extern void keyboard_handler(void);
extern void timer_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

// Get character with shift/caps lock handling
static inline char get_char_from_keycode(unsigned char keycode) {
    char c;
    
    // Get base character
    if (shift_pressed) {
        c = keyboard_map_shifted[keycode];
    } else {
        c = keyboard_map[keycode];
    }
    
    // Apply caps lock for letters only
    if (caps_lock && c >= 'a' && c <= 'z') {
        c = c - 32; // Convert to uppercase
    } else if (caps_lock && c >= 'A' && c <= 'Z') {
        c = c + 32; // Convert to lowercase
    }
    
    return c;
}

// External ISR declarations
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

struct IDT_entry IDT[IDT_SIZE];


void accept_fs_write() {
    kprint("toastFS Opened. What do you want the FILENAME to be?");
    kprint_newline();
    char* filename = rec_input();
    kprint("What content do you want to write to the file?");

    char* content = rec_input();
    local_fs(filename, content);
    kprint_newline();
}

void isr_handler(unsigned int interrupt_number) {
    static const char *exceptions[] = {
        "Divide By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "Bound Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack-Segment Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 Floating Point Exception",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating Point Exception",
        "Virtualization Exception",
        "Control Protection Exception",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved"
    };

    // Output to serial port (terminal)
    serial_write_string("\n=================================\n");
    serial_write_string("!!! CPU EXCEPTION DETECTED !!!\n");
    serial_write_string("=================================\n");
    serial_write_string("Exception #");
    exceptionCount++;
    kprint_int(interrupt_number);
    serial_write_string(": ");
    if (interrupt_number < 32) {
        serial_write_string(exceptions[interrupt_number]);
    } else {
        serial_write_string("Unknown Exception");
    }
    serial_write_string("\n=================================\n\n");

    clear_screen();
    kprint("\n=== CPU EXCEPTION ===\n");
    exceptionCheck();
    
    // Critical faults trigger l3_panic (major panic)
    if (interrupt_number == 8 || // Double Fault
        interrupt_number == 13 || // General Protection Fault
        interrupt_number == 14) { // Page Fault
        l3_panic(exceptions[interrupt_number]);
    }
    // Other exceptions trigger l1_panic (minor panic)
    else if (interrupt_number < 32) {
        l1_panic(exceptions[interrupt_number]);
    }

    // Freeze the system
    while(1) {
        __asm__ volatile("hlt");
    }
}


void read_fs_contents() {
    kprint("WARNING: Make sure the file ID is correct.");
    kprint_newline();
    kprint("Enter the file ID to read:");
    char* file_id = rec_input();
    kprint_newline();
    read_local_fs(file_id);
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

static void set_idt_gate(int num, unsigned long handler) {
    IDT[num].offset_lowerbits = handler & 0xFFFF;
    IDT[num].selector = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[num].zero = 0;
    IDT[num].type_attr = INTERRUPT_GATE;
    IDT[num].offset_higherbits = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    unsigned long idt_address;
    unsigned long idt_ptr[2];

    // Register CPU exception handlers (ISR 0-31)
    set_idt_gate(0, (unsigned long)isr0);
    set_idt_gate(1, (unsigned long)isr1);
    set_idt_gate(2, (unsigned long)isr2);
    set_idt_gate(3, (unsigned long)isr3);
    set_idt_gate(4, (unsigned long)isr4);
    set_idt_gate(5, (unsigned long)isr5);
    set_idt_gate(6, (unsigned long)isr6);
    set_idt_gate(7, (unsigned long)isr7);
    set_idt_gate(8, (unsigned long)isr8);
    set_idt_gate(9, (unsigned long)isr9);
    set_idt_gate(10, (unsigned long)isr10);
    set_idt_gate(11, (unsigned long)isr11);
    set_idt_gate(12, (unsigned long)isr12);
    set_idt_gate(13, (unsigned long)isr13);
    set_idt_gate(14, (unsigned long)isr14);
    set_idt_gate(15, (unsigned long)isr15);
    set_idt_gate(16, (unsigned long)isr16);
    set_idt_gate(17, (unsigned long)isr17);
    set_idt_gate(18, (unsigned long)isr18);
    set_idt_gate(19, (unsigned long)isr19);
    set_idt_gate(20, (unsigned long)isr20);
    set_idt_gate(21, (unsigned long)isr21);
    set_idt_gate(22, (unsigned long)isr22);
    set_idt_gate(23, (unsigned long)isr23);
    set_idt_gate(24, (unsigned long)isr24);
    set_idt_gate(25, (unsigned long)isr25);
    set_idt_gate(26, (unsigned long)isr26);
    set_idt_gate(27, (unsigned long)isr27);
    set_idt_gate(28, (unsigned long)isr28);
    set_idt_gate(29, (unsigned long)isr29);
    set_idt_gate(30, (unsigned long)isr30);
    set_idt_gate(31, (unsigned long)isr31);

    // Register keyboard handler (IRQ1 = ISR 33 = 0x21)
    unsigned long keyboard_address = (unsigned long)keyboard_handler;
    set_idt_gate(0x21, keyboard_address);

    // Register timer handler (IRQ0 = ISR 32 = 0x20)
    unsigned long timer_address = (unsigned long)timer_handler;
    set_idt_gate(0x20, timer_address);

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
    // Enable IRQ0 (timer) and IRQ1 (keyboard)
    write_port(0x21, 0xFC);  // 0xFC = 11111100 (bits 0 and 1 clear)
}

void kprint(const char *str) {
    unsigned int i = 0;
    while (str[i]) {
        // Handle newline character
        if (str[i] == '\n') {
            kprint_newline();
            i++;
            continue;
        }
        
        // Don't overwrite clock area (line 0, columns 63-79)
        int current_x = (current_loc / 2) % COLUMNS_IN_LINE;
        int current_y = (current_loc / 2) / COLUMNS_IN_LINE;
        if (current_y == 0 && current_x >= 63) {
            // Skip to next line to preserve clock
            current_loc = BYTES_PER_ELEMENT * COLUMNS_IN_LINE;
        }
        
        // Auto-scroll if we're at the bottom
        if (current_loc >= SCREENSIZE) {
            // Scroll up by one line, but preserve line 0 (clock area)
            // Save line 0
            char line0_backup[BYTES_PER_ELEMENT * COLUMNS_IN_LINE];
            for (unsigned int j = 0; j < BYTES_PER_ELEMENT * COLUMNS_IN_LINE; j++) {
                line0_backup[j] = vidptr[j];
            }
            
            // Scroll up
            for (unsigned int j = 0; j < SCREENSIZE - (BYTES_PER_ELEMENT * COLUMNS_IN_LINE); j++) {
                vidptr[j] = vidptr[j + (BYTES_PER_ELEMENT * COLUMNS_IN_LINE)];
            }
            
            // Clear last line
            for (unsigned int j = SCREENSIZE - (BYTES_PER_ELEMENT * COLUMNS_IN_LINE); j < SCREENSIZE; j += 2) {
                vidptr[j] = ' ';
                vidptr[j + 1] = 0x07;
            }
            
            // Restore line 0
            for (unsigned int j = 0; j < BYTES_PER_ELEMENT * COLUMNS_IN_LINE; j++) {
                vidptr[j] = line0_backup[j];
            }
            
            current_loc = SCREENSIZE - (BYTES_PER_ELEMENT * COLUMNS_IN_LINE);
        }
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
    
    // Auto-scroll if we're past the bottom
    if (current_loc >= SCREENSIZE) {
        // Scroll up by one line
        for (unsigned int j = 0; j < SCREENSIZE - line_size; j++) {
            vidptr[j] = vidptr[j + line_size];
        }
        // Clear last line
        for (unsigned int j = SCREENSIZE - line_size; j < SCREENSIZE; j += 2) {
            vidptr[j] = ' ';
            vidptr[j + 1] = 0x07;
        }
        current_loc = SCREENSIZE - line_size;
    }
    
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

void clear_screen(void) {
    // Clear screen but preserve line 0 (clock area)
    // Save line 0
    char line0_backup[BYTES_PER_ELEMENT * COLUMNS_IN_LINE];
    for (unsigned int j = 0; j < BYTES_PER_ELEMENT * COLUMNS_IN_LINE; j++) {
        line0_backup[j] = vidptr[j];
    }
    
    // Clear entire screen
    for (unsigned int i = 0; i < SCREENSIZE; i += 2) {
        vidptr[i] = ' ';
        vidptr[i + 1] = 0x07;
    }
    
    // Restore line 0
    for (unsigned int j = 0; j < BYTES_PER_ELEMENT * COLUMNS_IN_LINE; j++) {
        vidptr[j] = line0_backup[j];
    }
    
    // Start printing from line 1
    current_loc = BYTES_PER_ELEMENT * COLUMNS_IN_LINE;
    update_cursor(0, 1);
}

void keyboard_handler_main(void) {
    unsigned char status = read_port(KEYBOARD_STATUS_PORT);
    write_port(0x20, 0x20); // End of interrupt signal

    if (status & 0x01) {
        unsigned char keycode = read_port(KEYBOARD_DATA_PORT);
        
        // Handle arrow keys (extended scancodes start with 0xE0) - MUST CHECK FIRST!
        static int extended = 0;
        if (keycode == 0xE0) {
            extended = 1;
            return;
        }
        
        if (extended) {
            extended = 0;
            
            // Up arrow (0x48)
            if (keycode == 0x48 && history_count > 0) {
                // Clear current line
                while (input_index > 0) {
                    input_index--;
                    current_loc -= 2;
                    vidptr[current_loc] = ' ';
                    vidptr[current_loc + 1] = 0x07;
                }
                
                // Navigate history
                if (!browsing_history) {
                    history_index = history_count - 1;
                    browsing_history = 1;
                } else if (history_index > 0) {
                    history_index--;
                }
                
                // Load command from history
                const char* cmd = command_history[history_index];
                input_index = 0;
                while (cmd[input_index] != '\0') {
                    input_buffer[input_index] = cmd[input_index];
                    vidptr[current_loc++] = cmd[input_index];
                    vidptr[current_loc++] = 0x07;
                    input_index++;
                }
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
                return;
            }
            
            // Down arrow (0x50)
            if (keycode == 0x50 && browsing_history) {
                // Clear current line
                while (input_index > 0) {
                    input_index--;
                    current_loc -= 2;
                    vidptr[current_loc] = ' ';
                    vidptr[current_loc + 1] = 0x07;
                }
                
                // Navigate forward in history
                if (history_index < history_count - 1) {
                    history_index++;
                    
                    // Load command from history
                    const char* cmd = command_history[history_index];
                    input_index = 0;
                    while (cmd[input_index] != '\0') {
                        input_buffer[input_index] = cmd[input_index];
                        vidptr[current_loc++] = cmd[input_index];
                        vidptr[current_loc++] = 0x07;
                        input_index++;
                    }
                } else {
                    // Reached newest, clear input
                    browsing_history = 0;
                    history_index = -1;
                    input_index = 0;
                }
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
                return;
            }
            
            return;
        }
        
        // Track shift key state (left shift = 0x2A, right shift = 0x36)
        if (keycode == 0x2A || keycode == 0x36) {
            shift_pressed = 1;
            return;
        }
        if (keycode == 0xAA || keycode == 0xB6) { // Shift release
            shift_pressed = 0;
            return;
        }
        
        // Track caps lock (0x3A)
        if (keycode == 0x3A) {
            caps_lock = !caps_lock;
            return;
        }
        
        // Ignore key release events (bit 7 set means key released)
        if (keycode & 0x80) {
            return;
        }
        
        if (keycode == ENTER_KEY_CODE) {
            
            kprint_newline();
            input_buffer[input_index] = '\0';
            
            serial_write_string("\n[INPUT] Command entered: ");
            serial_write_string(input_buffer);
            serial_write_string("\n");
            
            // Add non-empty commands to history
            if (input_index > 0 && strcmp(input_buffer, "") != 0) {
                serial_write_string("[HISTORY] Adding command to history\n");
                // Shift history if full
                if (history_count >= HISTORY_SIZE) {
                    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                        for (int j = 0; j < KEYBOARD_INPUT_LENGTH; j++) {
                            command_history[i][j] = command_history[i + 1][j];
                        }
                    }
                    history_count = HISTORY_SIZE - 1;
                }
                
                // Add command to history
                for (int i = 0; i < input_index; i++) {
                    command_history[history_count][i] = input_buffer[i];
                }
                command_history[history_count][input_index] = '\0';
                history_count++;
            }
            
            // Reset history browsing
            history_index = -1;
            browsing_history = 0;
            
            if (current_loc >= SCREENSIZE) {
                clear_screen();
            }
            if (strcmp(input_buffer, "db0f") == 0) {
                kprint("DB0 FAULT");
                int a = 5/0;
            } else if (strcmp(input_buffer, "clear") == 0) {
                callservice("clear", "shell");
            } else if (strcmp(input_buffer, "help") == 0) {
                callservice("help", "shell");
            } else if (strcmp(input_buffer, "read") == 0) {
                callservice("fs-read", "shell");
            } else if (strcmp(input_buffer, "bomb") == 0) {
               kprint("DONT TYPE ANYTHING OS WILL BOMB!!");
               kprint(" THIS ISNT GOOD YOU LAUNCHED A BOMB!");
               serial_write_string("[SECURITY] toastSecure Code was changed from 1 to 0 in test. Next letter input will lock the OS.");
               toast_shell_color(" lebron james is coming 4 u", YELLOW);
               securecode = 0;
            } else if (strcmp(input_buffer, "panic") == 0) {
                callservice("panic-test", "shell");
            } else if (strcmp(input_buffer, "mpanic") == 0) {
                l3_panic("fatal panic");
            } else if (strcmp(input_buffer, "fs-testfile") == 0) {
                local_fs("testfile.txt", "This is a test file created in toastOS's local filesystem.");
            } else if (strcmp(input_buffer, "fs-readfile") == 0) {
                read_local_fs("testfile.txt-1");
            } else if (strcmp(input_buffer, "list") == 0) {
                callservice("fs-list", "shell");
            } else if (strcmp(input_buffer, "fs-list") == 0) {
                callservice("fs-list", "shell");
            } else if (strcmp(input_buffer, "fs-autotest") == 0) {
                callservice("fta", "shell");
            } else if (strcmp(input_buffer, "cursor-enable") == 0) {
                callservice("cursor-on", "shell");
            } else if (strcmp(input_buffer, "cursor-disable") == 0) {
                callservice("cursor-off", "shell");
            } else if (strcmp(input_buffer, "fun printwithcolor") == 0) {
                callservice("color", "shell");
            } else if (strcmp(input_buffer, "shutdown") == 0) {
                callservice("shutdown", "shell");
            } else if (strcmp(input_buffer, "write") == 0) {
                callservice("fs-write", "shell");
            } else if (strcmp(input_buffer, "reboot") == 0) {
                callservice("reboot", "shell");
            } else if (strcmp(input_buffer, "mem-info") == 0) {
                callservice("mem-info", "shell");
            } else if (strcmp(input_buffer, "svc-disable") == 0) {
                callservice("svc-disable", "shell");
            } else if (strcmp(input_buffer, "svc-enable") == 0) {
                callservice("svc-enable", "shell");
            } else if (strcmp(input_buffer, "svc-list") == 0) {
                callservice("svc-list", "shell");
            } else if (strcmp(input_buffer, "securelock") == 0) {
                callservice("securelock", "shell");
            } else if (strcmp(input_buffer, "time") == 0) {
                callservice("time", "shell");
            } else if (strcmp(input_buffer, "date") == 0) {
                callservice("date", "shell");
            } else if (strcmp(input_buffer, "datetime") == 0) {
                callservice("datetime", "shell");
            } else if (strcmp(input_buffer, "timezone") == 0) {
                callservice("timezone", "shell");
            } else if (strcmp(input_buffer, "reg-list") == 0) {
                callservice("registry-list", "shell");
            } else if (strcmp(input_buffer, "reg-get") == 0) {
                callservice("registry-get", "shell");
            } else if (strcmp(input_buffer, "reg-set") == 0) {
                callservice("registry-set", "shell");
            } else if (strcmp(input_buffer, "reg-delete") == 0) {
                callservice("registry-delete", "shell");
            } else if (strcmp(input_buffer, "reg-save") == 0) {
                callservice("registry-save", "shell");
            } else if (strcmp(input_buffer, "call") == 0) {
                callservice("call", "shell");
            } else if (strcmp(input_buffer, "edit") == 0) {
                callservice("edit", "shell");
            } else if (strcmp(input_buffer, "edit-open") == 0) {
                callservice("edit-open", "shell");
            } else if (strcmp(input_buffer, "apps") == 0) {
                app_list();
            } else if (strcmp(input_buffer, "ifconfig") == 0) {
                network_ifconfig();
            } else if (strncmp(input_buffer, "ping ", 5) == 0) {
                network_ping(input_buffer + 5);
            } else if (strcmp(input_buffer, "netstat") == 0) {
                network_netstat();
            } else if (strncmp(input_buffer, "setip ", 6) == 0) {
                network_setip(input_buffer + 6);
            } else if (strncmp(input_buffer, "run ", 4) == 0) {
                // Extract app name after "run "
                char* app_name = input_buffer + 4;
                app_run(app_name);
            } else if (strcmp(input_buffer, "") != 0) {
                // Check if it's an app name they forgot to prefix with "run"
                extern int app_exists(const char* name);
                if (app_exists(input_buffer)) {
                    toast_shell_color("Did you mean: ", YELLOW);
                    toast_shell_color("run ", LIGHT_CYAN);
                    toast_shell_color(input_buffer, LIGHT_GREEN);
                    toast_shell_color("?", YELLOW);
                    kprint_newline();
                } else {
                    kprint("Command not recognized. Type 'help' for available commands!");
                }
            }

            input_index = 0;
            kprint_newline();
            toast_shell_color("toastOS > ", LIGHT_CYAN);
            return;
        }
        
        // Handle arrow keys (extended scancodes start with 0xE0)
        if (keycode == 0xE0) {
            extended = 1;
            return;
        }
        
        if (extended) {
            extended = 0;
            
            // Up arrow (0x48)
            if (keycode == 0x48 && history_count > 0) {
                // Clear current line
                while (input_index > 0) {
                    input_index--;
                    current_loc -= 2;
                    vidptr[current_loc] = ' ';
                    vidptr[current_loc + 1] = 0x07;
                }
                
                // Navigate history
                if (!browsing_history) {
                    history_index = history_count - 1;
                    browsing_history = 1;
                } else if (history_index > 0) {
                    history_index--;
                }
                
                // Load command from history
                const char* cmd = command_history[history_index];
                input_index = 0;
                while (cmd[input_index] != '\0') {
                    input_buffer[input_index] = cmd[input_index];
                    vidptr[current_loc++] = cmd[input_index];
                    vidptr[current_loc++] = 0x07;
                    input_index++;
                }
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
                return;
            }
            
            // Down arrow (0x50)
            if (keycode == 0x50 && browsing_history) {
                // Clear current line
                while (input_index > 0) {
                    input_index--;
                    current_loc -= 2;
                    vidptr[current_loc] = ' ';
                    vidptr[current_loc + 1] = 0x07;
                }
                
                // Navigate forward in history
                if (history_index < history_count - 1) {
                    history_index++;
                    
                    // Load command from history
                    const char* cmd = command_history[history_index];
                    input_index = 0;
                    while (cmd[input_index] != '\0') {
                        input_buffer[input_index] = cmd[input_index];
                        vidptr[current_loc++] = cmd[input_index];
                        vidptr[current_loc++] = 0x07;
                        input_index++;
                    }
                } else {
                    // Reached newest, clear input
                    browsing_history = 0;
                    history_index = -1;
                    input_index = 0;
                }
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
                return;
            }
            
            return;
        }

        char c = get_char_from_keycode(keycode);
        if (c == '\b' && input_index > 0) {
            // Handle backspace: remove the last character from the buffer and screen
            input_index--;
            current_loc -= 2; // Move cursor back
            vidptr[current_loc] = ' '; // Clear the character on screen
            vidptr[current_loc + 1] = 0x07; // Reset attribute byte
            update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
            browsing_history = 0; // Exit history mode when typing
        } else if (c && input_index < KEYBOARD_INPUT_LENGTH - 1) {
            input_buffer[input_index++] = c;
            vidptr[current_loc++] = c;
            vidptr[current_loc++] = 0x07;
            update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
            browsing_history = 0; // Exit history mode when typing
        }
        // you can turn off securecodes by not typing bomb but you cant idk what i'm rambling abyt
        // check for a scancode, just type bomb! it will set securecode to 0, causing the os to panic halt
        check_securecode();
    }
}

char* rec_input(void) {
    static char temp_buffer[KEYBOARD_INPUT_LENGTH];
    int temp_index = 0;
    
    // Clear the buffer
    for (int i = 0; i < KEYBOARD_INPUT_LENGTH; i++) {
        temp_buffer[i] = '\0';
    }
    
    // Disable keyboard interrupts to avoid conflicts
    write_port(0x21, 0xFF);
    
    // Flush keyboard buffer to clear any stale data
    while (read_port(KEYBOARD_STATUS_PORT) & 0x01) {
        read_port(KEYBOARD_DATA_PORT);
    }
    
    while(1) {
        unsigned char status = read_port(KEYBOARD_STATUS_PORT);
        if (status & 0x01) {
            unsigned char keycode = read_port(KEYBOARD_DATA_PORT);
            
            // Send EOI to PIC
            write_port(0x20, 0x20);
            
            // Track shift key state (left shift = 0x2A, right shift = 0x36)
            if (keycode == 0x2A || keycode == 0x36) {
                shift_pressed = 1;
                continue;
            }
            if (keycode == 0xAA || keycode == 0xB6) { // Shift release
                shift_pressed = 0;
                continue;
            }
            
            // Track caps lock (0x3A)
            if (keycode == 0x3A) {
                caps_lock = !caps_lock;
                continue;
            }
            
            // Ignore key release events (bit 7 set means key released)
            if (keycode & 0x80) {
                continue;
            }
            
            if (keycode == ENTER_KEY_CODE) {
                temp_buffer[temp_index] = '\0';
                kprint_newline();
                
                // Re-enable keyboard interrupts
                write_port(0x21, 0xFD);
                
                return temp_buffer;
            }

            char c = get_char_from_keycode(keycode);
            if (c == '\b' && temp_index > 0) {
                temp_index--;
                current_loc -= 2;
                vidptr[current_loc] = ' ';
                vidptr[current_loc + 1] = 0x07;
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
            } else if (c && temp_index < KEYBOARD_INPUT_LENGTH - 1) {
                temp_buffer[temp_index++] = c;
                vidptr[current_loc++] = c;
                vidptr[current_loc++] = 0x07;
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
            }
        }
    }
}

void init_shell(void) {
    serial_write_string("\n");
    serial_write_string("========================================\n");
    serial_write_string("        toastOS Boot Sequence\n");
    serial_write_string("========================================\n");
    serial_write_string("[INIT] Serial port initialized\n");
    serial_init();

    serial_write_string("[INIT] Registering services...\n");
    init_services();
    
    serial_write_string("[INIT] Clearing screen...\n");
    clear_screen();
    
    serial_write_string("[INIT] Displaying welcome message\n");
    
    // Display welcome banner (line 0 has clock, we start from line 1)
    kprint_newline();
    toast_shell_color("    Welcome to toastOS v1.1!", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("    Clock shows current time (UTC-5 EST)", YELLOW);
    kprint_newline();
    kprint("    Use 'timezone' command to change");
    kprint_newline();
    kprint_newline();
    
    kprint("Type 'help' for commands");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("toastOS > ", LIGHT_CYAN);
    
    serial_write_string("[INIT] Enabling cursor\n");
    enable_cursor(0, 15);
    
    serial_write_string("[INIT] Setting up Interrupt Descriptor Table (IDT)\n");
    idt_init();
    
    serial_write_string("[INIT] Initializing keyboard controller\n");
    kb_init();
    
    serial_write_string("========================================\n");
    serial_write_string("[INIT] System initialization complete!\n");
    serial_write_string("========================================\n\n");
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

unsigned char keyboard_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',    /* 9 */
    '(', ')', '_', '+', '\b',    /* Backspace */
    '\t',            /* Tab */
    'Q', 'W', 'E', 'R',  /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',    /* Enter */
    0,         /* Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',    /* 39 */
    '"', '~',   0,       /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',           /* 49 */
    'M', '<', '>', '?',   0,             /* Right shift */
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
    '_',
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

// Update clock display in top-right corner
void update_clock_display(void) {
    // Display time and date in top-right corner
    // Time format: "HH:MM:SS" (8 chars)
    // Date format: "MM/DD/YY" (8 chars)
    // Total: 17 chars with space
    
    extern uint8_t rtc_read_seconds(void);
    extern uint8_t rtc_read_minutes(void);
    extern uint8_t rtc_read_hours(void);
    extern uint8_t rtc_read_day(void);
    extern uint8_t rtc_read_month(void);
    extern uint8_t rtc_read_year(void);
    extern int rtc_timezone_offset;
    
    // Read RTC values
    uint8_t sec = rtc_read_seconds();
    uint8_t min = rtc_read_minutes();
    int hour = rtc_read_hours();
    uint8_t day = rtc_read_day();
    uint8_t month = rtc_read_month();
    uint8_t year = rtc_read_year();
    
    // Apply timezone offset
    hour += rtc_timezone_offset;
    
    // Handle day rollover
    if (hour < 0) {
        hour += 24;
    } else if (hour >= 24) {
        hour -= 24;
    }
    
    // Position for time: right side of screen, line 0
    // 80 chars wide, need 17 chars (8 time + 1 space + 8 date)
    int time_pos = (0 * COLUMNS_IN_LINE + (COLUMNS_IN_LINE - 17)) * 2;
    uint8_t color = 0x0E; // Yellow on black (more visible)
    
    // Display time: HH:MM:SS
    vidptr[time_pos + 0] = '0' + (hour / 10);
    vidptr[time_pos + 1] = color;
    vidptr[time_pos + 2] = '0' + (hour % 10);
    vidptr[time_pos + 3] = color;
    vidptr[time_pos + 4] = ':';
    vidptr[time_pos + 5] = color;
    vidptr[time_pos + 6] = '0' + (min / 10);
    vidptr[time_pos + 7] = color;
    vidptr[time_pos + 8] = '0' + (min % 10);
    vidptr[time_pos + 9] = color;
    vidptr[time_pos + 10] = ':';
    vidptr[time_pos + 11] = color;
    vidptr[time_pos + 12] = '0' + (sec / 10);
    vidptr[time_pos + 13] = color;
    vidptr[time_pos + 14] = '0' + (sec % 10);
    vidptr[time_pos + 15] = color;
    
    // Space between time and date
    vidptr[time_pos + 16] = ' ';
    vidptr[time_pos + 17] = color;
    
    // Display date: MM/DD/YY
    vidptr[time_pos + 18] = '0' + (month / 10);
    vidptr[time_pos + 19] = color;
    vidptr[time_pos + 20] = '0' + (month % 10);
    vidptr[time_pos + 21] = color;
    vidptr[time_pos + 22] = '/';
    vidptr[time_pos + 23] = color;
    vidptr[time_pos + 24] = '0' + (day / 10);
    vidptr[time_pos + 25] = color;
    vidptr[time_pos + 26] = '0' + (day % 10);
    vidptr[time_pos + 27] = color;
    vidptr[time_pos + 28] = '/';
    vidptr[time_pos + 29] = color;
    vidptr[time_pos + 30] = '0' + (year / 10);
    vidptr[time_pos + 31] = color;
    vidptr[time_pos + 32] = '0' + (year % 10);
    vidptr[time_pos + 33] = color;
}

