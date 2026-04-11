/*
 * toastOS C standard library (glibc-compatible subset)
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#include "toast_libc.hpp"
#include "kio.hpp"

/* ===== ctype functions ===== */

int isdigit(int c)  { return c >= '0' && c <= '9'; }
int islower(int c)  { return c >= 'a' && c <= 'z'; }
int isupper(int c)  { return c >= 'A' && c <= 'Z'; }
int isalpha(int c)  { return islower(c) || isupper(c); }
int isalnum(int c)  { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isspace(int c)  { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isprint(int c)  { return c >= 0x20 && c <= 0x7E; }
int isgraph(int c)  { return c > 0x20 && c <= 0x7E; }
int iscntrl(int c)  { return (c >= 0 && c < 0x20) || c == 0x7F; }
int ispunct(int c)  { return isgraph(c) && !isalnum(c); }
int isblank(int c)  { return c == ' ' || c == '\t'; }
int isascii(int c)  { return (unsigned)c <= 127; }
int toascii(int c)  { return c & 0x7F; }
int toupper(int c)  { return islower(c) ? c - 32 : c; }
int tolower(int c)  { return isupper(c) ? c + 32 : c; }

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        if (*a != *b) return *a - *b;
        a++; b++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return NULL;
}

/* ===== string functions ===== */

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

/* ===== additional string functions ===== */

size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) len++;
    return len;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
        s1++; s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) d++;
    while (n-- && *src) *d++ = *src++;
    *d = '\0';
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (char)c == '\0' ? (char *)s : NULL;
}

char *strpbrk(const char *s, const char *accept) {
    for (; *s; s++) {
        for (const char *a = accept; *a; a++) {
            if (*s == *a) return (char *)s;
        }
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    for (; *s; s++) {
        const char *a = accept;
        int found = 0;
        for (; *a; a++) {
            if (*s == *a) { found = 1; break; }
        }
        if (!found) break;
        count++;
    }
    return count;
}

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    for (; *s; s++) {
        for (const char *r = reject; *r; r++) {
            if (*s == *r) return count;
        }
        count++;
    }
    return count;
}

static char *strtok_save = NULL;

char *strtok(char *str, const char *delim) {
    return strtok_r(str, delim, &strtok_save);
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (!str) str = *saveptr;
    if (!str) return NULL;

    /* Skip leading delimiters */
    str += strspn(str, delim);
    if (*str == '\0') { *saveptr = NULL; return NULL; }

    /* Find end of token */
    char *end = str + strcspn(str, delim);
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = NULL;
    }
    return str;
}

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = (char *)malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *d = (char *)malloc(len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = '\0';
    }
    return d;
}

/* Error strings for strerror */
char *strerror(int errnum) {
    switch (errnum) {
        case 0:  return (char *)"Success";
        case 2:  return (char *)"No such file or directory";
        case 5:  return (char *)"I/O error";
        case 9:  return (char *)"Bad file descriptor";
        case 12: return (char *)"Out of memory";
        case 13: return (char *)"Permission denied";
        case 17: return (char *)"File exists";
        case 22: return (char *)"Invalid argument";
        case 24: return (char *)"Too many open files";
        case 28: return (char *)"No space left on device";
        case 38: return (char *)"Function not implemented";
        default: return (char *)"Unknown error";
    }
}

/* ===== simple heap allocator ===== */

#define HEAP_SIZE (2 * 1024 * 1024) /* 2 MB */

/* Block header for tracking allocations */
typedef struct block_header {
    size_t size;              /* payload size */
    int    used;              /* 1 = in use, 0 = free */
    struct block_header *next;
} block_header_t;

static unsigned char heap[HEAP_SIZE] __attribute__((aligned(16)));
static int heap_initialized = 0;
static block_header_t *free_list = NULL;

static void heap_init(void) {
    free_list = (block_header_t *)heap;
    free_list->size = HEAP_SIZE - sizeof(block_header_t);
    free_list->used = 0;
    free_list->next = NULL;
    heap_initialized = 1;
}

void *malloc(size_t size) {
    if (!heap_initialized) heap_init();
    if (size == 0) return NULL;

    /* align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    block_header_t *curr = free_list;
    while (curr) {
        if (!curr->used && curr->size >= size) {
            /* split if there's room for another block */
            if (curr->size >= size + sizeof(block_header_t) + 8) {
                block_header_t *new_block =
                    (block_header_t *)((unsigned char *)(curr + 1) + size);
                new_block->size = curr->size - size - sizeof(block_header_t);
                new_block->used = 0;
                new_block->next = curr->next;
                curr->size = size;
                curr->next = new_block;
            }
            curr->used = 1;
            return (void *)(curr + 1);
        }
        curr = curr->next;
    }
    return NULL; /* out of memory */
}

