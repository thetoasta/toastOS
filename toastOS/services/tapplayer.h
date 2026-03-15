/*

toastOS Application Layer header File

*/

#include "../drivers/toast_libc.h"

#ifndef SERVICES_H
#define SERVICES_H

#define PERM_PRINT       0x01   /* Terminal output       */
#define PERM_FS          0x02   /* File system access    */
#define PERM_PANIC       0x04   /* Kernel panic / crash  */
#define PERM_TIME        0x08   /* Read system time      */
#define PERM_DEVICE      0x10   /* Direct device / port  */
#define PERM_ALL         0xFF   /* All permissions       */

typedef struct {
    char* app_name;
    int app_id;
    int initialized;
    int permissions;
} AppContext;



/* Kernel assigns the app ID and permissions - apps cannot self-assign */
int register_app(char* app_name, int permissions);
void exitapp(int exit_code);
void killapp(int app_id, char* reason);

AppContext* get_app_context(void);

void print(char* string);
void panic(char* reason, int severe);
char* rec_input(void);

#endif