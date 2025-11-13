/*
 * Example User Application: Echo
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * 
 * This demonstrates how to write a user application for toastOS.
 * To add your own app:
 * 1. Include "toast_api.h"
 * 2. Use the API functions (console_print, fs_write, etc.)
 * 3. Register your app in apps.c using app_register()
 */

#include "toast_api.h"

void app_echo(void) {
    console_clear();
    console_print_color("=== Echo Application ===\n", COLOR_CYAN);
    console_print("Type something and press Enter. Type 'exit' to quit.\n\n");
    
    while (1) {
        console_print_color("> ", COLOR_YELLOW);
        char* input = console_input();
        
        // Check for exit
        if (string_compare(input, "exit") == 0) {
            console_print_color("\nExiting Echo app...\n", COLOR_GREEN);
            break;
        }
        
        // Echo the input
        console_print_color("Echo: ", COLOR_LIGHT_GREEN);
        console_print(input);
        console_newline();
    }
}