void free(void *ptr) {
    if (!ptr) return;
    block_header_t *blk = ((block_header_t *)ptr) - 1;
    blk->used = 0;

    /* coalesce adjacent free blocks */
    block_header_t *curr = free_list;
    while (curr) {
        if (!curr->used && curr->next && !curr->next->used) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
            continue; /* check again in case of triple-coalesce */
        }
        curr = curr->next;
    }
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }

    block_header_t *blk = ((block_header_t *)ptr) - 1;
    if (blk->size >= size) return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, blk->size);
        free(ptr);
    }
    return new_ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

/* ===== qsort (simple shell sort) ===== */

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
    unsigned char *b = (unsigned char *)base;
    unsigned char tmp[256]; /* fine for FreeType's usage */
    size_t gap, i, j;
    for (gap = nmemb / 2; gap > 0; gap /= 2) {
        for (i = gap; i < nmemb; i++) {
            memcpy(tmp, b + i * size, size);
            for (j = i; j >= gap; j -= gap) {
                if (compar(b + (j - gap) * size, tmp) > 0)
                    memcpy(b + j * size, b + (j - gap) * size, size);
                else
                    break;
            }
            memcpy(b + j * size, tmp, size);
        }
    }
}

/* ===== strtol ===== */

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    long acc = 0;
    int neg = 0;

    while (isspace(*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && s[0] == '0' &&
        (s[1] == 'x' || s[1] == 'X')) {
        base = 16; s += 2;
    } else if (base == 0 && s[0] == '0') {
        base = 8; s++;
    } else if (base == 0) {
        base = 10;
    }

    while (*s) {
        int digit;
        if (isdigit(*s)) digit = *s - '0';
        else if (isalpha(*s)) digit = tolower(*s) - 'a' + 10;
        else break;
        if (digit >= base) break;
        acc = acc * base + digit;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -acc : acc;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc = 0;

    while (isspace(*s)) s++;
    if (*s == '+') s++;

    if ((base == 0 || base == 16) && s[0] == '0' &&
        (s[1] == 'x' || s[1] == 'X')) {
        base = 16; s += 2;
    } else if (base == 0 && s[0] == '0') {
        base = 8; s++;
    } else if (base == 0) {
        base = 10;
    }

    while (*s) {
        int digit;
        if (isdigit(*s)) digit = *s - '0';
        else if (isalpha(*s)) digit = tolower(*s) - 'a' + 10;
        else break;
        if (digit >= base) break;
        acc = acc * base + digit;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return acc;
}

int atoi(const char *nptr)  { return (int)strtol(nptr, NULL, 10); }
long atol(const char *nptr) { return strtol(nptr, NULL, 10); }
int abs(int x)              { return x < 0 ? -x : x; }
long labs(long x)           { return x < 0 ? -x : x; }

div_t div(int numer, int denom) {
    div_t r;
    r.quot = numer / denom;
    r.rem  = numer % denom;
    return r;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t r;
    r.quot = numer / denom;
    r.rem  = numer % denom;
    return r;
}

char *getenv(const char *name) {
    (void)name;
    return NULL; /* no environment in a kernel */
}

void abort(void) {
    __asm__ volatile("cli; hlt");
    for (;;);
}

void exit(int status) {
    (void)status;
    __asm__ volatile("cli; hlt");
    for (;;);
}

static void (*atexit_funcs[32])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (atexit_count >= 32) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

/* ===== rand / srand (LCG) ===== */

static unsigned int rand_seed = 1;

void srand(unsigned int seed) { rand_seed = seed; }

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & RAND_MAX);
}

/* ===== bsearch ===== */

void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *)) {
    const unsigned char *b = (const unsigned char *)base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = compar(key, b + mid * size);
        if (cmp == 0) return (void *)(b + mid * size);
        if (cmp > 0) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}

/* ===== signal stubs ===== */

static sighandler_t signal_handlers[32];

sighandler_t signal(int signum, sighandler_t handler) {
    if (signum < 0 || signum >= 32) return SIG_ERR;
    sighandler_t old = signal_handlers[signum];
    signal_handlers[signum] = handler;
    return old;
}

int raise(int sig) {
    if (sig < 0 || sig >= 32) return -1;
    sighandler_t h = signal_handlers[sig];
    if (h == SIG_DFL || h == SIG_IGN) return 0;
    if (h != SIG_ERR) h(sig);
    return 0;
}

/* ===== assert ===== */

void __assert_fail(const char *expr, const char *file, int line) {
    kprint("ASSERTION FAILED: ");
    kprint(expr);
    kprint(" at ");
    kprint(file);
    kprint_newline();
    abort();
    (void)line;
}

/* ===== sprintf (wraps snprintf) ===== */

int sprintf(char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    /* Use a large limit; caller must ensure buffer is big enough */
    int ret = vsnprintf(str, 65536, fmt, ap);
    va_end(ap);
    return ret;
}

/* snprintf is provided by stdio.c – not duplicated here */
