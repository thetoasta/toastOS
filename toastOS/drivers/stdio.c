#include "stdio.h"
#include "toast_libc.h"

// A simple implementation of vsnprintf
int vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    char *p = buffer;
    const char *end = buffer + size - 1;
    int written = 0;

    for (; *format != '\0'; ++format) {
        if (*format == '%') {
            ++format;
            if (*format == 'd') {
                int i = va_arg(args, int);
                char buf[12];
                char *q = buf + sizeof(buf) - 1;
                *q = '\0';
                if (i == 0) {
                    *--q = '0';
                } else {
                    int sign = i < 0;
                    if (sign) i = -i;
                    while (i > 0) {
                        *--q = (i % 10) + '0';
                        i /= 10;
                    }
                    if (sign) *--q = '-';
                }
                while (*q) {
                    if (p < end) *p++ = *q;
                    q++;
                    written++;
                }
            } else if (*format == 's') {
                const char *s = va_arg(args, const char *);
                while (*s) {
                    if (p < end) *p++ = *s;
                    s++;
                    written++;
                }
            } else if (*format == '%') {
                if (p < end) *p++ = '%';
                written++;
            }
        } else {
            if (p < end) *p++ = *format;
            written++;
        }
    }
    *p = '\0';
    return written;
}

// A simple implementation of snprintf
int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, size, format, args);
    va_end(args);
    return ret;
}

/* ===================================================================
 * Buffered FILE* I/O layer — wraps the POSIX fd calls from posix.c
 * =================================================================== */

#include "posix.h"
#include "fat16.h"
#include "mmu.h"
#include "kio.h"

/* ---- Standard streams ---- */
static toast_FILE stdin_stream  = { .fd = 0, .flags = 0, .eof = 0, .error = 0, .unget = -1, .mode = "r"  };
static toast_FILE stdout_stream = { .fd = 1, .flags = 1, .eof = 0, .error = 0, .unget = -1, .mode = "w"  };
static toast_FILE stderr_stream = { .fd = 2, .flags = 1, .eof = 0, .error = 0, .unget = -1, .mode = "w"  };

toast_FILE *toast_stdin_ptr  = &stdin_stream;
toast_FILE *toast_stdout_ptr = &stdout_stream;
toast_FILE *toast_stderr_ptr = &stderr_stream;

