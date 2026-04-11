/*
 * toastOS++ Application Layer
 * Namespace: toast::app
 */

#ifndef TAPPLAYER_HPP
#define TAPPLAYER_HPP

#include "../drivers/toast_libc.hpp"
#include "../drivers/kio.hpp"

#define PERM_PRINT       0x01
#define PERM_FS          0x02
#define PERM_PANIC       0x04
#define PERM_TIME        0x08
#define PERM_DEVICE      0x10
#define PERM_ALL         0xFF

struct AppContext {
    char* app_name;
    int app_id;
    int initialized;
    int permissions;
};

#define MAX_APP_SLOTS 6

namespace toast {
namespace app {

int register_app(char* name, int permissions);
void exit(int exit_code);
void kill(int app_id, char* reason);

AppContext* context();
AppContext* context_for_terminal(int term);
int terminal_has_app(int term);

void print(char* string);
void panic(char* reason, int severe);

} // namespace app
} // namespace toast

/* Legacy C-style aliases */
int register_app(char* app_name, int permissions);
void exitapp(int exit_code);
void killapp(int app_id, char* reason);
AppContext* get_app_context();
AppContext* get_app_context_for_terminal(int term);
int terminal_has_app(int term);

#endif /* TAPPLAYER_HPP */
