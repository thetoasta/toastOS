/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#ifndef APPS_H
#define APPS_H

#define MAX_APPS 16
#define MAX_APP_NAME 32

// Application function pointer type
typedef void (*app_func_t)(void);

// Application structure
typedef struct {
    char name[MAX_APP_NAME];
    app_func_t func;
    int enabled;
} app_entry_t;

// Initialize application system
void apps_init(void);

// Register a new application
int app_register(const char* name, app_func_t func);

// Check if an app exists
int app_exists(const char* name);

// Run an application by name
int app_run(const char* name);

// List all registered applications
void app_list(void);

#endif // APPS_H
