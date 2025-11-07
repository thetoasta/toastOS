/* custom built, toastOS Panic handler. toastOS Panic */

#include "kio.h"
#include "funcs.h"

void l1_panic(const char *message) {
    clear_screen();
    kprint_newline();
    toast_shell_color("== toastOS Recoverable Crash ==", LIGHT_RED);
    kprint_newline();
    kprint("toastOS has encountered a error, but can recover. Here was the error: ");
    kprint(message);
    kprint_newline();
    panic_init();
    kprint_newline();
    serial_write_string("Panic.");
    serial_write_string(message);
}

void l3_panic(const char *message) {
    clear_screen();
    kprint_newline();
    toast_shell_color("!! toastOS Crash !!", RED);
    kprint_newline();
    kprint("toastOS has encountered a error, and cannot recover. Here was the error: ");
    kprint(message);
    serial_write_string("Halt.");
    serial_write_string(message);
    __asm__ volatile ("hlt");
}