/* custom built, toastOS Panic handler. toastOS Panic */

#include "kio.h"
#include "funcs.h"

void l1_panic(const char *message) {
    clear_screen();
    kprint("A panic: ");
    kprint(message);
    kprint_newline();
    kprint("System halted.");
    disable_cursor();
    while (1) {
        __asm__ volatile("hlt");
    }
}