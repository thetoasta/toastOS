#ifndef STDIO_H
#define STDIO_H

#include "toast_libc.h"

// Define size_t if it's not already defined
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

/*
 * A limited implementation of snprintf.
 * Formats a string and stores it in a buffer.
 *
 * buffer: The buffer to store the formatted string.
 * size: The maximum number of characters to write to the buffer (including the null terminator).
 * format: The format string.
 * ...: The arguments for the format string.
 *
 * Returns the number of characters that would have been written if the buffer was large enough,
 * not including the null terminator.
 *
 * Supported format specifiers:
 * %s - string
 * %c - character
 * %d - signed decimal integer
 * %u - unsigned decimal integer
 * %x - unsigned hexadecimal integer (lowercase)
 * %p - pointer address
 * %% - literal '%'
 */
int snprintf(char *buffer, size_t size, const char *format, ...);

/*
 * A limited implementation of vsnprintf.
 * This is the same as snprintf, but takes a va_list instead of a variable number of arguments.
 */
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);

/* ===== Buffered FILE* I/O (POSIX / glibc compatible) ===== */

#ifndef _TOAST_FILE_DEFINED
#define _TOAST_FILE_DEFINED

#define BUFSIZ          512
#define FOPEN_MAX       64
#define FILENAME_MAX    256
#define _IOFBF          0   /* fully buffered */
#define _IOLBF          1   /* line buffered  */
#define _IONBF          2   /* unbuffered     */
#define EOF             (-1)

/* FILE structure */
typedef struct _toast_FILE {
    int   fd;               /* underlying POSIX fd         */
    int   flags;            /* open flags                  */
    int   eof;              /* EOF indicator               */
    int   error;            /* error indicator             */
    int   unget;            /* ungetc char (-1 if none)    */
    char  mode[8];          /* fopen mode string           */
} toast_FILE;

/* Standard streams - defined in stdio.c */
extern toast_FILE *toast_stdin_ptr;
extern toast_FILE *toast_stdout_ptr;
extern toast_FILE *toast_stderr_ptr;

/* Override the names used in normal C code */
#define FILE   toast_FILE
#define stdin  toast_stdin_ptr
#define stdout toast_stdout_ptr
#define stderr toast_stderr_ptr

/* ---- File open/close ---- */
FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int   fclose(FILE *stream);
int   fflush(FILE *stream);

/* ---- Byte I/O ---- */
int   fgetc(FILE *stream);
int   fputc(int c, FILE *stream);
int   getc(FILE *stream);
int   putc(int c, FILE *stream);
int   getchar(void);
int   putchar(int c);
int   ungetc(int c, FILE *stream);

/* ---- String I/O ---- */
char *fgets(char *s, int size, FILE *stream);
int   fputs(const char *s, FILE *stream);
int   puts(const char *s);

/* ---- Block I/O ---- */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* ---- Seek / tell ---- */
int   fseek(FILE *stream, long offset, int whence);
long  ftell(FILE *stream);
void  rewind(FILE *stream);
int   feof(FILE *stream);
int   ferror(FILE *stream);
void  clearerr(FILE *stream);

/* ---- Formatted I/O ---- */
int   fprintf(FILE *stream, const char *fmt, ...);
int   vfprintf(FILE *stream, const char *fmt, va_list ap);
int   printf(const char *fmt, ...);
int   sprintf(char *str, const char *fmt, ...);
int   sscanf(const char *str, const char *fmt, ...);

/* ---- File ops ---- */
int   remove(const char *path);
int   rename(const char *oldpath, const char *newpath);
FILE *tmpfile(void);

/* ---- Misc ---- */
void  perror(const char *s);
int   fileno(FILE *stream);

#endif /* _TOAST_FILE_DEFINED */

#endif /* STDIO_H */
