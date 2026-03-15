/*
 * tapp_start.c — toastOS app entry shim
 *
 * This file is compiled into every .tapp binary.
 * The kernel calls __tapp_start(syscalls, app_id), which stores the
 * syscall table pointer and then calls user-provided app_main().
 *
 * Do NOT modify this file unless you know what you are doing.
 */

#include "toast_app.h"

/* Global pointer to the kernel-provided syscall table.
   Accessible to all compilation units that include toast_app.h. */
ToastSyscallTable *__toast_syscalls = (ToastSyscallTable *)0;

/*
 * Kernel entry point for the app.
 * The loader calls:   __tapp_start(&g_syscall_table, app_id)
 */
void __tapp_start(ToastSyscallTable *syscalls, int app_id) {
    __toast_syscalls = syscalls;
    app_main(app_id);
}
