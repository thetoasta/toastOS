/*
 * toastOS++ Toast Libc
 * Converted to C++ from toastOS
 */

#ifndef TOAST_LIBC_HPP
#define TOAST_LIBC_HPP

#ifdef __cplusplus
extern "C" {
#endif

/*
 * toastOS C standard library (glibc-compatible subset)
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

/* ---- stddef.h ---- */
#ifndef _STDDEF_H
#define _STDDEF_H
typedef unsigned int size_t;
typedef int ptrdiff_t;
#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif
#endif
#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type *)0)->member)
#endif
#endif

/* ---- stdbool.h ---- */
#ifndef _STDBOOL_H
#define _STDBOOL_H
#ifndef __cplusplus
#define bool  _Bool
#define true  1
#define false 0
#endif
#endif

/* ---- limits.h ---- */
#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define CHAR_MIN   SCHAR_MIN
#define CHAR_MAX   SCHAR_MAX
#define SHRT_MIN   (-32768)
#define SHRT_MAX   32767
#define USHRT_MAX  65535
#define INT_MIN    (-2147483647 - 1)
#define INT_MAX    2147483647
#define UINT_MAX   4294967295U
#define LONG_MIN   (-2147483647L - 1L)
#define LONG_MAX   2147483647L
#define ULONG_MAX  4294967295UL
#define LLONG_MIN  (-9223372036854775807LL - 1LL)
#define LLONG_MAX  9223372036854775807LL
#define ULLONG_MAX 18446744073709551615ULL
#define PATH_MAX   256
#define NAME_MAX   255
#define OPEN_MAX   64
#define PIPE_BUF   4096

/* ---- stdarg.h ---- */
/* GCC builtins – guarded so we don't clash with GCC's own <stdarg.h> */
#ifndef _STDARG_H
typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_end(v)     __builtin_va_end(v)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_copy(d,s)  __builtin_va_copy(d,s)
#endif

/* ---- ctype.h ---- */
int isalnum(int c);
int isalpha(int c);
int isdigit(int c);
int isxdigit(int c);
int islower(int c);
int isupper(int c);
int isspace(int c);
int isprint(int c);
int isgraph(int c);
int iscntrl(int c);
int ispunct(int c);
int isblank(int c);
int toupper(int c);
int tolower(int c);
int isascii(int c);
int toascii(int c);

/* ---- string.h ---- */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
int    strcasecmp(const char *s1, const char *s2);
int    strncasecmp(const char *s1, const char *s2, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
char  *strcat(char *dest, const char *src);
char  *strncat(char *dest, const char *src, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);
char  *strpbrk(const char *s, const char *accept);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char  *strtok(char *str, const char *delim);
char  *strtok_r(char *str, const char *delim, char **saveptr);
char  *strdup(const char *s);
char  *strndup(const char *s, size_t n);
char  *strerror(int errnum);

/* ---- stdlib.h ---- */
void *malloc(size_t size);
void  free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);

int   abs(int x);
long  labs(long x);
int   atoi(const char *nptr);
long  atol(const char *nptr);
long  strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

void  qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *));

char *getenv(const char *name);
void  abort(void);
void  exit(int status);
int   atexit(void (*func)(void));

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 32767
int   rand(void);
void  srand(unsigned int seed);

/* div_t / ldiv_t */
typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
div_t  div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);

/* ---- setjmp / longjmp ---- */
typedef int jmp_buf[6];  /* eax, ebx, ecx, edx, esp, eip (i386) */
int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

/* ---- snprintf (minimal) ---- */
int snprintf(char *str, size_t size, const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

/* ---- assert.h ---- */
void __assert_fail(const char *expr, const char *file, int line);
#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))
#endif

/* ---- signal.h (stubs) ---- */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV   11
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGUSR1   10
#define SIGUSR2   12
#define SIG_DFL   ((void (*)(int))0)
#define SIG_IGN   ((void (*)(int))1)
#define SIG_ERR   ((void (*)(int))-1)

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
int raise(int sig);

#ifdef __cplusplus
}
#endif

#endif /* TOAST_LIBC_HPP */
