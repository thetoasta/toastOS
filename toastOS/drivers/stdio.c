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
