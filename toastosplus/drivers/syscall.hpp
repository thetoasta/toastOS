/*
 * toastOS++ Syscall
 * Converted to C++ from toastOS
 */

#ifndef SYSCALL_HPP
#define SYSCALL_HPP

#ifdef __cplusplus
extern "C" {
#endif

/* toastOS Syscall Interface - POSIX-compatible INT 0x80 */

#include "stdint.hpp"

/* Syscall numbers (Linux-compatible subset for i386) */
#define SYS_EXIT        1
#define SYS_FORK        2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_WAITPID     7
#define SYS_UNLINK      10
#define SYS_CHDIR       12
#define SYS_TIME        13
#define SYS_LSEEK       19
#define SYS_GETPID      20
#define SYS_KILL        37
#define SYS_BRK         45
#define SYS_IOCTL       54
#define SYS_GETCWD      183
#define SYS_STAT        106
#define SYS_FSTAT       108
#define SYS_MKDIR       39
#define SYS_YIELD       158
#define SYS_NANOSLEEP   162
#define SYS_GETTID      224

#define MAX_SYSCALLS    256

/* Syscall handler type: takes up to 3 args, returns int */
typedef int (*syscall_fn_t)(uint32_t, uint32_t, uint32_t);

/* Initialise syscall table and register INT 0x80 */
void syscall_init(void);

/* Register a syscall handler */
void syscall_register(int num, syscall_fn_t handler);

/* Called from assembly ISR 0x80 stub */
void syscall_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSCALL_HPP */
