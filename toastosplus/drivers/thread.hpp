/*
 * toastOS++ Threading
 * Namespace: toast::thread
 */

#ifndef THREAD_HPP
#define THREAD_HPP

#include "stdint.hpp"

#define MAX_THREADS     32
#define THREAD_STACK_SIZE 4096

/* Thread states */
#define THREAD_UNUSED   0
#define THREAD_READY    1
#define THREAD_RUNNING  2
#define THREAD_BLOCKED  3
#define THREAD_SLEEPING 4
#define THREAD_DEAD     5

/* Thread ID types */
typedef uint32_t tid_t;
typedef uint32_t pid_t;

/* Saved CPU context */
struct cpu_context_t {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip;
    uint32_t eflags;
};

/* Thread Control Block */
struct thread_t {
    tid_t           tid;
    pid_t           pid;
    uint8_t         state;
    uint8_t         priority;
    char            name[32];
    cpu_context_t   context;
    uint32_t        stack_base;
    uint32_t        stack_size;
    uint32_t        sleep_until;
    void           *exit_code;
};

/* Mutex (simple spinlock) */
struct mutex_t {
    volatile uint32_t locked;
    tid_t owner;
};

#define MUTEX_INIT { 0, 0 }

namespace toast {
namespace thread {

void init();
tid_t create(const char* name, void (*entry)(void*), void* arg);
void yield();
void exit(void* retval);
void sleep(uint32_t ms);
tid_t self();
thread_t* current();
void schedule();
void block();
void unblock(tid_t tid);
pid_t pid();

namespace mutex {
    void lock(mutex_t* m);
    void unlock(mutex_t* m);
    int trylock(mutex_t* m);
}

} // namespace thread
} // namespace toast

/* Legacy C-style function aliases */
void thread_init();
tid_t thread_create(const char* name, void (*entry)(void*), void* arg);
void thread_yield();
void thread_exit(void* retval);
void thread_sleep(uint32_t ms);
tid_t thread_self();
thread_t* thread_current();
void thread_schedule();
void thread_block();
void thread_unblock(tid_t tid);
pid_t getpid();
void mutex_lock(mutex_t* m);
void mutex_unlock(mutex_t* m);
int mutex_trylock(mutex_t* m);

#endif /* THREAD_HPP */
