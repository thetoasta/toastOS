/*
 * toastOS++ Keyboard/Screen I/O
 * Namespace: toast::io
 */

#ifndef KIO_HPP
#define KIO_HPP

#include "stdint.hpp"

/* Screen configuration */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define KEYBOARD_INPUT_LENGTH 256

/* Colors */
#define BLACK 0x0
#define BLUE 0x1
#define TOASTOS_BLUE 0x1
#define GREEN 0x2
#define CYAN 0x3
#define RED 0x4
#define MAGENTA 0x5
#define PURPLE 0x5
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

/* I/O ports */
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08
#define ENTER_KEY_CODE 0x1C

/* External assembly symbols */
extern "C" void keyboard_handler();
extern "C" char read_port(unsigned short port);
extern "C" void write_port(unsigned short port, unsigned char data);
extern "C" void load_idt(unsigned long *idt_ptr);

/* IDT entry structure */
struct IDT_entry {
    unsigned short int offset_lowerbits;
    unsigned short int selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short int offset_higherbits;
};

/* Global variables (defined in kio.cpp) */
extern "C" unsigned char keyboard_map[128];
extern "C" unsigned int current_loc;
extern "C" char *vidptr;

/* Legacy C functions - all use C linkage */
extern "C" {
    void kb_init();
    void kprint(const char* str);
    void kprintln(const char* str);
    void kprint_newline();
    void clear_screen();
    void clear_screen_color(uint8_t bg);
    void keyboard_handler_main();
    void update_cursor(int x, int y);
    void init_shell();
    void panic_init();
    void toast_shell_color(const char* str, uint8_t color);
    char* rec_input();
    void shutdown();
    void reboot();
    void disk_operations_terminal();
    void toastos_install();
    void print_num(uint32_t n);
    int get_current_terminal();
}

/* C++ namespace API - these wrap the C functions */
namespace toast {
namespace io {

inline void init() { kb_init(); }
inline void print(const char* str) { kprint(str); }
inline void println(const char* str) { kprintln(str); }
inline void newline() { kprint_newline(); }
inline void clear() { clear_screen(); }
inline void clear(uint8_t bg_color) { clear_screen_color(bg_color); }
inline void print_color(const char* str, uint8_t color) { toast_shell_color(str, color); }
inline char* input() { return rec_input(); }
inline void cursor(int x, int y) { update_cursor(x, y); }
inline void number(uint32_t n) { print_num(n); }
inline int terminal() { return get_current_terminal(); }

namespace sys {
    inline void halt() { shutdown(); }
    inline void restart() { reboot(); }
}

} // namespace io
} // namespace toast

#endif /* KIO_HPP */
