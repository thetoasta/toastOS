/*
 * toastOS C standard library shims for FreeType
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#include "toast_libc.h"

/* ===== memory functions ===== */

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

static int isspace_c(int c) {
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\f' || c == '\v';
}

static int isdigit_c(int c) { return c >= '0' && c <= '9'; }
static int isalpha_c(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    long acc = 0;
    int neg = 0;

    while (isspace_c(*s)) s++;
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
        if (isdigit_c(*s)) digit = *s - '0';
        else if (isalpha_c(*s)) digit = (*s | 0x20) - 'a' + 10;
        else break;
        if (digit >= base) break;
        acc = acc * base + digit;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -acc : acc;
}

char *getenv(const char *name) {
    (void)name;
    return NULL; /* no environment in a kernel */
}

/* ===== setjmp / longjmp (i386 only) ===== */
/* Implemented in setjmp.asm */

/* snprintf is provided by stdio.c – not duplicated here */
