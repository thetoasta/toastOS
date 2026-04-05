/* toastOS Syscall Interface - POSIX-compatible INT 0x80 */

#include "syscall.h"
#include "posix.h"
#include "thread.h"
#include "toast_libc.h"
#include "kio.h"
#include "time.h"
#include "fat16.h"

/* Syscall dispatch table */
static syscall_fn_t syscall_table[MAX_SYSCALLS];

/* ---- Built-in syscall implementations ---- */

static int sys_exit_impl(uint32_t code, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    thread_exit((void *)code);
    return 0; /* unreachable */
}

static int sys_fork_impl(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    /* Fork is not fully implementable without user-space processes.
     * Return -1 with errno = ENOSYS */
    return -1;
}

static int sys_read_impl(uint32_t fd, uint32_t buf, uint32_t count) {
    return posix_read((int)fd, (void *)buf, (int)count);
}

static int sys_write_impl(uint32_t fd, uint32_t buf, uint32_t count) {
    return posix_write((int)fd, (const void *)buf, (int)count);
}

static int sys_open_impl(uint32_t path, uint32_t flags, uint32_t mode) {
    (void)mode;
    return posix_open((const char *)path, (int)flags);
}

static int sys_close_impl(uint32_t fd, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return posix_close((int)fd);
}

static int sys_lseek_impl(uint32_t fd, uint32_t offset, uint32_t whence) {
    return posix_lseek((int)fd, (int)offset, (int)whence);
}

static int sys_getpid_impl(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    return (int)getpid();
}

static int sys_brk_impl(uint32_t addr, uint32_t b, uint32_t c) {
    (void)addr; (void)b; (void)c;
    /* Simple stub: we don't have per-process heaps yet */
    return -1;
}

static int sys_chdir_impl(uint32_t path, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return fat16_chdir((const char *)path);
}

static int sys_getcwd_impl(uint32_t buf, uint32_t size, uint32_t c) {
    (void)c;
    const char *cwd = fat16_getcwd();
    if (!cwd) return -1;
    size_t len = strlen(cwd);
    if (len + 1 > size) return -1;
    memcpy((void *)buf, cwd, len + 1);
    return (int)len;
}

static int sys_unlink_impl(uint32_t path, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return fat16_delete_at((const char *)path);
}

static int sys_mkdir_impl(uint32_t path, uint32_t mode, uint32_t c) {
    (void)mode; (void)c;
    return fat16_mkdir((const char *)path);
}

static int sys_yield_impl(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    thread_yield();
    return 0;
}

static int sys_gettid_impl(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    return (int)thread_self();
}

static int sys_nanosleep_impl(uint32_t ms, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    thread_sleep(ms);
    return 0;
}

static int sys_kill_impl(uint32_t pid, uint32_t sig, uint32_t c) {
    (void)pid; (void)sig; (void)c;
    /* Stub: no signal delivery yet */
    return -1;
}

static int sys_stat_impl(uint32_t path, uint32_t buf, uint32_t c) {
    (void)c;
    return posix_stat((const char *)path, (struct posix_stat *)buf);
}

static int sys_fstat_impl(uint32_t fd, uint32_t buf, uint32_t c) {
    (void)c;
    return posix_fstat((int)fd, (struct posix_stat *)buf);
}

static int sys_ioctl_impl(uint32_t fd, uint32_t cmd, uint32_t arg) {
    (void)fd; (void)cmd; (void)arg;
    return -1; /* stub */
}

static int sys_time_impl(uint32_t tptr, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    uint32_t t = get_uptime_seconds();
    if (tptr)
        *(uint32_t *)tptr = t;
    return (int)t;
}

/* ---- Initialisation ---- */

void syscall_init(void) {
    /* Clear the table */
    memset(syscall_table, 0, sizeof(syscall_table));

    /* Register built-in syscalls */
    syscall_table[SYS_EXIT]      = sys_exit_impl;
    syscall_table[SYS_FORK]      = sys_fork_impl;
    syscall_table[SYS_READ]      = sys_read_impl;
    syscall_table[SYS_WRITE]     = sys_write_impl;
    syscall_table[SYS_OPEN]      = sys_open_impl;
    syscall_table[SYS_CLOSE]     = sys_close_impl;
    syscall_table[SYS_LSEEK]     = sys_lseek_impl;
    syscall_table[SYS_GETPID]    = sys_getpid_impl;
    syscall_table[SYS_BRK]       = sys_brk_impl;
    syscall_table[SYS_CHDIR]     = sys_chdir_impl;
    syscall_table[SYS_GETCWD]    = sys_getcwd_impl;
    syscall_table[SYS_UNLINK]    = sys_unlink_impl;
    syscall_table[SYS_MKDIR]     = sys_mkdir_impl;
    syscall_table[SYS_YIELD]     = sys_yield_impl;
    syscall_table[SYS_GETTID]    = sys_gettid_impl;
    syscall_table[SYS_NANOSLEEP] = sys_nanosleep_impl;
    syscall_table[SYS_KILL]      = sys_kill_impl;
    syscall_table[SYS_STAT]      = sys_stat_impl;
    syscall_table[SYS_FSTAT]     = sys_fstat_impl;
    syscall_table[SYS_IOCTL]     = sys_ioctl_impl;
    syscall_table[SYS_TIME]      = sys_time_impl;
    syscall_table[SYS_WAITPID]   = sys_fork_impl;  /* stub, same as fork */
}

void syscall_register(int num, syscall_fn_t handler) {
    if (num >= 0 && num < MAX_SYSCALLS)
        syscall_table[num] = handler;
}

/*
 * Called from the INT 0x80 assembly stub.
 * Registers on entry (Linux i386 convention):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3
 * Return value goes back in EAX.
 */
typedef struct {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
} syscall_regs_t;

void syscall_dispatch(syscall_regs_t *regs) {
    uint32_t num = regs->eax;

    if (num < MAX_SYSCALLS && syscall_table[num]) {
        regs->eax = (uint32_t)syscall_table[num](regs->ebx, regs->ecx, regs->edx);
    } else {
        regs->eax = (uint32_t)-1; /* ENOSYS */
    }
}
