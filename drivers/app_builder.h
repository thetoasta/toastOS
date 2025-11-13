/*
 * toastOS App Builder - Self-hosting IDE
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * 
 * Allows users to write and "compile" apps directly in toastOS
 */

#ifndef APP_BUILDER_H
#define APP_BUILDER_H

// Initialize the app builder system
void app_builder_init(void);

// Main app builder interface
void app_builder(void);

// Build and register an app from source
int build_app_from_source(const char* app_name, const char* source_code);

// List all user-created apps
void list_user_apps(void);

// Execute user app by ID (for shell command)
void execute_user_app_by_id(int app_id);

#endif // APP_BUILDER_H
