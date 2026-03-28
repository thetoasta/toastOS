/*

slightly mentally inclined user that misspelled their name, or something else?
run this.

*/

#include "../drivers/kio.h"
#include "../drivers/funcs.h"
#include "../drivers/panic.h"
#include "../drivers/fat16.h"
#include "../drivers/time.h"
#include "../drivers/toast_libc.h"
#include "../drivers/registry.h"

const bool ready = true;

void printwelcome() {
    kprint("toastOS settings app");
    kprint_newline();
    kprint("what you want ?");
    kprint_newline();
    kprint("1. new name");
    kprint_newline();
    kprint("2. new timezone");
    kprint_newline();
    kprint("3. timeformat change");
    kprint_newline();
    kprint("4. leave this place");
    kprint_newline();
}

void settings() {
    while (ready) {
        printwelcome();
        kprint("> ");
        const char* doing = rec_input();
        if (strcmp(doing, "1") == 0) {
            kprint("what new name");
            kprint_newline();
            kprint("> ");
            const char* namenew = rec_input();
            reg_set("TOASTOS/KERNEL/NAME", namenew);
            kprint("done!");
            reg_save();
            kprint_newline();
        } else if (strcmp(doing, "2") == 0) {
            kprint("what new timezone");
            kprint_newline();
            kprint("EST, PST, CST, etc.. HAS TO BE CAPITAL! you CAN do something like +5 or -5 if not listed");
            kprint_newline();
            kprint("> ");
            const char* timezonenew = rec_input();
            reg_set("TOASTOS/KERNEL/TIMEZONE", timezonenew);
            kprint("done!");
            reg_save();
            kprint_newline();
        } else if (strcmp(doing, "3") == 0) {
            kprint("what new timeformat? 12/24");
            kprint_newline();
            kprint("> ");
            const char* formatnew = rec_input();
            reg_set("TOASTOS/KERNEL/TIMEFORMAT", formatnew);
            kprint("done!");
            reg_save();
            kprint_newline();
        } else if (strcmp(doing, "4") == 0) {
            break;
        } else {
            kprint("hmm don't know that one. type 4 to leave.");
            kprint_newline();
        }
    }
}