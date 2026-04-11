#include "kio.hpp"
#include "funcs.hpp"
#include "panic.hpp"
#include "file.hpp"
#include "fat16.hpp"
#include "ata.hpp"
#include "bootloader.hpp"
#include "time.hpp"
#include "toast_libc.hpp"
#include "../services/tapplayer.hpp"
#include "registry.hpp"
#include "exec.hpp"
#include "editor.hpp"
#include "tscript.hpp"
#include "security.hpp"
#include "../services/settings.hpp"
#include "mmu.hpp"
#include "user.hpp"
#include "toastcc.hpp"
#include "net.hpp"

extern "C" {

/*

toastOS brain. links everything together, thinks and then somehow works. 

v1.7

*/

/* ===== APP DECLARATIONS ===== */
extern void obama_main(int app_id);
extern void toast_mgr_main(int app_id);

/* ===== APP REGISTRY ===== */
typedef struct {
    char* name;
    void (*entry)(int app_id);
    int permissions;
} AppEntry;

static AppEntry app_list[] = {
    {"obama", obama_main, PERM_ALL},
    {"manager", toast_mgr_main, PERM_PRINT | PERM_PANIC},
};
static int app_count = sizeof(app_list) / sizeof(AppEntry);

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
// Base terminal count, don't change no matter what because this just lets toastOS know the terminal system.
// If changed, terminals might not work.
// MS Version
#define NUM_TERMINALS 7
// * Total allowed terminals.
// Leave on 6. JUST LEAVE ON 6.
static int runtime_num_terminals = 7;
/* Service notice shown flag (runtime) */
static int service_notice_shown = 0;

/* ===== VIRTUAL TERMINAL SYSTEM ===== */
typedef struct {
    char screen_buffer[SCREENSIZE];
    char input_buffer[KEYBOARD_INPUT_LENGTH];
    unsigned int input_index;
    unsigned int cursor_loc;
    uint8_t bg_color;
    uint8_t active;
} VirtualTerminal;

static VirtualTerminal terminals[NUM_TERMINALS];
static int current_terminal = 0;

char input_buffer[KEYBOARD_INPUT_LENGTH];
unsigned int input_index = 0;
static uint8_t shift_held = 0;
static uint8_t alt_held   = 0;
static uint8_t ctrl_held  = 0;

unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;
static uint8_t screen_bg_color = BLACK;

/* ===== COMMAND HISTORY ===== */
#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][KEYBOARD_INPUT_LENGTH];
static int history_count = 0;
static int history_pos   = 0;  /* current browse position, -1 = live input */
static char history_saved[KEYBOARD_INPUT_LENGTH]; /* partial input saved when browsing */

static void history_push(const char *cmd) {
    if (cmd[0] == '\0') return;
    /* Don't store duplicate of the most recent entry */
    if (history_count > 0 && strcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0) return;
    strncpy(history[history_count % HISTORY_SIZE], cmd, KEYBOARD_INPUT_LENGTH - 1);
    history[history_count % HISTORY_SIZE][KEYBOARD_INPUT_LENGTH - 1] = '\0';
    history_count++;
}

/* Erase current input on screen and replace with str */
static void replace_input_line(const char *str) {
    /* Back up to prompt start */
    while (input_index > 0) {
        input_index--;
        current_loc -= 2;
        vidptr[current_loc] = ' ';
        vidptr[current_loc + 1] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
    }
    /* Write new string */
    unsigned int i = 0;
    while (str[i] && i < KEYBOARD_INPUT_LENGTH - 1) {
        input_buffer[i] = str[i];
        vidptr[current_loc++] = str[i];
        vidptr[current_loc++] = (uint8_t)((screen_bg_color << 4) | LIGHT_GREY);
        i++;
    }
    input_buffer[i] = '\0';
    input_index = i;
    update_cursor((current_loc / 2) % COLUMNS_IN_LINE, (current_loc / 2) / COLUMNS_IN_LINE);
}

extern unsigned char keyboard_map[128];
extern unsigned char keyboard_map_shifted[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

static uint8_t parse_hex_nibble(const char* s);
static void save_current_terminal(void);
static void restore_terminal(int term_idx);
static void switch_terminal(int term_idx);

int get_current_terminal(void) {
    return current_terminal;
}

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
    /* private toastOS Security service checking if multiswitch count changed */
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

    __asm__ volatile("cli");

    /* Method 1: Keyboard controller reset (8042 pulse reset line) */
    uint8_t status;
    int timeout = 100000;
    do {
        status = inb(0x64);
        timeout--;
    } while ((status & 0x02) && timeout > 0);
    outb(0x64, 0xFE);  /* Pulse CPU reset line */

    /* Small delay to let the reset take effect */
    for (volatile int i = 0; i < 100000; i++) {}

    /* Method 2: ACPI/chipset reset via port 0xCF9 */
    outb(0xCF9, 0x06);  /* 0x06 = hard reset */

    for (volatile int i = 0; i < 100000; i++) {}

    /* Method 3: 0x0E reset for some chipsets */
    outb(0xCF9, 0x0E);

    for (volatile int i = 0; i < 100000; i++) {}

    /* If all else fails, halt */
    __asm__ volatile("hlt");
    __asm__ volatile("hlt");
}

/* Note: idt_init() has been moved to panic.c - it now handles both 
 * CPU exceptions and keyboard interrupts properly with assembly stubs */

void kb_init(void) {
    // Enable IRQ0 (timer) and IRQ1 (keyboard)
    write_port(0x21, 0xFC);
}

void kprintln(const char *str) {
    kprint(str);
    kprint_newline();
}

void kprint(const char *str) {
    if (!str) return;
    unsigned int i = 0;
    // ensure we start below the top bar
    if (current_loc < 160) current_loc = 160;
    
    while (str[i]) {
        if (str[i] == '\n') {
            i++;
            kprint_newline();
            continue;
        }
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

        // E0 extended-key prefix — must be checked BEFORE the release check
        // because 0xE0 has bit 7 set and would otherwise be misidentified as
        // a key-release event.
        if ((unsigned char)keycode == 0xE0) {
            got_e0 = 1;
            return;
        }

        // Ignore key release events (bit 7 set = key released)
        if ((unsigned char)keycode & 0x80) {
            unsigned char released = keycode & 0x7F;
            if (released == 0x2A || released == 0x36) shift_held = 0;
            if (released == 0x38)                     alt_held   = 0;
            if (released == 0x1D)                     ctrl_held  = 0;
            got_e0 = 0;
            return;
        }

        // Left shift (0x2A) or Right shift (0x36) pressed
        if ((unsigned char)keycode == 0x2A || (unsigned char)keycode == 0x36) {
            shift_held = 1;
            return;
        }

        // Left Alt (0x38) pressed
        if ((unsigned char)keycode == 0x38) {
            alt_held = 1;
            return;
        }

        // Left Ctrl (0x1D) pressed
        if ((unsigned char)keycode == 0x1D) {
            ctrl_held = 1;
            return;
        }

        // Backslash (0x2B) to exit current app
        if ((unsigned char)keycode == 0x2B) {
            exitapp(0);
            kprint_newline();
            kprint("App terminated by user.");
            kprint_newline();
            toast_shell_color("toastOS > ", RED);
            input_index = 0;
            return;
        }

        if (got_e0) {
            got_e0 = 0;

            /* Route extended key to editor if active */
            if (editor_is_active()) {
                char c = shift_held ? keyboard_map_shifted[(unsigned char)keycode]
                                    : keyboard_map[(unsigned char)keycode];
                editor_handle_key((uint8_t)keycode, c, shift_held, ctrl_held, 1);
                if (editor_run_requested()) {
                    goto do_tscript_run;
                }
                if (!editor_is_active()) {
                    clear_screen();
                    kprint_newline();
                    toast_shell_color("toastOS > ", RED);
                    input_index = 0;
                }
                return;
            }

            // Right Alt (AltGr) with E0 prefix = 0x38
            if ((uint8_t)keycode == 0x38) {
                alt_held = 1;
                return;
            }

            /* Up arrow (0x48) — history previous */
            if ((uint8_t)keycode == 0x48) {
                if (history_count > 0) {
                    if (history_pos == -1) {
                        /* Save current partial input */
                        strncpy(history_saved, input_buffer, KEYBOARD_INPUT_LENGTH - 1);
                        history_saved[input_index] = '\0';
                        history_pos = history_count - 1;
                    } else if (history_pos > 0 && history_pos > history_count - HISTORY_SIZE) {
                        history_pos--;
                    }
                    replace_input_line(history[history_pos % HISTORY_SIZE]);
                }
                return;
            }
            /* Down arrow (0x50) — history next */
            if ((uint8_t)keycode == 0x50) {
                if (history_pos >= 0) {
                    history_pos++;
                    if (history_pos >= history_count) {
                        history_pos = -1;
                        replace_input_line(history_saved);
                    } else {
                        replace_input_line(history[history_pos % HISTORY_SIZE]);
                    }
                }
                return;
            }
            return; // Ignore other extended keys for now
        }
        
        // Handle Alt + Number for terminal switching
        if (alt_held) {
            // Number keys: 1=0x02, 2=0x03, 3=0x04, 4=0x05, 5=0x06, 6=0x07
                if (keycode >= 0x02 && keycode <= 0x07) {
                int term_idx = keycode - 0x02;  // 0-5 for terminals 1-6
                if (term_idx < runtime_num_terminals) {
                    switch_terminal(term_idx);
                }
                return;
            }
        }
        
        /* Route regular key to editor if active */
        if (editor_is_active()) {
            char c = shift_held ? keyboard_map_shifted[(unsigned char)keycode]
                                : keyboard_map[(unsigned char)keycode];
            editor_handle_key((uint8_t)keycode, c, shift_held, ctrl_held, 0);
            if (editor_run_requested()) {
                goto do_tscript_run;
            }
            if (!editor_is_active()) {
                clear_screen();
                kprint_newline();
                toast_shell_color("toastOS > ", RED);
                input_index = 0;
            }
            return;
        }

        if (keycode == ENTER_KEY_CODE) {
            kprint_newline();
            input_buffer[input_index] = '\0';
            history_push(input_buffer);
            history_pos = -1;
            if (current_loc >= SCREENSIZE) {
                clear_screen();
            }

            /* ===== SYSTEM COMMANDS ===== */
            if (strcmp(input_buffer, "help") == 0) {
                kprint("toastOS v1.1 - Command Reference");
                kprint_newline();
                kprint_newline();
                kprint("  System:    help, clear, info, shutdown, reboot, settings");
                kprint_newline();
                kprint("  Display:   cursor-on, cursor-off, bg");
                kprint_newline();
                kprint("  Time:      date, uptime, timezone <tz>, timeformat 12|24");
                kprint_newline();
                kprint("  Alarms:    alarm set HH:MM [note], alarm list, alarm clear");
                kprint_newline();
                kprint("  Disk:      disk, ls, cat <file>, rm, disk write, disk rename");
                kprint_newline();
                kprint("  Apps:      apps, run <app>, exec <file.tapp>");
                kprint_newline();
                kprint("  Editor:    edit <file>");
                kprint_newline();
                kprint("  IDE:       toast app engine <file.tsc>");
                kprint_newline();
                kprint("  C Code:    tcc <file.c>");
                kprint_newline();
                kprint("  Network:   ping <ip|host>, ret-contents <ip|host> <path>, localip");
                kprint_newline();
                kprint("  Browser:   browse <url>");
                kprint_newline();
                kprint("  Registry:  reg list, reg get, reg set, reg del");
                kprint_newline();
                kprint("  Misc:      echo <text>, history, whoami");
                kprint_newline();
                kprint("  Setup:     toastsetup reset");
                kprint_newline();
                kprint("  Debug:     panic, mpanic, test-div0");
                kprint_newline();
                kprint("  Shortcuts: Alt+1 to Alt+6 switch terminals");
                kprint_newline();
                kprint("             Up/Down arrows browse command history");
                kprint_newline();
            }
            else if (strcmp(input_buffer, "clear") == 0) {
                clear_screen();
            } else if (strcmp(input_buffer, "settings") == 0) {
                settings();
            }
            else if (strcmp(input_buffer, "info") == 0) {
                kprint("toastOS v1.1 by thetoasta (2025)");
                kprint_newline();
                kprint("github.com/thetoasta/toastOS");
                kprint_newline();
                kprint_newline();

                /* ---- Storage ---- */
                kprint("=== Storage ===");
                kprint_newline();
                disk_info_t dinfo;
                if (ata_get_disk_info(&dinfo) == 0) {
                    kprint("Disk Name : ");
                    kprint(dinfo.model);
                    kprint_newline();
                    kprint("Disk Type : ");
                    kprint(dinfo.type);
                    kprint_newline();
                    kprint("Capacity  : ");
                    print_num(dinfo.size_mb);
                    kprint(" MB");
                    kprint_newline();
                } else {
                    kprint("No ATA disk detected");
                    kprint_newline();
                }

                kprint_newline();

                /* ---- Network ---- */
                kprint("=== Network ===");
                kprint_newline();
                net_info_t ninfo;
                net_get_info(&ninfo);
                if (ninfo.link_up) {
                    kprint("Interface : ");
                    kprint(ninfo.connection_type);
                    kprint_newline();
                    kprint("NIC       : ");
                    kprint(ninfo.nic_name);
                    kprint_newline();
                    kprint("Status    : Connected");
                    kprint_newline();
                } else {
                    kprint("Status    : Not connected");
                    kprint_newline();
                }
            }
            else if (strcmp(input_buffer, "shutdown") == 0) {
                shutdown();
            }
            else if (strcmp(input_buffer, "reboot") == 0) {
                reboot();
            }

            /* ===== NEW UTILITY COMMANDS ===== */
            else if (strncmp(input_buffer, "echo ", 5) == 0) {
                kprint(input_buffer + 5);
            }
            else if (strcmp(input_buffer, "echo") == 0) {
                /* empty echo = blank line */
            }
            else if (strcmp(input_buffer, "date") == 0) {
                time_t t = get_time();
                int adj_h = (int)t.hour + get_timezone();
                if (adj_h < 0) adj_h += 24;
                if (adj_h >= 24) adj_h -= 24;
                /* Print: YYYY-MM-DD HH:MM:SS */
                print_num(t.year);
                kprint("-");
                if (t.month < 10) kprint("0");
                print_num(t.month);
                kprint("-");
                if (t.day < 10) kprint("0");
                print_num(t.day);
                kprint(" ");
                if (adj_h < 10) kprint("0");
                print_num((uint32_t)adj_h);
                kprint(":");
                if (t.minute < 10) kprint("0");
                print_num(t.minute);
                kprint(":");
                if (t.second < 10) kprint("0");
                print_num(t.second);
            }
            else if (strcmp(input_buffer, "uptime") == 0) {
                uint32_t secs = get_uptime_seconds();
                uint32_t mins = secs / 60;
                uint32_t hrs  = mins / 60;
                if (hrs > 0) {
                    print_num(hrs);
                    kprint("h ");
                }
                print_num(mins % 60);
                kprint("m ");
                print_num(secs % 60);
                kprint("s");
            }
            else if (strcmp(input_buffer, "mem") == 0) {
                kprint("heap used: ");
                print_num(mmu_used() / 1024);
                kprint(" KB  free: ");
                print_num(mmu_free() / 1024);
                kprint(" KB");
            }
            else if (strcmp())
            else if (strcmp(input_buffer, "apps") == 0) {
                kprint("Available apps:");
                kprint_newline();
                for (int i = 0; i < app_count; i++) {
                    kprint("  ");
                    kprint(app_list[i].name);
                    kprint_newline();
                }
                kprint_newline();
                kprint("Running apps:");
                kprint_newline();
                int any_running = 0;
                for (int t = 0; t < 6; t++) {
                    AppContext* ac = get_app_context_for_terminal(t);
                    if (ac && ac->initialized) {
                        any_running = 1;
                        kprint("  Terminal ");
                        print_num((uint32_t)(t + 1));
                        kprint(": ");
                        kprint(ac->app_name);
                        kprint_newline();
                    }
                }
                if (!any_running) {
                    kprint("  (none)");
                    kprint_newline();
                }
            }
            else if (strcmp(input_buffer, "history") == 0) {
                int start = history_count > HISTORY_SIZE ? history_count - HISTORY_SIZE : 0;
                for (int i = start; i < history_count; i++) {
                    kprint("  ");
                    kprint(history[i % HISTORY_SIZE]);
                    kprint_newline();
                }
            }

            /* ===== CAT <filename> (inline) ===== */
            else if (strncmp(input_buffer, "cat ", 4) == 0) {
                char* fname = input_buffer + 4;
                static char cat_buf[4096];
                int bytes = fat16_read_file(fname, cat_buf, 4096 - 1);
                if (bytes >= 0) {
                    cat_buf[bytes] = '\0';
                    kprint(cat_buf);
                } else {
                    kprint("File not found: ");
                    kprint(fname);
                }
            }
            else if (strncmp(input_buffer, "timezone ", 9) == 0) {
                char* tz = input_buffer + 9;
                if (strcmp(tz, "EST") == 0) {
                    set_timezone(-5);
                    reg_set("TOASTOS/KERNEL/TIMEZONE", "EST");
                    kprint("Timezone set to EST (UTC-5)");
                } else if (strcmp(tz, "CST") == 0) {
                    set_timezone(-6);
                    reg_set("TOASTOS/KERNEL/TIMEZONE", "CST");
                    kprint("Timezone set to CST (UTC-6)");
                } else if (strcmp(tz, "PST") == 0) {
                    set_timezone(-8);
                    reg_set("TOASTOS/KERNEL/TIMEZONE", "PST");
                    kprint("Timezone set to PST (UTC-8)");
                } else if (strcmp(tz, "MST") == 0) {
                    set_timezone(-7);
                    reg_set("TOASTOS/KERNEL/TIMEZONE", "MST");
                    kprint("Timezone set to MST (UTC-7)");
                } else if (strcmp(tz, "UTC") == 0) {
                    set_timezone(0);
                    reg_set("TOASTOS/KERNEL/TIMEZONE", "UTC");
                    kprint("Timezone set to UTC");
                } else if (strcmp(tz, "GMT") == 0) {
                    set_timezone(0);
                    reg_set("TOASTOS/KERNEL/TIMEZONE", "GMT");
                    kprint("Timezone set to GMT");
                } else if (tz[0] == '+' || tz[0] == '-') {
                    int sign = (tz[0] == '-') ? -1 : 1;
                    int val = 0;
                    int i = 1;
                    while (tz[i] >= '0' && tz[i] <= '9') {
                        val = val * 10 + (tz[i] - '0');
                        i++;
                    }
                    if (val <= 12) {
                        set_timezone(sign * val);
                        reg_set("TOASTOS/KERNEL/TIMEZONE", tz);
                        kprint("Timezone set to UTC");
                        kprint(tz);
                    } else {
                        kprint("Invalid offset. Use -12 to +12.");
                    }
                } else {
                    kprint("Usage: timezone EST|CST|MST|PST|UTC|GMT|+N|-N");
                }
                reg_save();
                update_top_bar();
            }
            
            /* ===== TIMEFORMAT COMMAND ===== */
            else if (strncmp(input_buffer, "timeformat ", 11) == 0) {
                char* fmt = input_buffer + 11;
                if (strcmp(fmt, "12") == 0) {
                    set_time_format(0);
                    reg_set("TOASTOS/KERNEL/TIMEFORMAT", "12");
                    reg_save();
                    kprint("Clock set to 12-hour format.");
                } else if (strcmp(fmt, "24") == 0) {
                    set_time_format(1);
                    reg_set("TOASTOS/KERNEL/TIMEFORMAT", "24");
                    reg_save();
                    kprint("Clock set to 24-hour format.");
                } else {
                    kprint("Usage: timeformat 12|24");
                }
                update_top_bar();
            }

            /* ===== REGISTRY COMMANDS ===== */
            else if (strcmp(input_buffer, "reg list") == 0) {
                kprint("--- Registry ---");
                kprint_newline();
                reg_list();
            }
            else if (strncmp(input_buffer, "reg list ", 9) == 0) {
                char* prefix = input_buffer + 9;
                kprint("--- Registry: ");
                kprint(prefix);
                kprint(" ---");
                kprint_newline();
                reg_list_prefix(prefix);
            }
            else if (strcmp(input_buffer, "toastsetup reset") == 0) {
                kprint("Reset setup progress? (y) >");
                char* opt = rec_input();
                if (strcmp(opt, "y") == 0) {
                    reg_set("TOASTOS/KERNEL/SETUPSTATUS", "0");
                    reg_save();
                    kprint("Setup reset.");
                } else {
                    kprint("Cancelled.");
                }
            }
            else if (strcmp(input_buffer, "lock") == 0) {
                 const char* password = reg_get("TOASTOS/SECURITY/PASSWORD");
    if (password) {
        bool active = true;
        int tries = 0;
        while (active) {
            clear_screen();
            kprint("password, set, please enter it to continue.");
            kprint_newline();
            kprint("enter password to sign into ");
            const char* name = reg_get("TOASTOS/KERNEL/NAME");
            if (name) {
                kprint(name);
            } else {
                kprint("toastOS");
            }
            kprint(" > ");
            char* input = rec_input();
            if (check_password(input)) {
                active = false;
            } else {
                tries++;
                if (tries >= 3) {
                    kprint("sorry, too many attempts. you must reboot manually.");
                    __asm__ volatile ("cli; hlt");
                }
                kprint("that's incorrect. try again.");
            }
        }
    }
            }
            else if (strcmp(input_buffer, "whoami") == 0) {
                const char* uname = reg_get("TOASTOS/KERNEL/NAME");
                kprint(uname ? uname : "(not set - run 'toastsetup reset' to configure)");
            }
            else if (strncmp(input_buffer, "reg set ", 8) == 0) {
                /* reg set KEY VALUE */
                char* args = input_buffer + 8;
                char* space = args;
                while (*space && *space != ' ') space++;
                if (*space == ' ') {
                    *space = '\0';
                    char* val = space + 1;
                    if (reg_set(args, val) == 0) {
                        reg_save();
                        kprint("Set: ");
                        kprint(args);
                        kprint(" = ");
                        kprint(val);
                    } else {
                        kprint("Registry full.");
                    }
                } else {
                    kprint("Usage: reg set <key> <value>");
                }
            }
            else if (strncmp(input_buffer, "reg get ", 8) == 0) {
                char* key = input_buffer + 8;
                const char* val = reg_get(key);
                if (val) {
                    kprint(key);
                    kprint(" = ");
                    kprint(val);
                } else {
                    kprint("Key not found: ");
                    kprint(key);
                }
            }
            else if (strncmp(input_buffer, "reg del ", 8) == 0) {
                char* key = input_buffer + 8;
                if (reg_delete(key) == 0) {
                    reg_save();
                    kprint("Deleted: ");
                    kprint(key);
                } else {
                    kprint("Key not found: ");
                    kprint(key);
                }
            }

            /* ===== FILE / REGISTRY HELPERS ===== */
            else if (strcmp(input_buffer, "file -reg save") == 0) {
                kprint("Saving registry...");
                reg_save();
                kprint("Done.");
                update_top_bar();
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
                static char read_buf[4096];
                int bytes = fat16_read_file(fname, read_buf, 4096 - 1);
                if (bytes >= 0) {
                    read_buf[bytes] = '\0';
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
            else if (strcmp(input_buffer, "disk rename") == 0) {
                kprint("Current filename: ");
                char* old_name = rec_input();
                kprint("New filename: ");
                char* new_name = rec_input();
                static char rename_buf[4096];
                int bytes = fat16_read_file(old_name, rename_buf, 4096 - 1);
                if (bytes >= 0) {
                    rename_buf[bytes] = '\0';
                    fat16_create_file(new_name, rename_buf);
                    fat16_delete_file(old_name);
                    kprint("Renamed.");
                } else {
                    kprint("File not found.");
                }
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
            else if (strcmp(input_buffer, "system-test multiswitch") == 0) {
                kprint("Which terminal? (1-6): ");
                char* term_str = rec_input();
                if (term_str[0] >= '1' && term_str[0] <= '6') {
                    int term_idx = term_str[0] - '1';
                    switch_terminal(term_idx);
                } else {
                    kprint("Invalid terminal number.");
                }
            }
            else if (strcmp(input_buffer, "fat16-init") == 0) { fat16_init(); }
            else if (strcmp(input_buffer, "fat16-list") == 0) { fat16_list_files(); }
            
            /* ===== EDITOR ===== */
            else if (strncmp(input_buffer, "edit ", 5) == 0) {
                char* fname = input_buffer + 5;
                // toastSecurity prompt if the user actually wants to edit protected file (registry!)
                if (strcmp(fname, "toastreg.txt") == 0) {
                    showFilePrompt("toastreg.txt", "this file contains core data for the OS.");
                } else {
                    editor_open(fname);
                    input_index = 0;
                    return;
                }
            }

            /* ===== TOAST C COMPILER ===== */
            else if (strncmp(input_buffer, "tcc ide ", 8) == 0) {
                char* fname = input_buffer + 8;
                if (fname[0] == '\0') {
                    kprint("Usage: tcc ide <file.c>");
                } else {
                    editor_open_ide(fname);
                    input_index = 0;
                    return;
                }
            }
            else if (strncmp(input_buffer, "tcc ", 4) == 0) {
                char* fname = input_buffer + 4;
                if (fname[0] == '\0') {
                    kprint("Usage: tcc <file.c>  or  tcc ide <file.c>");
                } else {
                    tcc_run_file(fname);
                }
            }

            /* ===== TOAST APP ENGINE ===== */
            else if (strncmp(input_buffer, "toast app engine ", 17) == 0) {
                char* fname = input_buffer + 17;
                if (fname[0] == '\0') {
                    kprint("Usage: toast app engine <file.tsc>");
                } else {
                    editor_open_ide(fname);
                    input_index = 0;
                    return;
                }
            }

            /* ===== EXTERNAL APP LOADER ===== */
            else if (strncmp(input_buffer, "exec ", 5) == 0) {
                char* fname = input_buffer + 5;
                int result = exec_run(fname);
                if (result == EXEC_ERR_NOTFOUND) {
                    kprint("[exec] File not found on disk.");
                } else if (result == EXEC_ERR_ELF) {
                    kprint("[exec] Not a valid .tapp ELF binary.");
                } else if (result == EXEC_ERR_SEGSEC) {
                    kprint("[exec] Blocked: binary targets restricted memory.");
                } else if (result == EXEC_ERR_NOMETA) {
                    kprint("[exec] Blocked: missing .tapp_meta section.");
                } else if (result == EXEC_ERR_DENIED) {
                    kprint("[exec] Launch cancelled by user.");
                }
            }
            else if (strncmp(input_buffer, "setpword ", 9) == 0) {
                if (reg_get("TOASTOS/SECURITY/PASSWORD") != NULL) {
                    kprint("Password already set. This command can only be used once.");
                    return;
                }
                char* password = input_buffer + 9;
                set_password(password);
                kprint("password set");
            }
            /* ===== APP LAUNCHER ===== */
            else if (strncmp(input_buffer, "run ", 4) == 0) {
                char* app_name = input_buffer + 4;
                /* Check if this terminal already has an app running */
                if (terminal_has_app(current_terminal)) {
                    kprint("An app is already running in this terminal. Switch to another terminal (Alt+1-6) or exit the current app first.");
                } else {
                    int found = 0;
                    for (int i = 0; i < app_count; i++) {
                        if (strcmp(app_name, app_list[i].name) == 0) {
                            found = 1;
                            kprint("[toastSecurity] toastOS Applications are not currently protected. Please make sure this app is safe. \nType OK to open this app.");
                            kprint_newline();
                            char* openauth = rec_input();
                            if (strcmp(openauth, "OK") != 0) {
                                kprint_newline();
                                kprint("[toastSecurity] Cancelled opening this app.");
                                break;
                            }
                            int id = register_app(app_list[i].name, app_list[i].permissions);
                            app_list[i].entry(id);
                            break;
                        }
                    }
                    if (!found) {
                        kprint("App not found: ");
                        kprint(app_name);
                    }
                }
            }

            /* ===== NETWORK COMMANDS ===== */
            else if (strncmp(input_buffer, "ping ", 5) == 0) {
                char* ip = input_buffer + 5;
                if (ip[0] == '\0') {
                    kprint("Usage: ping <ip or hostname>");
                } else {
                    if (net_init() == 0) {
                        net_ping(ip);
                    }
                }
            }
            else if (strncmp(input_buffer, "ret-contents ", 13) == 0) {
                /* ret-contents <ip|host> <path>  OR  ret-contents <ip|host> (defaults to /) */
                char* args = input_buffer + 13;
                if (args[0] == '\0') {
                    kprint("Usage: ret-contents <ip or hostname> [path]");
                } else {
                    char* space = args;
                    while (*space && *space != ' ') space++;
                    const char* path = "/";
                    if (*space == ' ') {
                        *space = '\0';
                        path = space + 1;
                    }
                    if (net_init() == 0) {
                        net_http_get(args, path);
                    }
                }
            }
            else if (strcmp(input_buffer, "localip") == 0) {
                if (net_init() == 0) {
                    net_print_local_ip();
                }
            }
            else if (strncmp(input_buffer, "browse ", 7) == 0) {
                char* burl = input_buffer + 7;
                if (burl[0] == '\0') {
                    kprint("Usage: browse <url>");
                    kprint_newline();
                } else {
                    if (net_init() == 0) {
                        net_browse(burl);
                    }
                }
            }

            /* ===== ALARM COMMANDS ===== */
            else if (strncmp(input_buffer, "alarm set ", 10) == 0) {
                /* alarm set HH:MM [note] */
                char* args = input_buffer + 10;
                /* parse HH:MM */
                int h = 0, m = 0;
                int pos = 0;
                while (args[pos] >= '0' && args[pos] <= '9') {
                    h = h * 10 + (args[pos] - '0');
                    pos++;
                }
                if (args[pos] == ':') {
                    pos++;
                    while (args[pos] >= '0' && args[pos] <= '9') {
                        m = m * 10 + (args[pos] - '0');
                        pos++;
                    }
                }
                if (h < 0 || h > 23 || m < 0 || m > 59) {
                    kprint("Invalid time. Use HH:MM (24hr).");
                } else {
                    const char *note = (args[pos] == ' ') ? &args[pos + 1] : "";
                    int slot = alarm_set((uint8_t)h, (uint8_t)m, note);
                    if (slot >= 0) {
                        kprint("Alarm set for ");
                        if (h < 10) kprint("0");
                        print_num((uint32_t)h);
                        kprint(":");
                        if (m < 10) kprint("0");
                        print_num((uint32_t)m);
                        if (note[0]) {
                            kprint(" - ");
                            kprint(note);
                        }
                    } else {
                        kprint("No free alarm slots (max 8).");
                    }
                }
            }
            else if (strcmp(input_buffer, "alarm list") == 0) {
                int found = 0;
                for (int ai = 0; ai < MAX_ALARMS; ai++) {
                    const Alarm* a = alarm_get(ai);
                    if (a && a->active) {
                        found = 1;
                        kprint("  [");
                        print_num((uint32_t)ai);
                        kprint("] ");
                        if (a->hour < 10) kprint("0");
                        print_num((uint32_t)a->hour);
                        kprint(":");
                        if (a->minute < 10) kprint("0");
                        print_num((uint32_t)a->minute);
                        if (a->note[0]) {
                            kprint(" - ");
                            kprint(a->note);
                        }
                        kprint_newline();
                    }
                }
                if (!found) {
                    kprint("No alarms set.");
                }
            }
            else if (strncmp(input_buffer, "alarm clear ", 12) == 0) {
                char* idx_str = input_buffer + 12;
                if (strcmp(idx_str, "all") == 0) {
                    alarm_clear_all();
                    kprint("All alarms cleared.");
                } else {
                    int idx = 0;
                    while (*idx_str >= '0' && *idx_str <= '9') {
                        idx = idx * 10 + (*idx_str - '0');
                        idx_str++;
                    }
                    const Alarm* a = alarm_get(idx);
                    if (a && a->active) {
                        alarm_clear(idx);
                        kprint("Alarm cleared.");
                    } else {
                        kprint("No active alarm at that index.");
                    }
                }
            }
            else if (strcmp(input_buffer, "alarm") == 0) {
                kprint("Usage: alarm set HH:MM [note]");
                kprint_newline();
                kprint("       alarm list");
                kprint_newline();
                kprint("       alarm clear <id|all>");
            }

            /* ===== UNKNOWN COMMAND ===== */
            else if (strcmp(input_buffer, "") != 0) {
                kprint("Unknown command. Type 'help' for commands.");
            }

            input_index = 0;
            kprint_newline();
            toast_shell_color("toastOS > ", RED);
            return;
        }

        char c = shift_held ? keyboard_map_shifted[(unsigned char)keycode]
                             : keyboard_map[(unsigned char)keycode];
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
    return;

    /* ---- ToastScript IDE run handler ---- */
do_tscript_run: {
        static char ts_run_buf[EDITOR_MAX_FILESIZE];
        const char *fn = editor_get_filename();
        int r = fat16_read_file(fn, ts_run_buf, EDITOR_MAX_FILESIZE - 1);
        if (r < 0) {
            clear_screen();
            kprint_newline();
            kprint("[run] Could not read file.");
        } else {
            ts_run_buf[r] = '\0';
            /* Detect file type by extension and route accordingly */
            int fnlen = strlen(fn);
            int is_c_file = (fnlen >= 2 &&
                             fn[fnlen-2] == '.' &&
                             (fn[fnlen-1] == 'C' || fn[fnlen-1] == 'c'));
            if (is_c_file) {
                /* .C file → run with toastCC interpreter */
                clear_screen();
                kprint_newline();
                kprint("[toastCC] running ");
                kprint(fn);
                kprint_newline();
                tcc_run_source(ts_run_buf);
            } else {
                /* default: ToastScript */
                tscript_run(ts_run_buf);
            }
        }
        kprint_newline();
        kprint("[press any key to return to editor]");
        /* poll for a keypress */
        while (!(read_port(KEYBOARD_STATUS_PORT) & 0x01))
            __asm__ volatile("hlt");
        (void)read_port(KEYBOARD_DATA_PORT);  /* consume scancode */
        /* reopen editor in IDE mode */
        editor_open_ide(fn);
        return;
    }
}

char* rec_input(void) {
    static char temp_buffer[KEYBOARD_INPUT_LENGTH];
    int temp_index = 0;
    static uint8_t got_e0 = 0;
    
    /* Save current IRQ mask so we restore to it (not unconditionally to 0xFC),
       allowing callers like disk_operations_terminal to keep IRQ1 masked. */
    uint8_t saved_irq_mask = read_port(0x21);

    /* Send EOI for the keyboard IRQ that got us here.
       Mask IRQ1 (keyboard) so the ISR won't steal our polled keystrokes,
       but leave IRQ0 (timer) enabled so the clock keeps ticking. */
    write_port(0x20, 0x20);          /* EOI */
    write_port(0x21, saved_irq_mask | 0x02);   /* mask IRQ1, preserve others */
    __asm__ volatile("sti");

    while(1) {
        __asm__ volatile("hlt");  /* sleep until next interrupt */
        unsigned char status = read_port(KEYBOARD_STATUS_PORT);
        if (status & 0x01) {
            char keycode = read_port(KEYBOARD_DATA_PORT);
            
            // Handle key release events (bit 7 set)
            if ((unsigned char)keycode == 0xE0) {
                got_e0 = 1;
                continue;
            }
            if (keycode & 0x80) {
                unsigned char released = keycode & 0x7F;
                if (released == 0x2A || released == 0x36) {
                    shift_held = 0;
                }
                got_e0 = 0;
                continue;
            }
            
            // Handle E0-prefixed keys

            
            if (keycode == ENTER_KEY_CODE) {
                temp_buffer[temp_index] = '\0';
                kprint_newline();
                write_port(0x21, saved_irq_mask);       /* restore IRQ mask */
                return temp_buffer;
            }

            // Track shift in rec_input too
            if ((unsigned char)keycode == 0x2A || (unsigned char)keycode == 0x36) {
                shift_held = 1;
                continue;
            }

            // Backslash (0x2B) to exit current app
            if ((unsigned char)keycode == 0x2B) {
                exitapp(0);
                kprint_newline();
                kprint("App terminated by user.");
                kprint_newline();
                toast_shell_color("toastOS > ", RED);
                temp_buffer[0] = '\0';
                write_port(0x21, saved_irq_mask);       /* restore IRQ mask */
                return temp_buffer;
            }

            char c = shift_held ? keyboard_map_shifted[(unsigned char)keycode]
                                : keyboard_map[(unsigned char)keycode];
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

static void save_current_terminal(void) {
    VirtualTerminal *term = &terminals[current_terminal];
    // Save screen content (skip top bar at line 0)
    for (unsigned int i = 160; i < SCREENSIZE; i++) {
        term->screen_buffer[i] = vidptr[i];
    }
    // Save input state
    for (unsigned int i = 0; i < KEYBOARD_INPUT_LENGTH; i++) {
        term->input_buffer[i] = input_buffer[i];
    }
    term->input_index = input_index;
    term->cursor_loc = current_loc;
    term->bg_color = screen_bg_color;
    term->active = 1;
}

static void restore_terminal(int term_idx) {
    VirtualTerminal *term = &terminals[term_idx];
    if (term->active) {
        // Restore screen content (skip top bar)
        for (unsigned int i = 160; i < SCREENSIZE; i++) {
            vidptr[i] = term->screen_buffer[i];
        }
        // Restore input state
        for (unsigned int i = 0; i < KEYBOARD_INPUT_LENGTH; i++) {
            input_buffer[i] = term->input_buffer[i];
        }
        input_index = term->input_index;
        current_loc = term->cursor_loc;
        screen_bg_color = term->bg_color;
    } else {
        // Fresh terminal - initialize
        clear_screen();
        const char* uname = reg_get("TOASTOS/KERNEL/NAME");
        if (uname) {
            kprint("Welcome back, ");
            kprint(uname);
        } else {
            kprint("Welcome to toastOS");
        }
        kprint_newline();
        kprint_newline();
        toast_shell_color("toastOS > ", RED);
        term->active = 1;
    }
}

static void switch_terminal(int term_idx) {
    if (term_idx == current_terminal || term_idx >= runtime_num_terminals) {
        return;
    }
    save_current_terminal();
    current_terminal = term_idx;
    restore_terminal(term_idx);
    update_top_bar();
    int x = (current_loc / 2) % COLUMNS_IN_LINE;
    int y = (current_loc / 2) / COLUMNS_IN_LINE;
    update_cursor(x, y);
}

void init_shell(void) {
    // Initialize all terminals as inactive
    for (int i = 0; i < runtime_num_terminals; i++) {
        terminals[i].active = 0;
        terminals[i].input_index = 0;
        terminals[i].cursor_loc = 160;
        terminals[i].bg_color = BLACK;
    }
    current_terminal = 0;
    terminals[0].active = 1;
    
    clear_screen();
    update_top_bar();

    toast_shell_color("   v   _                _    ___  ___ ", LIGHT_CYAN);
    kprint_newline();
    toast_shell_color("   1  | |_ ___  __ _ __| |_ / _ \\/ __|", LIGHT_CYAN);
    kprint_newline();
    toast_shell_color("   .  |  _/ _ \\/ _` (_-<  _| (_) \\__ \\", LIGHT_CYAN);
    kprint_newline();
    toast_shell_color("   1  \\__\\___/\\__,_/__/\\__|\\___/|___/", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();

    const char* uname = reg_get("TOASTOS/KERNEL/NAME");
    if (uname) {
        kprint("Welcome back, ");
        kprint(uname);
    } else {
        kprint("Welcome to toastOS");
    }
    kprint_newline();
    toast_shell_color("toastOS > ", RED);
    enable_cursor(0, 15);
    kb_init();
}

void panic_init(void) {
    update_top_bar();
    const char* uname = reg_get("TOASTOS/KERNEL/NAME");
    if (uname) {
        kprint("Welcome back, ");
        kprint(uname);
    } else {
        kprint("Welcome to toastOS");
    }
    kprint_newline();
    toast_shell_color("toastOS > ", RED);
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
    kprint("    rename  - Rename a file");
    kprint_newline();
    kprint("    install - Install toastOS to disk");
    kprint_newline();
    kprint("    exit    - Return to toastOS shell");
    kprint_newline();
    kprint_newline();
    
    /* Mask IRQ1 (keyboard) so the main ISR cannot fire while we own the keyboard */
    write_port(0x21, read_port(0x21) | 0x02);

    while (1) {
        toast_shell_color("disk> ", LIGHT_CYAN);
        char* cmd = rec_input();
        
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            /* Restore keyboard IRQ before handing back to main shell */
            write_port(0x21, read_port(0x21) & ~0x02);
            clear_screen_color(BLACK);
            const char* uname = reg_get("TOASTOS/KERNEL/NAME");
            if (uname) {
                kprint("Welcome back, ");
                kprint(uname);
            } else {
                kprint("Welcome to toastOS");
            }
            kprint_newline();
            toast_shell_color("toastOS > ", RED);
            break;
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
            static char read_buf[4096];
            int bytes = fat16_read_file(fname, read_buf, 4096 - 1);
            if (bytes >= 0) {
                read_buf[bytes] = '\0';
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
        else if (strcmp(cmd, "rename") == 0) {
            kprint("Current filename: ");
            char* old_name = rec_input();
            kprint("New filename: ");
            char* new_name = rec_input();
            static char ren_buf[4096];
            int bytes = fat16_read_file(old_name, ren_buf, 4096 - 1);
            if (bytes >= 0) {
                ren_buf[bytes] = '\0';
                fat16_create_file(new_name, ren_buf);
                fat16_delete_file(old_name);
                toast_shell_color("Renamed.", LIGHT_GREEN);
                kprint_newline();
            } else {
                toast_shell_color("Error: File not found.", LIGHT_RED);
                kprint_newline();
            }
        }
        else if (strcmp(cmd, "help") == 0) {
            kprint("Commands: write, read, rename, format, erase, init, list, del, install, exit");
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
    if (!str) return;
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

/* the MASTER keyboard map for everywhere */
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
    127, /* Delete */
    0, 0, 0,
    0, /* F11 */
    0, /* F12 */
    0  /* Undefined keys */
};

} // extern "C"
