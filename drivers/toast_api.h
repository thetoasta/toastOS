/*
 * toastOS User Application API
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * 
 * This header provides a clean API for user applications.
 * Import this in your apps to access system functions.
 */

#ifndef TOAST_API_H
#define TOAST_API_H

/* ============================================
   CONSOLE I/O API
   ============================================ */

// Print a string to console
#define console_print(str) _api_print(str)

// Print a string with color
#define console_print_color(str, color) _api_print_color(str, color)

// Print an integer
#define console_print_int(num) _api_print_int(num)

// Print a newline
#define console_newline() _api_newline()

// Get user input
#define console_input() _api_input()

// Clear the screen
#define console_clear() _api_clear()

/* ============================================
   FILESYSTEM API
   ============================================ */

// Write a file to disk
#define fs_write(filename, content) _api_fs_write(filename, content)

// Read a file from disk
#define fs_read(file_id) _api_fs_read(file_id)

// List all files
#define fs_list() _api_fs_list()

// Delete a file
#define fs_delete(file_id) _api_fs_delete(file_id)

/* ============================================
   TIME/DATE API
   ============================================ */

// Get current time as string
#define time_get_current() _api_time_get()

// Get current date as string
#define date_get_current() _api_date_get()

// Read hours (0-23)
#define time_read_hours() _api_read_hours()

// Read minutes (0-59)
#define time_read_minutes() _api_read_minutes()

// Read seconds (0-59)
#define time_read_seconds() _api_read_seconds()

/* ============================================
   REGISTRY API (System Configuration)
   ============================================ */

// Get a registry value
#define registry_get(key) _api_registry_get(key)

// Set a registry value (requires auth)
#define registry_set(key, value) _api_registry_set(key, value)

/* ============================================
   STRING UTILITIES
   ============================================ */

// Compare two strings
#define string_compare(s1, s2) _api_strcmp(s1, s2)

// Compare N characters
#define string_compare_n(s1, s2, n) _api_strncmp(s1, s2, n)

// Get string length
#define string_length(s) _api_strlen(s)

// Copy string
#define string_copy(dest, src, n) _api_strncpy(dest, src, n)

/* ============================================
   COLOR CONSTANTS
   ============================================ */

#define COLOR_BLACK 0x0
#define COLOR_BLUE 0x1
#define COLOR_GREEN 0x2
#define COLOR_CYAN 0x3
#define COLOR_RED 0x4
#define COLOR_MAGENTA 0x5
#define COLOR_BROWN 0x6
#define COLOR_LIGHT_GREY 0x7
#define COLOR_DARK_GREY 0x8
#define COLOR_LIGHT_BLUE 0x9
#define COLOR_LIGHT_GREEN 0xA
#define COLOR_LIGHT_CYAN 0xB
#define COLOR_LIGHT_RED 0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW 0xE
#define COLOR_WHITE 0xF

/* ============================================
   INTERNAL API (DO NOT CALL DIRECTLY)
   ============================================ */

// These are implemented in the kernel - users should use the macros above
extern void _api_print(const char* str);
extern void _api_print_color(const char* str, unsigned char color);
extern void _api_print_int(int num);
extern void _api_newline(void);
extern char* _api_input(void);
extern void _api_clear(void);
extern void _api_fs_write(const char* filename, const char* content);
extern void _api_fs_read(const char* file_id);
extern void _api_fs_list(void);
extern void _api_fs_delete(const char* file_id);
extern void _api_time_get(void);
extern void _api_date_get(void);
extern unsigned char _api_read_hours(void);
extern unsigned char _api_read_minutes(void);
extern unsigned char _api_read_seconds(void);
extern const char* _api_registry_get(const char* key);
extern void _api_registry_set(const char* key, const char* value);
extern int _api_strcmp(const char* s1, const char* s2);
extern int _api_strncmp(const char* s1, const char* s2, unsigned int n);
extern unsigned int _api_strlen(const char* s);
extern char* _api_strncpy(char* dest, const char* src, unsigned int n);

#endif // TOAST_API_H
