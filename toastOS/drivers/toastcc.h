#ifndef TOASTCC_H
#define TOASTCC_H

/*
 * toastCC — sandboxed C interpreter for toastOS
 *
 * Write and run real C code inside toastOS. Programs run interpreted
 * in a sandbox with instruction and memory limits for safety.
 *
 * Supported C features:
 *   int, char, void, const types
 *   variables, arrays, functions (up to 8 params)
 *   if/else, while, for, break, continue, return
 *   arithmetic, bitwise, comparison, logical operators
 *   compound assignment (+=, -=, *=, /=, %=), ++/--
 *   string literals, character operations
 *
 * Built-in functions:
 *   printf(fmt, ...)    Formatted output (%d %s %c %x %u %%)
 *   puts(str)           Print string + newline
 *   putchar(c)          Print single character
 *   print(...)          Print values (no format string needed)
 *   println(...)        Print values + newline
 *   input()             Read a line of keyboard input (returns string)
 *   get_key()           Wait for a keypress (returns ASCII int)
 *   clear()             Clear the screen
 *   sleep(seconds)      Pause execution (max 30s)
 *   uptime()            Get system uptime in seconds
 *   read_file(name)     Read a FAT16 file (returns string)
 *   write_file(name,s)  Write string to a FAT16 file
 *   strlen(s)           String length
 *   strcmp(a, b)        Compare strings
 *   atoi(s)             String to integer
 *   itoa(n)             Integer to string
 *   abs(n)              Absolute value
 *   rand()              Random integer 0-32767
 *   srand(seed)         Seed the PRNG
 *   exit()              Stop the program
 *   sizeof(x)           Size of value (4 for int, strlen for string)
 *
 * Sandbox limits:
 *   500,000 instructions max per run
 *   64 KB heap allocation cap
 *   64-deep call stack limit
 *
 * Usage from shell:
 *   tcc <file.c>          Run a .c file directly
 *   tcc ide <file.c>      Open in editor with Ctrl+R to run
 */

void tcc_run_file(const char *filename);
void tcc_run_source(const char *source);
int  tcc_validate(const char *source, char *errmsg, int maxlen);

#endif
