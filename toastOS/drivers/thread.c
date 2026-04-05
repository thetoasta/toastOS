/* toastOS Threading - Cooperative & Preemptive Multithreading */

#include "thread.h"
#include "mmu.h"
#include "kio.h"
#include "toast_libc.h"
#include "time.h"

/* Thread table */
static thread_t threads[MAX_THREADS];
static tid_t current_tid = 0;
static tid_t next_tid = 1;        /* tid counter (0 = kernel main) */
static int scheduler_active = 0;

/* ---- Assembly helpers for context switching ---- */

/* Defined in kernel.asm */
extern void thread_switch_asm(uint32_t *old_esp, uint32_t new_esp);

/* ---- Internal helpers ---- */

static thread_t *find_thread(tid_t tid) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state != THREAD_UNUSED && threads[i].tid == tid)
            return &threads[i];
    }
    return (thread_t *)0;
}

static int find_slot(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED)
            return i;
    }
    return -1;
}

/* Simple round-robin: find next READY thread */
static int pick_next(void) {
    int cur = -1;
    /* Find current thread's slot index */
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == current_tid && threads[i].state != THREAD_UNUSED) {
            cur = i;
            break;
        }
    }

    /* Round-robin from current position */
    for (int off = 1; off <= MAX_THREADS; off++) {
        int idx = (cur + off) % MAX_THREADS;
        if (threads[idx].state == THREAD_READY)
            return idx;
    }
    return cur; /* stay on current if nothing else is ready */
}

/* Wake threads whose sleep time has elapsed */
static void wake_sleepers(void) {
    uint32_t now = get_uptime_seconds();
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_SLEEPING && now >= threads[i].sleep_until) {
            threads[i].state = THREAD_READY;
        }
    }
}

/* Wrapper that calls the thread entry and cleans up */
static void thread_entry_wrapper(void (*entry)(void *), void *arg) {
    entry(arg);
    thread_exit((void *)0);
}

/* ---- Public API ---- */

void thread_init(void) {
    /* Clear all thread slots */
    memset(threads, 0, sizeof(threads));

    /* Thread 0 = the kernel main thread (already running) */
    threads[0].tid = 0;
    threads[0].pid = 0;
    threads[0].state = THREAD_RUNNING;
    threads[0].priority = 0;
    strcpy(threads[0].name, "kernel_main");
    /* context.esp will be set when we first switch away from it */

    current_tid = 0;
    scheduler_active = 1;
}

tid_t thread_create(const char *name, void (*entry)(void *), void *arg) {
    int slot = find_slot();
    if (slot < 0)
        return (tid_t)-1;

    /* Allocate a stack */
    uint32_t stack_mem = (uint32_t)kmalloc(THREAD_STACK_SIZE);
    if (!stack_mem)
        return (tid_t)-1;

    thread_t *t = &threads[slot];
    memset(t, 0, sizeof(thread_t));

    t->tid = next_tid++;
    t->pid = 0;  /* all kernel threads share pid 0 */
    t->state = THREAD_READY;
    t->priority = 1;
    strncpy(t->name, name ? name : "thread", 31);
    t->stack_base = stack_mem;
    t->stack_size = THREAD_STACK_SIZE;

    /*
     * Set up the initial stack so that when thread_switch_asm
     * pops and returns, it enters thread_entry_wrapper(entry, arg).
     *
     * Stack layout (grows down):
     *   [top of stack]
     *   arg            <- argument 2 to wrapper
     *   entry          <- argument 1 to wrapper
     *   0xDEADBEEF     <- fake return address (thread should call thread_exit)
     *   ebp=0
     *   ebx=0
     *   esi=0
     *   edi=0          <- ESP points here after switch
     */
    uint32_t *sp = (uint32_t *)(stack_mem + THREAD_STACK_SIZE);

    /* Arguments for thread_entry_wrapper */
    *(--sp) = (uint32_t)arg;
    *(--sp) = (uint32_t)entry;
    *(--sp) = 0xDEADBEEF;  /* fake return address */

    /* EIP = thread_entry_wrapper */
    *(--sp) = (uint32_t)thread_entry_wrapper;

    /* Saved registers: ebp, ebx, esi, edi */
    *(--sp) = 0; /* ebp */
    *(--sp) = 0; /* ebx */
    *(--sp) = 0; /* esi */
    *(--sp) = 0; /* edi */

    t->context.esp = (uint32_t)sp;

    return t->tid;
}

void thread_yield(void) {
    if (!scheduler_active) return;

    wake_sleepers();

    int cur_slot = -1;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == current_tid && threads[i].state == THREAD_RUNNING) {
            cur_slot = i;
            break;
        }
    }

    int next_slot = pick_next();
    if (next_slot < 0 || next_slot == cur_slot)
        return;

    /* Mark current as ready (unless it's dead/blocked/sleeping) */
    if (cur_slot >= 0 && threads[cur_slot].state == THREAD_RUNNING)
        threads[cur_slot].state = THREAD_READY;

    /* Switch to next */
    thread_t *old = (cur_slot >= 0) ? &threads[cur_slot] : (thread_t *)0;
    thread_t *new_t = &threads[next_slot];
    new_t->state = THREAD_RUNNING;
    current_tid = new_t->tid;

    if (old) {
        thread_switch_asm(&old->context.esp, new_t->context.esp);
    }
}

void thread_exit(void *retval) {
    thread_t *t = thread_current();
    if (t) {
        t->exit_code = retval;
        t->state = THREAD_DEAD;
    }

    /* Switch to another thread; we'll never come back */
    thread_yield();

    /* Should not reach here, but just in case */
    for (;;) __asm__ volatile("hlt");
}

void thread_sleep(uint32_t ms) {
    thread_t *t = thread_current();
    if (!t) return;

    /* Convert ms to seconds (our timer is coarse) */
    uint32_t secs = (ms + 999) / 1000;
    if (secs == 0) secs = 1;

    t->sleep_until = get_uptime_seconds() + secs;
    t->state = THREAD_SLEEPING;
    thread_yield();
}

tid_t thread_self(void) {
    return current_tid;
}

thread_t *thread_current(void) {
    return find_thread(current_tid);
}

/* Called from timer IRQ for preemptive scheduling */
void thread_schedule(void) {
    if (!scheduler_active) return;
    wake_sleepers();
    /* For now, preemption just yields. Full register save/restore
     * happens inside thread_yield -> thread_switch_asm. */
    thread_yield();
}

void thread_block(void) {
    thread_t *t = thread_current();
    if (t) {
        t->state = THREAD_BLOCKED;
        thread_yield();
    }
}

void thread_unblock(tid_t tid) {
    thread_t *t = find_thread(tid);
    if (t && t->state == THREAD_BLOCKED)
        t->state = THREAD_READY;
}

pid_t getpid(void) {
    thread_t *t = thread_current();
    return t ? t->pid : 0;
}

/* ---- Mutex ---- */

void mutex_lock(mutex_t *m) {
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        thread_yield(); /* don't busy-spin, let others run */
    }
    m->owner = current_tid;
}

void mutex_unlock(mutex_t *m) {
    m->owner = 0;
    __sync_lock_release(&m->locked);
}

int mutex_trylock(mutex_t *m) {
    if (__sync_lock_test_and_set(&m->locked, 1) == 0) {
        m->owner = current_tid;
        return 0; /* success */
    }
    return -1; /* already locked */
}
