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

static AppContext app_slots[MAX_APP_SLOTS] = {{0}};
static int next_app_id = 100;

/* Helper: get the slot for the current terminal */
static AppContext* current_slot(void) {
    int t = get_current_terminal();
    if (t < 0 || t >= MAX_APP_SLOTS) t = 0;
    return &app_slots[t];
}

/* Called by the KERNEL only - assigns ID and permissions to the app
   in the current terminal's slot */
int register_app(char* app_name, int permissions) {
    AppContext* ctx = current_slot();
    ctx->app_name = app_name;
    ctx->app_id = next_app_id++;
    ctx->permissions = permissions;
    ctx->initialized = 1;
    return ctx->app_id;
}

AppContext* get_app_context(void) {
    AppContext* ctx = current_slot();
    if (!ctx->initialized) {
        l1_panic("A app has tried to do something protected. toastOS has stopped. (Code: NOT_AUTHORIZED)");
    }
    return ctx;
}

AppContext* get_app_context_for_terminal(int term) {
    if (term < 0 || term >= MAX_APP_SLOTS) return NULL;
    return &app_slots[term];
}

int terminal_has_app(int term) {
    if (term < 0 || term >= MAX_APP_SLOTS) return 0;
    return app_slots[term].initialized;
}

static int has_permission(int perm) {
    AppContext* ctx = get_app_context();
    return (ctx->permissions & perm) != 0;
}

void newline() {
    if (!has_permission(PERM_PRINT)) {
        AppContext* ctx = current_slot();
        kprint("[DENIED] App '");
        kprint(ctx->app_name);
        kprint("' lacks PRINT permission.");
        kprint_newline();
        killapp(ctx->app_id, "PRINT permission denied");
        return;
    }
    kprint_newline();
}

void print(char* string) {
    if (!has_permission(PERM_PRINT)) {
        AppContext* ctx = current_slot();
        kprint("[DENIED] App '");
        kprint(ctx->app_name);
        kprint("' lacks PRINT permission.");
        kprint_newline();
        killapp(ctx->app_id, "PRINT permission denied");
        return;
    }
    kprint(string);
    kprint_newline();
}



void panic(char* reason, int severe) {
    if (!has_permission(PERM_PANIC)) {
        AppContext* ctx = current_slot();
        kprint("[DENIED] App '");
        kprint(ctx->app_name);
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
    AppContext* ctx = current_slot();
    ctx->initialized = 0;
    ctx->permissions = 0;
    ctx->app_name = 0;
    ctx->app_id = 0;
}

void killapp(int app_id, char* reason) {
    /* Search all slots for the matching app_id */
    for (int s = 0; s < MAX_APP_SLOTS; s++) {
        if (app_slots[s].app_id == app_id) {
            kprint("A service has killed the app ");
            kprint(app_slots[s].app_name);
            kprint(". The reason was ");
            kprint(reason);
            kprint(".");
            app_slots[s].initialized = 0;
            app_slots[s].permissions = 0;
            app_slots[s].app_name = 0;
            app_slots[s].app_id = 0;
            return;
        }
    }
}