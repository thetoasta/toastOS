#include "kio.h"
#include "panic.h"

// Security, contains a securecode that if turns to 0, locks and halts os.

 // secure code 
int securecode = 1;
// pls work
static inline void check_securecode() {
    if (securecode == 0) {
        toast_shell_color("toastSecure Code was changed. Halting :(", RED);
        while (1) {
            asm volatile("hlt");
        }
     }
}