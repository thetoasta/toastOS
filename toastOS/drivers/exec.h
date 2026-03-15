/*
 * toastOS External Application Loader
 * Public interface for loading and running .tapp ELF32 binaries from FAT16.
 */
#ifndef EXEC_H
#define EXEC_H

#include "stdint.h"

/* --- Permission flags (must stay in sync with tapplayer.h and apps/sdk/toast_app.h) --- */
#define PERM_PRINT       0x01   /* Terminal output            - safe, no prompt */
#define PERM_FS          0x02   /* FAT16 file system access   - prompts user    */
#define PERM_PANIC       0x04   /* Kernel panic / crash       - prompts user    */
#define PERM_TIME        0x08   /* Read system time           - safe, no prompt */
#define PERM_DEVICE      0x10   /* Direct device / port I/O   - prompts user    */
#define PERM_ALL         0xFF   /* All of the above           - prompts user    */

/* --- App metadata embedded in the ELF .tapp_meta section --- */
#define TAPP_META_MAGIC  "TAPP"
#define TAPP_NAME_LEN    48
#define TAPP_DEV_LEN     48
#define TAPP_VER_LEN     16

typedef struct {
    char     magic[4];
    char     name[TAPP_NAME_LEN];
    char     developer[TAPP_DEV_LEN];
    char     version[TAPP_VER_LEN];
    uint32_t permissions;
} __attribute__((packed)) TAppMeta;

/*
 * Syscall table passed as the first argument to every app entry point.
 * Apps must NOT cache or modify this pointer.
 * Layout must stay in sync with apps/sdk/toast_app.h.
 */
typedef struct {
    void  (*print)(const char *str);
    char *(*rec_input)(void);
    void  (*exitapp)(int code);
    void  (*app_panic)(const char *reason, int severe);
    int   (*get_permissions)(void);
} ToastSyscallTable;

/* App entry point signature: void __tapp_start(ToastSyscallTable*, int app_id) */
typedef void (*tapp_entry_fn)(ToastSyscallTable *, int);

/* --- Return codes from exec_run --- */
#define EXEC_OK             0
#define EXEC_ERR_NOTFOUND  -1   /* file not on FAT16              */
#define EXEC_ERR_ELF       -2   /* malformed ELF header           */
#define EXEC_ERR_SEGSEC    -3   /* segment security violation      */
#define EXEC_ERR_NOMETA    -4   /* missing .tapp_meta section      */
#define EXEC_ERR_DENIED    -5   /* user denied execution           */
#define EXEC_ERR_IDTHOOK   -6   /* IDT was modified during run     */

/* --- Public API --- */

/* Populate the syscall table. Call once after all drivers are ready. */
void exec_init(void);

/*
 * Load and run a .tapp ELF32 binary by FAT16 filename.
 * Returns EXEC_OK on clean exit, or a negative EXEC_ERR_* code.
 */
int exec_run(const char *filename);

#endif /* EXEC_H */
