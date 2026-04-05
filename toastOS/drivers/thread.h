/* toastOS Threading - Cooperative & Preemptive Multithreading */
#ifndef THREAD_H
#define THREAD_H

#include "stdint.h"

#define MAX_THREADS     32
#define THREAD_STACK_SIZE 4096  /* 4KB per thread stack */

/* Thread states */
#define THREAD_UNUSED   0
#define THREAD_READY    1
#define THREAD_RUNNING  2
#define THREAD_BLOCKED  3
#define THREAD_SLEEPING 4
#define THREAD_DEAD     5

/* Thread ID type (POSIX pthread_t equivalent) */
typedef uint32_t tid_t;
typedef uint32_t pid_t;

/* Saved CPU context for context switching */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip;
    uint32_t eflags;
} cpu_context_t;

/* Thread Control Block */
typedef struct thread {
    tid_t           tid;
    pid_t           pid;            /* process ID (all kernel = pid 0) */
    uint8_t         state;
    uint8_t         priority;       /* 0 = highest */
    char            name[32];
    cpu_context_t   context;
    uint32_t        stack_base;     /* bottom of allocated stack */
    uint32_t        stack_size;
    uint32_t        sleep_until;    /* uptime ticks to sleep until */
    void           *exit_code;
} thread_t;

/* Initialise the threading subsystem */
void thread_init(void);

/* Create a new kernel thread. Returns tid or -1 on failure. */
tid_t thread_create(const char *name, void (*entry)(void *), void *arg);

/* Yield the CPU to the next ready thread */
void thread_yield(void);

/* Exit the current thread */
void thread_exit(void *retval);

/* Sleep the current thread for `ms` milliseconds */
void thread_sleep(uint32_t ms);

/* Get current thread ID */
tid_t thread_self(void);

/* Get current thread pointer */
thread_t *thread_current(void);

/* Called from timer IRQ to do preemptive scheduling */
void thread_schedule(void);

/* Block / unblock */
void thread_block(void);
void thread_unblock(tid_t tid);

/* POSIX-ish getpid */
pid_t getpid(void);

/* Mutex (simple spinlock for kernel threads) */
typedef struct {
    volatile uint32_t locked;
    tid_t owner;
} mutex_t;

#define MUTEX_INIT { 0, 0 }

void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
int  mutex_trylock(mutex_t *m);

#endif /* THREAD_H */
