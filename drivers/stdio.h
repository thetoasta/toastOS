#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stdint.h>

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

#endif /* STDIO_H */
