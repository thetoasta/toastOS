/*
 * toastOS Application Runtime System
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#include "apps.h"
#include "toast_api.h"
#include "kio.h"
#include "file.h"
#include "string.h"
#include "rtc.h"

// Include user-created apps here
#include "user_apps/example_echo.c"

#define SERIAL_PORT 0x3F8

// Serial helper functions
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline int serial_is_transmit_empty(void) {
    return inb(SERIAL_PORT + 5) & 0x20;
}

static inline void serial_write_char(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(SERIAL_PORT, c);
}

static inline void serial_write_string(const char *str) {
    while (*str) {
        serial_write_char(*str++);
    }
}

static inline void kprint_int(int n) {
    extern void kprint(const char *str);
    if (n < 0) {
        kprint("-");
        n = -n;
    }
    if (n / 10) {
        kprint_int(n / 10);
    }
    char c = '0' + (n % 10);
    char str[2] = {c, '\0'};
    kprint(str);
}

static app_entry_t app_registry[MAX_APPS];
static int app_count = 0;

// Built-in applications

// Simple calculator app
static void app_calculator(void) {
    toast_shell_color("=== Calculator ===", LIGHT_CYAN);
    kprint_newline();
    kprint("Enter first number: ");
    char* num1_str = rec_input();
    
    kprint("Enter operation (+, -, *, /): ");
    char* op = rec_input();
    
    kprint("Enter second number: ");
    char* num2_str = rec_input();
    
    // Simple string to int conversion
    int num1 = 0, num2 = 0;
    for (int i = 0; num1_str[i] >= '0' && num1_str[i] <= '9'; i++) {
        num1 = num1 * 10 + (num1_str[i] - '0');
    }
    for (int i = 0; num2_str[i] >= '0' && num2_str[i] <= '9'; i++) {
        num2 = num2 * 10 + (num2_str[i] - '0');
    }
    
    int result = 0;
    if (op[0] == '+') result = num1 + num2;
    else if (op[0] == '-') result = num1 - num2;
    else if (op[0] == '*') result = num1 * num2;
    else if (op[0] == '/' && num2 != 0) result = num1 / num2;
    else {
        toast_shell_color("Invalid operation!", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    kprint_newline();
    toast_shell_color("Result: ", LIGHT_GREEN);
    kprint_int(result);
    kprint_newline();
}

// Todo list app
static void app_todo(void) {
    toast_shell_color("=== Todo List ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    kprint("1. View todos");
    kprint_newline();
    kprint("2. Add todo");
    kprint_newline();
    kprint_newline();
    kprint("Choose option: ");
    char* choice = rec_input();
    
    if (choice[0] == '1') {
        kprint_newline();
        read_local_fs("todos.txt-1");
    } else if (choice[0] == '2') {
        kprint_newline();
        kprint("Enter todo item: ");
        char* todo = rec_input();
        
        // Append to todos file
        local_fs("todos.txt", todo);
        toast_shell_color("Todo added!", LIGHT_GREEN);
        kprint_newline();
    } else {
        toast_shell_color("Invalid choice!", LIGHT_RED);
        kprint_newline();
    }
}

// System info app
static void app_sysinfo(void) {
    toast_shell_color("=== toastOS System Information ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("OS Name: ", LIGHT_GREEN);
    kprint("toastOS v1.1");
    kprint_newline();
    
    toast_shell_color("Kernel: ", LIGHT_GREEN);
    kprint("toastKernel");
    kprint_newline();
    
    toast_shell_color("Architecture: ", LIGHT_GREEN);
    kprint("x86 32-bit");
    kprint_newline();
    
    toast_shell_color("Filesystem: ", LIGHT_GREEN);
    kprint("toastFS v1.0");
    kprint_newline();
    
    toast_shell_color("Bootloader: ", LIGHT_GREEN);
    kprint("Multiboot");
    kprint_newline();
    
    toast_shell_color("Current Time: ", LIGHT_GREEN);
    rtc_print_time();
    kprint_newline();
    
    toast_shell_color("Current Date: ", LIGHT_GREEN);
    rtc_print_date();
    kprint_newline();
    
    kprint_newline();
    toast_shell_color("Files on disk: ", YELLOW);
    kprint_newline();
    list_files();
}

// Stopwatch app
static void app_stopwatch(void) {
    toast_shell_color("=== Stopwatch ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    uint8_t start_sec = rtc_read_seconds();
    uint8_t start_min = rtc_read_minutes();
    
    toast_shell_color("Stopwatch started! Press ENTER to stop.", LIGHT_GREEN);
    kprint_newline();
    
    rec_input();  // Wait for user to press enter
    
    uint8_t end_sec = rtc_read_seconds();
    uint8_t end_min = rtc_read_minutes();
    
    int elapsed_sec = end_sec - start_sec;
    int elapsed_min = end_min - start_min;
    
    if (elapsed_sec < 0) {
        elapsed_sec += 60;
        elapsed_min -= 1;
    }
    
    kprint_newline();
    toast_shell_color("Elapsed time: ", LIGHT_CYAN);
    kprint_int(elapsed_min);
    kprint(" minutes, ");
    kprint_int(elapsed_sec);
    kprint(" seconds");
    kprint_newline();
}

// Note-taking app
static void app_notes(void) {
    toast_shell_color("=== Quick Notes ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    kprint("Enter note title: ");
    char* title = rec_input();
    
    kprint("Enter note content: ");
    char* content = rec_input();
    
    // Save note with timestamp
    local_fs(title, content);
    
    kprint_newline();
    toast_shell_color("Note saved!", LIGHT_GREEN);
    kprint_newline();
}

// Initialize application system
void apps_init(void) {
    serial_write_string("[APPS] Initializing application system...\n");
    
    app_count = 0;
    for (int i = 0; i < MAX_APPS; i++) {
        app_registry[i].name[0] = '\0';
        app_registry[i].func = 0;
        app_registry[i].enabled = 0;
    }
    
    // Register built-in apps
    app_register("calculator", app_calculator);
    app_register("calc", app_calculator);
    app_register("todo", app_todo);
    app_register("sysinfo", app_sysinfo);
    app_register("info", app_sysinfo);
    app_register("stopwatch", app_stopwatch);
    app_register("timer", app_stopwatch);
    app_register("notes", app_notes);
    app_register("note", app_notes);
    
    // Register user-created apps (from user_apps/)
    app_register("echo", app_echo);
    
    // Add your custom apps here:
    // app_register("myapp", app_myapp);
    
    serial_write_string("[APPS] Application system initialized\n");
    serial_write_string("[APPS] Registered ");
    serial_write_string(" built-in applications\n");
}

// Register a new application
int app_register(const char* name, app_func_t func) {
    if (app_count >= MAX_APPS) {
        serial_write_string("[APPS] ERROR: Maximum apps reached\n");
        return -1;
    }
    
    strncpy(app_registry[app_count].name, name, MAX_APP_NAME - 1);
    app_registry[app_count].func = func;
    app_registry[app_count].enabled = 1;
    app_count++;
    
    return 0;
}

// Check if an app exists by name
int app_exists(const char* name) {
    for (int i = 0; i < app_count; i++) {
        if (strcmp(app_registry[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Run an application by name
int app_run(const char* name) {
    serial_write_string("[APPS] Running application: ");
    serial_write_string(name);
    serial_write_string("\n");
    
    for (int i = 0; i < app_count; i++) {
        if (strcmp(app_registry[i].name, name) == 0) {
            if (!app_registry[i].enabled) {
                toast_shell_color("App is disabled!", LIGHT_RED);
                kprint_newline();
                return -1;
            }
            
            kprint_newline();
            app_registry[i].func();
            kprint_newline();
            
            serial_write_string("[APPS] Application completed\n");
            return 0;
        }
    }
    
    toast_shell_color("App not found: ", LIGHT_RED);
    kprint(name);
    kprint_newline();
    toast_shell_color("Type 'apps' to see available applications", YELLOW);
    kprint_newline();
    
    return -1;
}

// List all registered applications
void app_list(void) {
    toast_shell_color("=== Available Applications ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    if (app_count == 0) {
        kprint("No applications registered.");
        kprint_newline();
        return;
    }
    
    for (int i = 0; i < app_count; i++) {
        if (app_registry[i].enabled) {
            toast_shell_color("  - ", LIGHT_GREEN);
            kprint(app_registry[i].name);
            kprint_newline();
        }
    }
    
    kprint_newline();
    toast_shell_color("Run an app with: ", YELLOW);
    toast_shell_color("run <app-name>", LIGHT_CYAN);
    kprint_newline();
}
