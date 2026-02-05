#include "kio.h"
#include "funcs.h"
#include "panic.h"
#include "stdint.h"
#include "file.h"
#include "fat16.h"
#include "ata.h"
#include "bootloader.h"

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
static uint8_t screen_bg_color = BLACK;

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

static uint8_t parse_hex_nibble(const char* s);

static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void accept_fs_write() {
    kprint("toastFS Opened. What do you want the FILENAME to be?");
    kprint_newline();
    char* filename = rec_input();
    kprint("What content do you want to write to the file?");

    char* content = rec_input();
    local_fs(filename, content);
    kprint_newline();
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
    kprint("Shutting down...");
    kprint_newline();
    outw(0x604, 0x2000);  // QEMU / Bochs
    outw(0xB004, 0x2000); // Older Bochs
    __asm__ volatile("cli; hlt");
}

void reboot() {
    kprint("Rebooting...");
    kprint_newline();
    
    /* Method 1: Keyboard controller reset */
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);  /* Pulse CPU reset line */
    
    /* Method 2: Triple fault (if keyboard controller fails) */
    __asm__ volatile("cli");
    uint8_t null_idt[6] = {0};
    __asm__ volatile("lidt %0" : : "m"(null_idt));
    __asm__ volatile("int $0x03");  /* Triple fault */
    
    __asm__ volatile("cli; hlt");
}

/* Note: idt_init() has been moved to panic.c - it now handles both 
 * CPU exceptions and keyboard interrupts properly with assembly stubs */

void kb_init(void) {
    // Enable only IRQ1 (keyboard)
    write_port(0x21, 0xFD);
}

