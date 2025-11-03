#ifndef STRING_H
#define STRING_H

#include <stddef.h> // for size_t

int strcmp(const char* s1, const char* s2);
char* strncpy(char* dest, const char* src, size_t n);
size_t strlen(const char* s);

#endif // STRING_H