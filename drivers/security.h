#include "kio.h"
#include "panic.h"
#include "services.h"

// Security, contains a securecode that if turns to 0, locks and halts os.

 // secure code 
static int securecode = 1;

static inline void security_init() {
    kprint("Seuurity Initialized. toastSecure is active.");
}


// pls work
static inline void check_securecode() {
    if (securecode == 0) {
        securecode = 1;  // Prevent re-triggering
        kprint_newline();
        clear_screen();
        toast_shell_color("toastSecure Code was changed. ", RED);
        callservice("shutdown", "Toast Security Service");
     }
}