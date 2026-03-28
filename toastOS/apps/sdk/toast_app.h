/*
 * toastOS App SDK - toast_app.h
 *
 * Include this header in your app source file. It provides:
 *   - The TAPP_DEFINE() macro to embed app identity and permissions
 *   - Wrapper macros for all available kernel syscalls
 *   - Permission flag constants
 *
 * USAGE:
 *   1. Include this header.
 *   2. Declare your metadata with TAPP_DEFINE().
 *   3. Implement void app_main(int app_id).
 *   4. Compile with the provided app_link.ld linker script.
 *
 * BUILD EXAMPLE (in your app directory):
 *   export PATH=/usr/local/cross/bin:$PATH
 *   x86_64-elf-gcc -m32 -ffreestanding -nostdlib -I path/to/sdk \
 *       -c myapp.c -o myapp.o
 *   x86_64-elf-gcc -m32 -ffreestanding -nostdlib -I path/to/sdk \
 *       -c path/to/sdk/tapp_start.c -o tapp_start.o
 *   x86_64-elf-ld -m elf_i386 -T path/to/sdk/app_link.ld \
 *       -o myapp.elf myapp.o tapp_start.o
 */

#ifndef TOAST_APP_H
#define TOAST_APP_H

/* ---- Basic types (standalone — no system headers required) ---- */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;
typedef unsigned int       size_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

/* ---- Permission flags (must match exec.h and tapplayer.h) ---- */
#define PERM_PRINT       0x01   /* Terminal output       */
#define PERM_FS          0x02   /* File system access    */
#define PERM_PANIC       0x04   /* Kernel panic / crash  */
#define PERM_TIME        0x08   /* Read system time      */
#define PERM_DEVICE      0x10   /* Direct device / port  */
#define PERM_ALL         0xFF   /* All permissions       */

/* ---- Syscall table (must stay in sync with exec.h) ---- */
typedef struct {
    void  (*print)(const char *str);
    char *(*rec_input)(void);
    void  (*exitapp)(int code);
    void  (*app_panic)(const char *reason, int severe);
    int   (*get_permissions)(void);
} ToastSyscallTable;

/* ---- App metadata (must stay in sync with exec.h) ---- */
#define TAPP_NAME_LEN 48
#define TAPP_DEV_LEN  48
#define TAPP_VER_LEN  16

typedef struct {
    char     magic[4];
    char     name[TAPP_NAME_LEN];
    char     developer[TAPP_DEV_LEN];
    char     version[TAPP_VER_LEN];
    uint32_t permissions;
} __attribute__((packed)) TAppMeta;

/*
 * TAPP_DEFINE(app_name, developer, version, permissions)
 *
 * Place this macro once in your app source (not a header).
 * Example:
 *   TAPP_DEFINE("HelloWorld", "YourName", "1.0", PERM_PRINT);
 */
#define TAPP_DEFINE(appname, devname, ver, perms)                        \
    __attribute__((section(".tapp_meta"), used))                          \
    static const TAppMeta __tapp_metadata = {                            \
        .magic       = {'T', 'A', 'P', 'P'},                            \
        .name        = (appname),                                        \
        .developer   = (devname),                                        \
        .version     = (ver),                                            \
        .permissions = (perms)                                           \
    }

/* ---- Internal syscall table pointer (set by tapp_start.c) ---- */
extern ToastSyscallTable *__toast_syscalls;

/* ---- Syscall wrappers ---- */
#define print(s)              (__toast_syscalls->print(s))
#define rec_input()           (__toast_syscalls->rec_input())
#define exitapp(code)         (__toast_syscalls->exitapp(code))
#define app_panic(reason, s)  (__toast_syscalls->app_panic(reason, s))
#define get_permissions()     (__toast_syscalls->get_permissions())

/* ---- App entry point (you implement this) ---- */
void app_main(int app_id);

#endif /* TOAST_APP_H */
