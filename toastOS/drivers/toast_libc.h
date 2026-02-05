/*
 * toastOS C standard library shims for FreeType
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#ifndef TOAST_LIBC_H
#define TOAST_LIBC_H

/* ---- stddef.h ---- */
#ifndef _STDDEF_H
#define _STDDEF_H
typedef unsigned int size_t;
typedef int ptrdiff_t;
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type *)0)->member)
#endif
#endif

/* ---- limits.h ---- */
#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define SHRT_MIN   (-32768)
#define SHRT_MAX   32767
#define USHRT_MAX  65535
#define INT_MIN    (-2147483647 - 1)
#define INT_MAX    2147483647
#define UINT_MAX   4294967295U
#define LONG_MIN   (-2147483647L - 1L)
#define LONG_MAX   2147483647L
#define ULONG_MAX  4294967295UL

/* ---- string functions ---- */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
char  *strcat(char *dest, const char *src);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);

/* ---- memory allocation ---- */
void *malloc(size_t size);
void  free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);

/* ---- sorting ---- */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

/* ---- misc ---- */
long strtol(const char *nptr, char **endptr, int base);
char *getenv(const char *name);

/* ---- setjmp / longjmp ---- */
typedef int jmp_buf[6];  /* eax, ebx, ecx, edx, esp, eip (i386) */
int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

/* ---- snprintf (minimal) ---- */
int snprintf(char *str, size_t size, const char *fmt, ...);

/* ---- stdarg.h ---- */
/* GCC builtins – guarded so we don't clash with GCC's own <stdarg.h> */
#ifndef _STDARG_H
typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_end(v)     __builtin_va_end(v)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_copy(d,s)  __builtin_va_copy(d,s)
#endif

#endif /* TOAST_LIBC_H */
