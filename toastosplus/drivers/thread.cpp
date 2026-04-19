/*
 * toastOS++ Threading
 * Namespace: toast::thread
 */

#include "thread.hpp"
#include "mmu.hpp"
#include "kio.hpp"
#include "toast_libc.hpp"
#include "time.hpp"

namespace {
    thread_t threads[MAX_THREADS];
    tid_t current_tid = 0;
    tid_t next_tid = 1;
    int scheduler_active = 0;

    thread_t* find_thread(tid_t tid) {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].state != THREAD_UNUSED && threads[i].tid == tid)
                return &threads[i];
        }
        return nullptr;
    }

    int find_slot() {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].state == THREAD_UNUSED)
                return i;
        }
        return -1;
    }

    int pick_next() {
        int cur = -1;
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == current_tid && threads[i].state != THREAD_UNUSED) {
                cur = i;
                break;
            }
        }
        for (int off = 1; off <= MAX_THREADS; off++) {
            int idx = (cur + off) % MAX_THREADS;
            if (threads[idx].state == THREAD_READY)
                return idx;
        }
        return cur;
    }

    void wake_sleepers() {
        uint32_t now = get_uptime_seconds();
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].state == THREAD_SLEEPING && now >= threads[i].sleep_until) {
                threads[i].state = THREAD_READY;
            }
        }
    }
}

extern "C" void thread_switch_asm(uint32_t *old_esp, uint32_t new_esp);

static void thread_entry_wrapper(void (*entry)(void*), void* arg) {
    entry(arg);
    toast::thread::exit(nullptr);
}

namespace toast {
namespace thread {

void init() {
    memset(threads, 0, sizeof(threads));
    threads[0].tid = 0;
    threads[0].pid = 0;
    threads[0].state = THREAD_RUNNING;
    threads[0].priority = 0;
    strcpy(threads[0].name, "kernel_main");
    current_tid = 0;
    scheduler_active = 1;
}

tid_t create(const char* name, void (*entry)(void*), void* arg) {
    int slot = find_slot();
    if (slot < 0) return (tid_t)-1;

    uint32_t stack_mem = (uint32_t)toast::mem::alloc(THREAD_STACK_SIZE);
    if (!stack_mem) return (tid_t)-1;

    thread_t* t = &threads[slot];
    memset(t, 0, sizeof(thread_t));

    t->tid = next_tid++;
    t->pid = 0;
    t->state = THREAD_READY;
    t->priority = 1;
    strncpy(t->name, name ? name : "thread", 31);
    t->stack_base = stack_mem;
    t->stack_size = THREAD_STACK_SIZE;

    uint32_t* sp = (uint32_t*)(stack_mem + THREAD_STACK_SIZE);
    *(--sp) = (uint32_t)arg;
    *(--sp) = (uint32_t)entry;
    *(--sp) = 0xDEADBEEF;
    *(--sp) = (uint32_t)thread_entry_wrapper;
    *(--sp) = 0; // ebp
    *(--sp) = 0; // ebx
    *(--sp) = 0; // esi
    *(--sp) = 0; // edi

    t->context.esp = (uint32_t)sp;
    return t->tid;
}

void yield() {
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
    if (next_slot < 0 || next_slot == cur_slot) return;

    if (cur_slot >= 0 && threads[cur_slot].state == THREAD_RUNNING)
        threads[cur_slot].state = THREAD_READY;

    thread_t* old = (cur_slot >= 0) ? &threads[cur_slot] : nullptr;
    thread_t* new_t = &threads[next_slot];
    new_t->state = THREAD_RUNNING;
    current_tid = new_t->tid;

    if (old) {
        thread_switch_asm(&old->context.esp, new_t->context.esp);
    }
}

void exit(void* retval) {
    thread_t* t = current();
    if (t) {
        t->exit_code = retval;
        t->state = THREAD_DEAD;
    }
    yield();
    for (;;) __asm__ volatile("hlt");
}

void sleep(uint32_t ms) {
    thread_t* t = current();
    if (!t) return;

    uint32_t secs = (ms + 999) / 1000;
    if (secs == 0) secs = 1;

    t->sleep_until = get_uptime_seconds() + secs;
    t->state = THREAD_SLEEPING;
    yield();
}

tid_t self() {
    return current_tid;
}

thread_t* current() {
    return find_thread(current_tid);
}

void schedule() {
    if (!scheduler_active) return;
    wake_sleepers();
    yield();
}

void block() {
    thread_t* t = current();
    if (t) {
        t->state = THREAD_BLOCKED;
        yield();
    }
}

void unblock(tid_t tid) {
    thread_t* t = find_thread(tid);
    if (t && t->state == THREAD_BLOCKED)
        t->state = THREAD_READY;
}

pid_t pid() {
    thread_t* t = current();
    return t ? t->pid : 0;
}

namespace mutex {

void lock(mutex_t* m) {
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        toast::thread::yield();
    }
    m->owner = current_tid;
}

void unlock(mutex_t* m) {
    m->owner = 0;
    __sync_lock_release(&m->locked);
}

int trylock(mutex_t* m) {
    if (__sync_lock_test_and_set(&m->locked, 1) == 0) {
        m->owner = current_tid;
        return 0;
    }
    return -1;
}

} // namespace mutex
} // namespace thread
} // namespace toast

/* Legacy C aliases */
void thread_init() { toast::thread::init(); }
tid_t thread_create(const char* name, void (*entry)(void*), void* arg) { return toast::thread::create(name, entry, arg); }
void thread_yield() { toast::thread::yield(); }
void thread_exit(void* retval) { toast::thread::exit(retval); }
void thread_sleep(uint32_t ms) { toast::thread::sleep(ms); }
tid_t thread_self() { return toast::thread::self(); }
thread_t* thread_current() { return toast::thread::current(); }
void thread_schedule() { toast::thread::schedule(); }
void thread_block() { toast::thread::block(); }
void thread_unblock(tid_t tid) { toast::thread::unblock(tid); }
pid_t getpid() { return toast::thread::pid(); }
void mutex_lock(mutex_t* m) { toast::thread::mutex::lock(m); }
void mutex_unlock(mutex_t* m) { toast::thread::mutex::unlock(m); }
int mutex_trylock(mutex_t* m) { return toast::thread::mutex::trylock(m); }
