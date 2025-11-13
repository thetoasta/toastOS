/*
 * toastOS User Application API Implementation
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

// Forward declarations - API functions user apps will call
void _api_print(const char* str);
void _api_print_color(const char* str, unsigned char color);
void _api_print_int(int num);
void _api_newline(void);
char* _api_input(void);
void _api_clear(void);
void _api_fs_write(const char* filename, const char* content);
void _api_fs_read(const char* file_id);
void _api_fs_list(void);
void _api_fs_delete(const char* file_id);
void _api_time_get(void);
void _api_date_get(void);
unsigned char _api_read_hours(void);
unsigned char _api_read_minutes(void);
unsigned char _api_read_seconds(void);
const char* _api_registry_get(const char* key);
void _api_registry_set(const char* key, const char* value);
int _api_strcmp(const char* s1, const char* s2);
int _api_strncmp(const char* s1, const char* s2, unsigned int n);
unsigned int _api_strlen(const char* s);
char* _api_strncpy(char* dest, const char* src, unsigned int n);

// Forward declarations - Kernel functions we'll call
extern void kprint(char* str);
extern void toast_shell_color(char* text, unsigned char color);
extern char* rec_input(void);
extern void clear_screen(void);
extern void local_fs(char* file_name, char* file_contents);
extern void read_local_fs(char* file_id);
extern void list_files(void);
extern void delete_file(char* file_id);
extern void rtc_print_time(void);
extern void rtc_print_date(void);
extern unsigned char rtc_read_hours(void);
extern unsigned char rtc_read_minutes(void);
extern unsigned char rtc_read_seconds(void);
extern const char* registry_get(char* key);
extern int registry_set(char* key, char* value);

// String functions
static int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int my_strncmp(const char* s1, const char* s2, unsigned int n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static unsigned int my_strlen(const char* s) {
    unsigned int len = 0;
    while (s[len]) len++;
    return len;
}

static char* my_strncpy(char* dest, const char* src, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

/* ============================================
   CONSOLE I/O IMPLEMENTATION
   ============================================ */

void _api_print(const char* str) {
    kprint((char*)str);
}

void _api_print_color(const char* str, unsigned char color) {
    toast_shell_color((char*)str, color);
}

void _api_print_int(int num) {
    char buffer[12];
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    if (num == 0) {
        buffer[i++] = '0';
    } else {
        while (num > 0) {
            buffer[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    
    if (is_negative) {
        buffer[i++] = '-';
    }
    
    buffer[i] = '\0';
    
    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    
    kprint(buffer);
}

void _api_newline(void) {
    kprint("\n");
}

char* _api_input(void) {
    return rec_input();
}

void _api_clear(void) {
    clear_screen();
}

/* ============================================
   FILESYSTEM IMPLEMENTATION
   ============================================ */

void _api_fs_write(const char* filename, const char* content) {
    local_fs((char*)filename, (char*)content);
}

void _api_fs_read(const char* file_id) {
    read_local_fs((char*)file_id);
}

void _api_fs_list(void) {
    list_files();
}

void _api_fs_delete(const char* file_id) {
    delete_file((char*)file_id);
}

/* ============================================
   TIME/DATE IMPLEMENTATION
   ============================================ */

void _api_time_get(void) {
    rtc_print_time();
}

void _api_date_get(void) {
    rtc_print_date();
}

unsigned char _api_read_hours(void) {
    return rtc_read_hours();
}

unsigned char _api_read_minutes(void) {
    return rtc_read_minutes();
}

unsigned char _api_read_seconds(void) {
    return rtc_read_seconds();
}

/* ============================================
   REGISTRY IMPLEMENTATION
   ============================================ */

const char* _api_registry_get(const char* key) {
    return registry_get((char*)key);
}

void _api_registry_set(const char* key, const char* value) {
    registry_set((char*)key, (char*)value);
}

/* ============================================
   STRING UTILITIES IMPLEMENTATION
   ============================================ */

int _api_strcmp(const char* s1, const char* s2) {
    return my_strcmp(s1, s2);
}

int _api_strncmp(const char* s1, const char* s2, unsigned int n) {
    return my_strncmp(s1, s2, n);
}

unsigned int _api_strlen(const char* s) {
    return my_strlen(s);
}

char* _api_strncpy(char* dest, const char* src, unsigned int n) {
    return my_strncpy(dest, src, n);
}