/* ---- Helpers to convert mode string to POSIX flags ---- */
static int mode_to_flags(const char *mode) {
    int flags = 0;
    if (mode[0] == 'r') {
        flags = O_RDONLY;
        if (mode[1] == '+') flags = O_RDWR;
    } else if (mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        if (mode[1] == '+') flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (mode[0] == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        if (mode[1] == '+') flags = O_RDWR | O_CREAT | O_APPEND;
    }
    return flags;
}

/* ---- fopen ---- */
toast_FILE *fopen(const char *path, const char *mode) {
    int flags = mode_to_flags(mode);
    int fd = posix_open(path, flags);
    if (fd < 0) return (toast_FILE *)0;

    toast_FILE *f = (toast_FILE *)kmalloc(sizeof(toast_FILE));
    if (!f) { posix_close(fd); return (toast_FILE *)0; }

    f->fd    = fd;
    f->flags = flags;
    f->eof   = 0;
    f->error = 0;
    f->unget = -1;
    int i;
    for (i = 0; i < 7 && mode[i]; i++) f->mode[i] = mode[i];
    f->mode[i] = '\0';

    return f;
}

/* ---- freopen ---- */
toast_FILE *freopen(const char *path, const char *mode, toast_FILE *stream) {
    if (!stream) return (toast_FILE *)0;
    posix_close(stream->fd);

    int flags = mode_to_flags(mode);
    int fd = posix_open(path, flags);
    if (fd < 0) return (toast_FILE *)0;

    stream->fd    = fd;
    stream->flags = flags;
    stream->eof   = 0;
    stream->error = 0;
    stream->unget = -1;
    return stream;
}

/* ---- fclose ---- */
int fclose(toast_FILE *stream) {
    if (!stream) return EOF;
    int ret = posix_close(stream->fd);
    if (stream != &stdin_stream && stream != &stdout_stream && stream != &stderr_stream)
        kfree(stream);
    return ret < 0 ? EOF : 0;
}

/* ---- fflush ---- */
int fflush(toast_FILE *stream) {
    (void)stream;
    return 0;
}

/* ---- fgetc / getc ---- */
int fgetc(toast_FILE *stream) {
    if (!stream || stream->eof) return EOF;

    if (stream->unget >= 0) {
        int c = stream->unget;
        stream->unget = -1;
        return c;
    }

    unsigned char c;
    ssize_t n = posix_read(stream->fd, &c, 1);
    if (n <= 0) {
        stream->eof = 1;
        return EOF;
    }
    return (int)c;
}

int getc(toast_FILE *stream) { return fgetc(stream); }
int getchar(void) { return fgetc(toast_stdin_ptr); }

/* ---- fputc / putc ---- */
int fputc(int c, toast_FILE *stream) {
    if (!stream) return EOF;
    unsigned char ch = (unsigned char)c;
    ssize_t n = posix_write(stream->fd, &ch, 1);
    if (n <= 0) { stream->error = 1; return EOF; }
    return (int)ch;
}

int putc(int c, toast_FILE *stream) { return fputc(c, stream); }
int putchar(int c) { return fputc(c, toast_stdout_ptr); }

/* ---- ungetc ---- */
int ungetc(int c, toast_FILE *stream) {
    if (!stream || c == EOF) return EOF;
    stream->unget = c;
    stream->eof = 0;
    return c;
}

/* ---- fgets ---- */
char *fgets(char *s, int size, toast_FILE *stream) {
    if (!s || size <= 0 || !stream) return (char *)0;

    int i = 0;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (i == 0) return (char *)0;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

/* ---- fputs ---- */
int fputs(const char *s, toast_FILE *stream) {
    if (!s || !stream) return EOF;
    while (*s) {
        if (fputc(*s, stream) == EOF) return EOF;
        s++;
    }
    return 0;
}

/* ---- puts ---- */
int puts(const char *s) {
    if (fputs(s, toast_stdout_ptr) == EOF) return EOF;
    fputc('\n', toast_stdout_ptr);
    return 0;
}

/* ---- fread ---- */
size_t fread(void *ptr, size_t size, size_t nmemb, toast_FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    ssize_t n = posix_read(stream->fd, ptr, total);
    if (n <= 0) { stream->eof = 1; return 0; }
    return (size_t)n / size;
}

/* ---- fwrite ---- */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, toast_FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    ssize_t n = posix_write(stream->fd, ptr, total);
    if (n <= 0) { stream->error = 1; return 0; }
    return (size_t)n / size;
}

/* ---- fseek / ftell / rewind ---- */
int fseek(toast_FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    stream->unget = -1;
    stream->eof = 0;
    off_t res = posix_lseek(stream->fd, (off_t)offset, whence);
    return (res == (off_t)-1) ? -1 : 0;
}

long ftell(toast_FILE *stream) {
    if (!stream) return -1;
    return (long)posix_lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(toast_FILE *stream) {
    if (!stream) return;
    fseek(stream, 0, SEEK_SET);
    stream->error = 0;
}

/* ---- feof / ferror / clearerr ---- */
int feof(toast_FILE *stream) { return stream ? stream->eof : 0; }
int ferror(toast_FILE *stream) { return stream ? stream->error : 0; }
void clearerr(toast_FILE *stream) {
    if (stream) { stream->eof = 0; stream->error = 0; }
}

/* ---- fprintf / vfprintf / printf ---- */
int vfprintf(toast_FILE *stream, const char *fmt, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) {
        size_t len = (size_t)n;
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        posix_write(stream->fd, buf, len);
    }
    return n;
}

int fprintf(toast_FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(stream, fmt, ap);
    va_end(ap);
    return ret;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(toast_stdout_ptr, fmt, ap);
    va_end(ap);
    return ret;
}

/* ---- sscanf (minimal: %d, %s, %c, %x, %u) ---- */
int sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int count = 0;
    const char *s = str;

    while (*fmt && *s) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                int *ip = va_arg(ap, int *);
                char *end;
                long val = strtol(s, &end, 10);
                if (end == s) break;
                *ip = (int)val;
                s = end; count++;
            } else if (*fmt == 'u') {
                unsigned int *up = va_arg(ap, unsigned int *);
                char *end;
                unsigned long val = strtoul(s, &end, 10);
                if (end == s) break;
                *up = (unsigned int)val;
                s = end; count++;
            } else if (*fmt == 'x') {
                unsigned int *xp = va_arg(ap, unsigned int *);
                char *end;
                unsigned long val = strtoul(s, &end, 16);
                if (end == s) break;
                *xp = (unsigned int)val;
                s = end; count++;
            } else if (*fmt == 's') {
                char *sp = va_arg(ap, char *);
                while (*s && *s != ' ' && *s != '\t' && *s != '\n')
                    *sp++ = *s++;
                *sp = '\0'; count++;
            } else if (*fmt == 'c') {
                char *cp = va_arg(ap, char *);
                *cp = *s++; count++;
            }
            fmt++;
        } else if (*fmt == ' ') {
            while (*s == ' ' || *s == '\t') s++;
            fmt++;
        } else {
            if (*fmt != *s) break;
            fmt++; s++;
        }
    }
    va_end(ap);
    return count;
}

/* ---- remove ---- */
int remove(const char *path) {
    return fat16_delete_at(path);
}

/* ---- perror ---- */
void perror(const char *s) {
    if (s && *s) {
        kprint(s);
        kprint(": ");
    }
    kprint(strerror(errno));
    kprint_newline();
}

/* ---- fileno ---- */
int fileno(toast_FILE *stream) {
    return stream ? stream->fd : -1;
}
