/*

toastOS Application Layer

The layer apps use to use file system, panicing, and other things.
Apps CANNOT self-assign IDs or permissions. The kernel calls register_app().

*/

#include "../drivers/kio.h"
#include "../drivers/funcs.h"
#include "../drivers/panic.h"
#include "../drivers/fat16.h"
#include "../drivers/mmu.h"
#include "../drivers/time.h"
#include "../drivers/toast_libc.h"
#include "tapplayer.h"

static AppContext current_app = {0};
static int next_app_id = 100;

/* Called by the KERNEL only - assigns ID and permissions to the app */
int register_app(char* app_name, int permissions) {
    current_app.app_name = app_name;
    current_app.app_id = next_app_id++;
    current_app.permissions = permissions;
    current_app.initialized = 1;
    return current_app.app_id;
}

AppContext* get_app_context(void) {
    if (!current_app.initialized) {
        l1_panic("A app has tried to do something protected. toastOS has stopped. (Code: NOT_AUTHORIZED)");
    }
    return &current_app;
}

static int has_permission(int perm) {
    AppContext* ctx = get_app_context();
    return (ctx->permissions & perm) != 0;
}

void newline() {
    if (!has_permission(PERM_PRINT)) {
        kprint("[DENIED] App '");
        kprint(current_app.app_name);
        kprint("' lacks PRINT permission.");
        kprint_newline();
        killapp(current_app.app_id, "PRINT permission denied");
        return;
    }
    kprint_newline();
}

void print(char* string) {
    if (!has_permission(PERM_PRINT)) {
        kprint("[DENIED] App '");
        kprint(current_app.app_name);
        kprint("' lacks PRINT permission.");
        kprint_newline();
        killapp(current_app.app_id, "PRINT permission denied");
        return;
    }
    kprint(string);
    kprint_newline();
}



void panic(char* reason, int severe) {
    if (!has_permission(PERM_PANIC)) {
        kprint("[DENIED] App '");
        kprint(current_app.app_name);
        kprint("' lacks PANIC permission.");
        kprint_newline();
        return;
    }
    kprint_newline();
    kprint("A app is currently trying to make your toastOS crash. You have to approve this.");
    kprint_newline();
    kprint("Type 'ok' to allow.");
    kprint_newline();
    char* auth = rec_input();
    if (strcmp(auth, "ok") == 0) {
        if (severe == 1) {
            l3_panic(reason);
        } else {
            l1_panic(reason);
        }
    } else {
        kprint("Panic denied.");
    }
}

void exitapp(int exit_code) {
    AppContext* ctx = get_app_context();
    current_app.initialized = 0;
    current_app.permissions = 0;
    current_app.app_name = 0;
    current_app.app_id = 0;
}

void killapp(int app_id, char* reason) {
    if (current_app.app_id == app_id) {
        current_app.initialized = 0;
        current_app.permissions = 0;
        kprint("A service has killed the app ");
        kprint(current_app.app_name);
        kprint(" with the ID of ");
        // kprint(current_app.app_id);
        kprint(". The reason was ");
        kprint(reason);
        kprint(".");
        current_app.app_name = 0;
        current_app.app_id = 0;
    }
}