void kprint(const char *str) {
    unsigned int i = 0;
    // ensure we start below the top bar
    if (current_loc < 160) current_loc = 160;
    
    while (str[i]) {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
    }
    if (current_loc >= SCREENSIZE) {
        kprint_newline();
    }
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

void kprint_newline(void) {
    unsigned int line_size = BYTES_PER_ELEMENT * COLUMNS_IN_LINE;
    
    // Check if we are at the bottom of the screen
    if (current_loc >= SCREENSIZE - line_size) {
        // Scroll up lines 2-24 to 1-23 (Line 0 is top bar)
        // Memory range for lines 1-24: 160 to SCREENSIZE
        
        // Copy lines 2-24 to 1-23
        // Line 1 starts at 160. Line 2 starts at 320.
        // We want to move (SCREENSIZE - 320) bytes from 320 to 160.
        for (unsigned int i = 160; i < SCREENSIZE - line_size; i++) {
            vidptr[i] = vidptr[i + line_size];
        }
        
        // Clear the last line (Line 24)
        for (unsigned int i = SCREENSIZE - line_size; i < SCREENSIZE; i += 2) {
            vidptr[i] = ' ';
            vidptr[i+1] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
        }
        
        // Set cursor to start of last line
        current_loc = SCREENSIZE - line_size;
    } else {
        // Just move to next line
        current_loc += (line_size - (current_loc % line_size));
    }

    if (current_loc < 160) current_loc = 160; // Safety

    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

void clear_screen(void) {
    // Start from 160 to skip top bar
    for (unsigned int i = 160; i < SCREENSIZE; i += 2) {
        vidptr[i] = ' ';
        vidptr[i + 1] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
    }
    current_loc = 160;
    update_cursor(0, 1);
}

void clear_screen_color(uint8_t bg_color) {
    screen_bg_color = (uint8_t)(bg_color & 0x0F);
    // Start from 160 to skip top bar
    for (unsigned int i = 160; i < SCREENSIZE; i += 2) {
        vidptr[i] = ' ';
        vidptr[i + 1] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
    }
    current_loc = 160;
    update_cursor(0, 1);
}

void keyboard_handler_main(void) {
    unsigned char status = read_port(KEYBOARD_STATUS_PORT);
    write_port(0x20, 0x20); // End of interrupt signal

    if (status & 0x01) {
        char keycode = read_port(KEYBOARD_DATA_PORT);
        static uint8_t got_e0 = 0;

        // Ignore key release events (bit 7 set = key released)
        if (keycode & 0x80) {
            // Key release - just clear e0 flag if needed and return
            got_e0 = 0;
            return;
        }

        if ((uint8_t)keycode == 0xE0) {
            got_e0 = 1;
            return;
        }

        if (got_e0) {
            got_e0 = 0;
            if ((uint8_t)keycode == 0x53) {
                l3_panic("Delete initiated crash");
                return;
            }
            return; // Ignore other extended keys for now
        }
        
        if (keycode == ENTER_KEY_CODE) {
            kprint_newline();
            input_buffer[input_index] = '\0';
            if (current_loc >= SCREENSIZE) {
                clear_screen();
            }
            
            /* ===== SYSTEM COMMANDS ===== */
            if (strcmp(input_buffer, "help") == 0) {
                kprint("toastOS v1.0 - Command Reference");
                kprint_newline();
                kprint_newline();
                kprint("  System:    help, clear, info, shutdown, reboot");
                kprint_newline();
                kprint("  Display:   cursor-on, cursor-off, bg <color>");
                kprint_newline();
                kprint("  Disk:      disk operations (opens disk terminal)");
                kprint_newline();
                kprint("  Debug:     panic, mpanic, test-div0");
                kprint_newline();
            }
            else if (strcmp(input_buffer, "clear") == 0) {
                clear_screen();
            }
            else if (strcmp(input_buffer, "info") == 0) {
                kprint("toastOS v1.1 by thetoasta (2025)");
                kprint_newline();
                kprint("github.com/thetoasta/toastOS");
            }
            else if (strcmp(input_buffer, "shutdown") == 0) {
                shutdown();
            }
            else if (strcmp(input_buffer, "reboot") == 0) {
                reboot();
            }
            
            /* ===== DISPLAY COMMANDS ===== */
            else if (strcmp(input_buffer, "cursor-on") == 0) {
                enable_cursor(0, 15);
                kprint("Cursor enabled.");
            }
            else if (strcmp(input_buffer, "cursor-off") == 0) {
                disable_cursor();
                kprint("Cursor disabled.");
            }
            else if (strcmp(input_buffer, "bg") == 0) {
                kprint("Color (0-F): ");
                char* s = rec_input();
                uint8_t bg = parse_hex_nibble(s);
                clear_screen_color(bg);
            }
            
            /* ===== DISK OPERATIONS ===== */
            else if (strcmp(input_buffer, "disk operations") == 0 || strcmp(input_buffer, "disk") == 0) {
                disk_operations_terminal();
                input_index = 0;
                return;
            }
            
            /* ===== FAT16 DISK COMMANDS (legacy - kept for quick access) ===== */
            else if (strcmp(input_buffer, "disk init") == 0) {
                fat16_init();
            }
            else if (strcmp(input_buffer, "disk format") == 0) {
                kprint("WARNING: Erase all data? (yes/no): ");
                char* confirm = rec_input();
                if (strcmp(confirm, "yes") == 0) {
                    fat16_format();
                } else {
                    kprint("Cancelled.");
                }
            }
            else if (strcmp(input_buffer, "disk list") == 0 || strcmp(input_buffer, "ls") == 0) {
                fat16_list_files();
            }
            else if (strcmp(input_buffer, "disk write") == 0) {
                kprint("Filename: ");
                char* fname = rec_input();
                kprint("Content: ");
                char* content = rec_input();
                fat16_create_file(fname, content);
            }
            else if (strcmp(input_buffer, "disk read") == 0 || strcmp(input_buffer, "cat") == 0) {
                kprint("Filename: ");
                char* fname = rec_input();
                static char read_buf[1024];
                int bytes = fat16_read_file(fname, read_buf, 1024);
                if (bytes >= 0) {
                    kprint(read_buf);
                } else {
                    kprint("File not found.");
                }
            }
            else if (strcmp(input_buffer, "disk del") == 0 || strcmp(input_buffer, "rm") == 0) {
                kprint("Filename: ");
                char* fname = rec_input();
                fat16_delete_file(fname);
            }
            
            /* ===== DEBUG COMMANDS ===== */
            else if (strcmp(input_buffer, "panic") == 0) {
                l1_panic("Manual panic triggered.");
            }
            else if (strcmp(input_buffer, "mpanic") == 0) {
                l3_panic("MANUAL_FATAL_PANIC");
            }
            else if (strcmp(input_buffer, "test-div0") == 0) {
                volatile int x = 1 / 0;
                (void)x;
            }
            
            /* ===== LEGACY COMMANDS (for compatibility) ===== */
            else if (strcmp(input_buffer, "hai") == 0) {
                kprint("hai :3");
            }
            else if (strcmp(input_buffer, "system-quickinfo") == 0) {
                kprint("toastOS v1.0 by thetoasta (2025)");
            }
            else if (strcmp(input_buffer, "fat16-init") == 0) { fat16_init(); }
            else if (strcmp(input_buffer, "fat16-list") == 0) { fat16_list_files(); }
            
            /* ===== UNKNOWN COMMAND ===== */
            else if (strcmp(input_buffer, "") != 0) {
                kprint("Unknown command. Type 'help' for commands.");
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
            vidptr[current_loc + 1] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
            update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
        } else if (c && input_index < KEYBOARD_INPUT_LENGTH - 1) {
            input_buffer[input_index++] = c;
            vidptr[current_loc++] = c;
            vidptr[current_loc++] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
            update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
        }
    }
}

char* rec_input(void) {
    static char temp_buffer[KEYBOARD_INPUT_LENGTH];
    int temp_index = 0;
    
    while(1) {
        unsigned char status = read_port(KEYBOARD_STATUS_PORT);
        if (status & 0x01) {
            char keycode = read_port(KEYBOARD_DATA_PORT);
            
            // Ignore key release events (bit 7 set)
            if (keycode & 0x80) {
                continue;
            }
            
            // Ignore extended keys (E0 prefix)
            if ((unsigned char)keycode == 0xE0) {
                continue;
            }
            
            if (keycode == ENTER_KEY_CODE) {
                temp_buffer[temp_index] = '\0';
                kprint_newline();
                return temp_buffer;
            }

            char c = keyboard_map[(unsigned char)keycode];
            if (c == '\b' && temp_index > 0) {
                temp_index--;
                current_loc -= 2;
                vidptr[current_loc] = ' ';
                vidptr[current_loc + 1] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
            } else if (c && temp_index < KEYBOARD_INPUT_LENGTH - 1) {
                temp_buffer[temp_index++] = c;
                vidptr[current_loc++] = c;
                vidptr[current_loc++] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
                update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
            }
        }
    }
}

void init_shell(void) {
    clear_screen();
    kprint("toastOS System :O");
    kprint_newline();
    kprint("toastOS > ");
    enable_cursor(0, 15);
    kb_init();
}

void panic_init(void) {
    kprint("toastOS System :O");
    kprint_newline();
    kprint("toastOS > ");
    enable_cursor(0, 15);
    kb_init();
}

/* ===== DISK OPERATIONS TERMINAL ===== */
void disk_operations_terminal(void) {
    clear_screen_color(BLUE);
    kprint_newline();
    toast_shell_color("  ========================================", WHITE);
    kprint_newline();
    toast_shell_color("       toastOS Disk Operations Utility    ", WHITE);
    kprint_newline();
    toast_shell_color("  ========================================", WHITE);
    kprint_newline();
    kprint_newline();
    kprint("  Commands:");
    kprint_newline();
    kprint("    write   - Write a file to disk");
    kprint_newline();
    kprint("    read    - Read a file from disk");
    kprint_newline();
    kprint("    format  - Format the disk (FAT16)");
    kprint_newline();
    kprint("    erase   - Erase all files on disk");
    kprint_newline();
    kprint("    init    - Initialize disk driver");
    kprint_newline();
    kprint("    list    - List all files");
    kprint_newline();
    kprint("    del     - Delete a file");
    kprint_newline();
    kprint("    install - Install toastOS to disk");
    kprint_newline();
    kprint("    exit    - Return to toastOS shell");
    kprint_newline();
    kprint_newline();
    
    while (1) {
        toast_shell_color("disk> ", LIGHT_CYAN);
        char* cmd = rec_input();
        
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            clear_screen_color(BLACK);
            kprint("toastOS System :O");
            kprint_newline();
            kprint("toastOS > ");
            return;
        }
        else if (strcmp(cmd, "write") == 0) {
            kprint("Filename: ");
            char* fname = rec_input();
            kprint("Content: ");
            char* content = rec_input();
            fat16_create_file(fname, content);
        }
        else if (strcmp(cmd, "read") == 0) {
            kprint("Filename: ");
            char* fname = rec_input();
            static char read_buf[1024];
            int bytes = fat16_read_file(fname, read_buf, 1024);
            if (bytes >= 0) {
                kprint("Contents: ");
                kprint(read_buf);
                kprint_newline();
            } else {
                toast_shell_color("Error: File not found.", LIGHT_RED);
                kprint_newline();
            }
        }
        else if (strcmp(cmd, "format") == 0) {
            toast_shell_color("WARNING: This will erase ALL data!", YELLOW);
            kprint_newline();
            kprint("Type 'yes' to confirm: ");
            char* confirm = rec_input();
            if (strcmp(confirm, "yes") == 0) {
                fat16_format();
                toast_shell_color("Disk formatted successfully.", LIGHT_GREEN);
                kprint_newline();
            } else {
                kprint("Format cancelled.");
                kprint_newline();
            }
        }
        else if (strcmp(cmd, "erase") == 0) {
            toast_shell_color("WARNING: This will delete ALL files!", YELLOW);
            kprint_newline();
            kprint("Type 'yes' to confirm: ");
            char* confirm = rec_input();
            if (strcmp(confirm, "yes") == 0) {
                fat16_format();
                toast_shell_color("All files erased.", LIGHT_GREEN);
                kprint_newline();
            } else {
                kprint("Erase cancelled.");
                kprint_newline();
            }
        }
        else if (strcmp(cmd, "init") == 0) {
            fat16_init();
            toast_shell_color("Disk initialized.", LIGHT_GREEN);
            kprint_newline();
        }
        else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
            fat16_list_files();
        }
        else if (strcmp(cmd, "del") == 0 || strcmp(cmd, "delete") == 0) {
            kprint("Filename to delete: ");
            char* fname = rec_input();
            fat16_delete_file(fname);
        }
        else if (strcmp(cmd, "install") == 0) {
            toastos_install();
        }
        else if (strcmp(cmd, "help") == 0) {
            kprint("Commands: write, read, format, erase, init, list, del, install, exit");
            kprint_newline();
        }
        else if (strcmp(cmd, "") != 0) {
            toast_shell_color("Unknown command. Type 'help' for commands.", LIGHT_RED);
            kprint_newline();
        }
    }
}

/* ===== REAL toastOS INSTALLATION ===== */

/* Print a number (simple decimal) */
void print_num(uint32_t n) {
    char buf[16];
    int i = 0;
    if (n == 0) {
        kprint("0");
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    /* Reverse and print */
    while (i > 0) {
        char c[2] = { buf[--i], 0 };
        kprint(c);
    }
}

/* The actual installation function */
void toastos_install(void) {
    static uint8_t sector_buffer[512];
    
    clear_screen();
    toast_shell_color("==============================================", LIGHT_CYAN);
    kprint_newline();
    toast_shell_color("       toastOS Installation Wizard            ", LIGHT_CYAN);
    kprint_newline();
    toast_shell_color("==============================================", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    kprint("This will install toastOS to the hard disk.");
    kprint_newline();
    kprint("Installation steps:");
    kprint_newline();
    kprint("  1. Erase all disk data (sectors 0-4095)");
    kprint_newline();
    kprint("  2. Write MBR (Master Boot Record)");
    kprint_newline();
    kprint("  3. Write Stage 2 Bootloader");
    kprint_newline();
    kprint("  4. Write Boot Information");
    kprint_newline();
    kprint("  5. Copy Kernel Binary");
    kprint_newline();
    kprint("  6. Create FAT16 Filesystem");
    kprint_newline();
    kprint("  7. Create initial files");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("WARNING: ALL DISK DATA WILL BE PERMANENTLY ERASED!", LIGHT_RED);
    kprint_newline();
    toast_shell_color("This action cannot be undone!", LIGHT_RED);
    kprint_newline();
    kprint_newline();
    

    return;

    kprint("Type 'install toastos' to continue: ");
    char* confirm = rec_input();
    
    if (strcmp(confirm, "install toastos") != 0) {
        kprint_newline();
        toast_shell_color("Installation cancelled.", YELLOW);
        kprint_newline();
        return;
    }
    
    kprint_newline();
    kprint_newline();
    
    /* Step 1: Initialize ATA and erase disk */
    toast_shell_color("[1/7] Initializing disk...", LIGHT_CYAN);
    kprint_newline();
    if (ata_init() < 0) {
        toast_shell_color("ERROR: Failed to initialize ATA disk!", LIGHT_RED);
        kprint_newline();
        return;
    }
    kprint("      Disk initialized successfully.");
    kprint_newline();
    
    kprint("      Erasing disk (0-4095 sectors = 2MB)...");
    kprint_newline();
    
    /* Erase in chunks with progress indicator */
    for (uint32_t chunk = 0; chunk < 16; chunk++) {
        kprint("      ");
        if (ata_erase_sectors(chunk * 256, 256) < 0) {
            kprint_newline();
            toast_shell_color("ERROR: Failed to erase disk!", LIGHT_RED);
            kprint_newline();
            return;
        }
        /* Progress bar */
        for (uint32_t i = 0; i <= chunk; i++) kprint("#");
        kprint_newline();
    }
    
    toast_shell_color("      Complete! Disk erased.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Step 2: Write MBR */
    toast_shell_color("[2/7] Writing MBR (Master Boot Record)...", LIGHT_CYAN);
    kprint_newline();
    create_mbr_bootloader(sector_buffer);
    if (ata_write_sectors(MBR_SECTOR, 1, sector_buffer) < 0) {
        toast_shell_color("ERROR: Failed to write MBR!", LIGHT_RED);
        kprint_newline();
        return;
    }
    toast_shell_color("      MBR written successfully.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Step 3: Write Stage 2 Bootloader */
    toast_shell_color("[3/7] Writing Stage 2 Bootloader...", LIGHT_CYAN);
    kprint_newline();
    create_stage2_bootloader(sector_buffer);
    if (ata_write_sectors(STAGE2_SECTOR, 1, sector_buffer) < 0) {
        toast_shell_color("ERROR: Failed to write Stage 2!", LIGHT_RED);
        kprint_newline();
        return;
    }
    toast_shell_color("      Stage 2 bootloader written.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Step 4: Write Boot Info */
    toast_shell_color("[4/7] Writing boot information...", LIGHT_CYAN);
    kprint_newline();
    
    /* Clear sector buffer */
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    
    /* Get kernel from memory at 0x100000 */
    uint8_t* kernel_mem = (uint8_t*)KERNEL_LOAD_ADDR;
    
    /* Calculate kernel size - we'll use a reasonable estimate of 128KB max */
    uint32_t kernel_size = 128 * 1024;  /* 128KB */
    uint32_t kernel_sectors = (kernel_size + 511) / 512;
    
    /* Create boot info structure */
    boot_info_t* boot_info = (boot_info_t*)sector_buffer;
    boot_info->magic = 0x544F4153;  /* "TOAS" */
    boot_info->kernel_sectors = kernel_sectors;
    boot_info->kernel_size = kernel_size;
    boot_info->kernel_entry = KERNEL_LOAD_ADDR;
    boot_info->checksum = calculate_checksum(kernel_mem, kernel_size);
    
    /* Version string */
    const char* version = "toastOS v1.1";
    for (int i = 0; version[i] && i < 31; i++) {
        boot_info->version[i] = version[i];
    }
    
    if (ata_write_sectors(BOOT_INFO_SECTOR, 1, sector_buffer) < 0) {
        toast_shell_color("ERROR: Failed to write boot info!", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    kprint("      Boot info: ");
    print_num(kernel_sectors);
    kprint(" sectors (");
    print_num(kernel_size / 1024);
    kprint("KB)");
    kprint_newline();
    toast_shell_color("      Boot information written.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Step 5: Copy Kernel */
    toast_shell_color("[5/7] Copying kernel to disk...", LIGHT_CYAN);
    kprint_newline();
    kprint("      This may take a moment...");
    kprint_newline();
    kprint("      Progress: ");
    
    uint32_t progress_interval = kernel_sectors / 20;  /* 20 progress marks */
    if (progress_interval == 0) progress_interval = 1;
    
    for (uint32_t i = 0; i < kernel_sectors; i++) {
        /* Write one sector at a time */
        if (ata_write_sectors(KERNEL_START_SECT + i, 1, kernel_mem + (i * 512)) < 0) {
            kprint_newline();
            toast_shell_color("ERROR: Failed to write kernel sector ", LIGHT_RED);
            print_num(i);
            kprint("!");
            kprint_newline();
            return;
        }
        
        /* Show progress */
        if ((i % progress_interval) == 0) {
            kprint(".");
        }
    }
    
    kprint_newline();
    toast_shell_color("      Kernel copied successfully!", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Step 6: Create Filesystem */
    toast_shell_color("[6/7] Creating FAT16 filesystem...", LIGHT_CYAN);
    kprint_newline();
    if (fat16_format() < 0) {
        toast_shell_color("ERROR: Failed to format filesystem!", LIGHT_RED);
        kprint_newline();
        return;
    }
    toast_shell_color("      Filesystem created.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Step 7: Create initial files */
    toast_shell_color("[7/7] Creating initial files...", LIGHT_CYAN);
    kprint_newline();
    
    fat16_create_file("README.TXT", "Welcome to toastOS!\n\nYou are running toastOS from your hard disk.\n\nThis operating system was installed and is now\nbooting independently without an ISO file.\n\nEnjoy toastOS!\n");
    fat16_create_file("VERSION.TXT", "toastOS v1.1\n");
    
    toast_shell_color("      Initial files created.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    /* Success! */
    toast_shell_color("==============================================", LIGHT_GREEN);
    kprint_newline();
    toast_shell_color("        INSTALLATION COMPLETE!                ", LIGHT_GREEN);
    kprint_newline();
    toast_shell_color("==============================================", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    kprint("toastOS has been successfully installed to disk!");
    kprint_newline();
    kprint_newline();
    
    kprint("Next steps:");
    kprint_newline();
    kprint("  1. Remove the ISO from your VM settings");
    kprint_newline();
    kprint("  2. Restart the system");
    kprint_newline();
    kprint("  3. toastOS will boot directly from the hard disk");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("Type 'reboot' to restart now.", YELLOW);
    kprint_newline();
}


void toast_shell_color(const char* str, uint8_t color) {
    unsigned int i = 0;
    while (str[i]) {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = (uint8_t)((screen_bg_color << 4) | (color & 0x0F));
    }
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

static uint8_t parse_hex_nibble(const char* s) {
    char c = s && s[0] ? s[0] : '0';
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(10 + (c - 'a'));
    if (c >= 'A' && c <= 'F') return (uint8_t)(10 + (c - 'A'));
    return 0;
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
