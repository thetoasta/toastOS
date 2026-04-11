/*
 * toastOS++ Application Layer
 * Namespace: toast::app
 */

#include "../drivers/kio.hpp"
#include "../drivers/funcs.hpp"
#include "../drivers/panic.hpp"
#include "../drivers/fat16.hpp"
#include "../drivers/mmu.hpp"
#include "../drivers/time.hpp"
#include "../drivers/toast_libc.hpp"
#include "tapplayer.hpp"

namespace {
    AppContext app_slots[MAX_APP_SLOTS] = {{0}};
    int next_app_id = 100;
    
    AppContext* current_slot() {
        int t = get_current_terminal();
        if (t < 0 || t >= MAX_APP_SLOTS) t = 0;
        return &app_slots[t];
    }
    
    int has_permission(int perm) {
        AppContext* ctx = toast::app::context();
        return (ctx->permissions & perm) != 0;
    }
}

namespace toast {
namespace app {

int register_app(char* name, int permissions) {
    AppContext* ctx = current_slot();
    ctx->app_name = name;
    ctx->app_id = next_app_id++;
    ctx->permissions = permissions;
    ctx->initialized = 1;
    return ctx->app_id;
}

AppContext* context() {
    AppContext* ctx = current_slot();
    if (!ctx->initialized) {
        toast::sys::panic("A app has tried to do something protected. toastOS has stopped. (Code: NOT_AUTHORIZED)");
    }
    return ctx;
}

AppContext* context_for_terminal(int term) {
    if (term < 0 || term >= MAX_APP_SLOTS) return nullptr;
    return &app_slots[term];
}

int terminal_has_app(int term) {
    if (term < 0 || term >= MAX_APP_SLOTS) return 0;
    return app_slots[term].initialized;
}

void print(char* string) {
    if (!has_permission(PERM_PRINT)) {
        AppContext* ctx = current_slot();
        kprint("[DENIED] App '");
        kprint(ctx->app_name);
        kprint("' lacks PRINT permission.");
        kprint_newline();
        toast::app::kill(ctx->app_id, "PRINT permission denied");
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
            toast::sys::panic(reason);
        }
    } else {
        kprint("Panic denied.");
    }
}

void exit(int exit_code) {
    AppContext* ctx = current_slot();
    ctx->initialized = 0;
    ctx->permissions = 0;
    ctx->app_name = 0;
    ctx->app_id = 0;
}

void kill(int app_id, char* reason) {
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

} // namespace app
} // namespace toast

/* Legacy aliases */
int register_app(char* name, int permissions) { return toast::app::register_app(name, permissions); }
void exitapp(int exit_code) { toast::app::exit(exit_code); }
void killapp(int app_id, char* reason) { toast::app::kill(app_id, reason); }
AppContext* get_app_context() { return toast::app::context(); }
AppContext* get_app_context_for_terminal(int term) { return toast::app::context_for_terminal(term); }
int terminal_has_app(int term) { return toast::app::terminal_has_app(term); }
