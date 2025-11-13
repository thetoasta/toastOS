/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#include "editor.h"
#include "kio.h"
#include "funcs.h"
#include "string.h"
#include "file.h"
#include "timer.h"

static char editor_buffer[MAX_FILE_SIZE];
static char editor_filename[MAX_FILENAME];
static int editor_size = 0;
static int editor_active = 0;
static int editor_modified = 0;

// Display editor help
static void editor_show_help(void) {
    toast_shell_color("=== toastOS Text Editor ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    toast_shell_color("Commands:", YELLOW);
    kprint_newline();
    kprint("  :w      - Save file");
    kprint_newline();
    kprint("  :q      - Quit editor (without saving)");
    kprint_newline();
    kprint("  :wq     - Save and quit");
    kprint_newline();
    kprint("  :help   - Show this help");
    kprint_newline();
    kprint("  :clear  - Clear buffer");
    kprint_newline();
    kprint("  :view   - View current content");
    kprint_newline();
    kprint_newline();
    toast_shell_color("Type your text and press ENTER to add lines.", LIGHT_GREEN);
    kprint_newline();
    toast_shell_color("Start a line with ':' to use commands.", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
}

// Display current buffer content
static void editor_view_content(void) {
    if (editor_size == 0) {
        toast_shell_color("[Empty buffer]", DARK_GREY);
        kprint_newline();
        return;
    }
    
    toast_shell_color("--- Content Start ---", LIGHT_CYAN);
    kprint_newline();
    for (int i = 0; i < editor_size; i++) {
        char c[2] = {editor_buffer[i], '\0'};
        kprint(c);
    }
    kprint_newline();
    toast_shell_color("--- Content End ---", LIGHT_CYAN);
    kprint_newline();
    toast_shell_color("Size: ", DARK_GREY);
    kprint_int(editor_size);
    kprint(" / ");
    kprint_int(MAX_FILE_SIZE);
    kprint(" bytes");
    kprint_newline();
}

// Save file to disk
static void editor_save(void) {
    serial_write_string("[EDITOR] Saving file: ");
    serial_write_string(editor_filename);
    serial_write_string(" (");
    kprint_int(editor_size);
    serial_write_string(" bytes)\n");
    
    editor_buffer[editor_size] = '\0';
    
    // Use the filesystem to write
    local_fs(editor_filename, editor_buffer);
    
    toast_shell_color("File saved: ", LIGHT_GREEN);
    kprint(editor_filename);
    kprint(" (");
    kprint_int(editor_size);
    kprint(" bytes)");
    kprint_newline();
    
    serial_write_string("[EDITOR] Save complete\n");
}

// Main editor loop
static void editor_loop(void) {
    char line[256];
    int running = 1;
    
    editor_active = 1;  // Mark editor as active
    editor_modified = 0;  // Start with no modifications
    
    while (running) {
        toast_shell_color("> ", LIGHT_CYAN);
        char* input = rec_input();
        
        serial_write_string("[EDITOR] Input: ");
        serial_write_string(input);
        serial_write_string("\n");
        
        // Check for commands (start with :)
        if (input[0] == ':') {
            if (strcmp(input, ":w") == 0) {
                editor_save();
            } else if (strcmp(input, ":q") == 0) {
                toast_shell_color("Exiting editor (unsaved changes will be lost)", YELLOW);
                kprint_newline();
                serial_write_string("[EDITOR] Quit without saving\n");
                running = 0;
            } else if (strcmp(input, ":wq") == 0) {
                editor_save();
                toast_shell_color("Exiting editor", LIGHT_GREEN);
                kprint_newline();
                serial_write_string("[EDITOR] Save and quit\n");
                running = 0;
            } else if (strcmp(input, ":help") == 0) {
                editor_show_help();
            } else if (strcmp(input, ":clear") == 0) {
                editor_size = 0;
                toast_shell_color("Buffer cleared", YELLOW);
                kprint_newline();
                serial_write_string("[EDITOR] Buffer cleared\n");
            } else if (strcmp(input, ":view") == 0) {
                editor_view_content();
            } else {
                toast_shell_color("Unknown command. Type :help for help.", LIGHT_RED);
                kprint_newline();
            }
        } else {
            // Add line to buffer
            int len = strlen(input);
            
            // Check if there's space
            if (editor_size + len + 1 >= MAX_FILE_SIZE) {
                toast_shell_color("ERROR: Buffer full! Save or clear to continue.", LIGHT_RED);
                kprint_newline();
                serial_write_string("[EDITOR] Buffer full!\n");
                continue;
            }
            
            // Copy input to buffer
            for (int i = 0; i < len; i++) {
                editor_buffer[editor_size++] = input[i];
            }
            editor_buffer[editor_size++] = '\n';
            
            editor_modified = 1;  // Mark as modified
            
            toast_shell_color("[Line added - ", DARK_GREY);
            kprint_int(editor_size);
            kprint(" bytes used]");
            kprint_newline();
        }
    }
    
    editor_active = 0;  // Mark editor as inactive when exiting
}

// Start editor with a new file
void editor_new(void) {
    serial_write_string("\n[EDITOR] Starting new file\n");
    
    toast_shell_color("=== toastOS Text Editor - New File ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    kprint("Enter filename: ");
    char* filename = rec_input();
    
    // Copy filename
    strncpy(editor_filename, filename, MAX_FILENAME - 1);
    editor_filename[MAX_FILENAME - 1] = '\0';
    
    serial_write_string("[EDITOR] Filename: ");
    serial_write_string(editor_filename);
    serial_write_string("\n");
    
    // Clear buffer
    editor_size = 0;
    for (int i = 0; i < MAX_FILE_SIZE; i++) {
        editor_buffer[i] = '\0';
    }
    
    kprint_newline();
    toast_shell_color("Creating new file: ", LIGHT_GREEN);
    kprint(editor_filename);
    kprint_newline();
    kprint_newline();
    editor_show_help();
    
    editor_loop();
    
    serial_write_string("[EDITOR] Session ended\n");
}

// Open existing file for editing
void editor_open(const char* filename) {
    serial_write_string("\n[EDITOR] Opening file: ");
    serial_write_string(filename);
    serial_write_string("\n");
    
    toast_shell_color("=== toastOS Text Editor - Open File ===", LIGHT_CYAN);
    kprint_newline();
    kprint_newline();
    
    // Copy filename
    strncpy(editor_filename, filename, MAX_FILENAME - 1);
    editor_filename[MAX_FILENAME - 1] = '\0';
    
    // Try to read existing file
    // For now, start with empty buffer
    // TODO: Load from filesystem when read is implemented
    editor_size = 0;
    for (int i = 0; i < MAX_FILE_SIZE; i++) {
        editor_buffer[i] = '\0';
    }
    
    toast_shell_color("Opening file: ", LIGHT_GREEN);
    kprint(filename);
    kprint_newline();
    toast_shell_color("(Note: File loading not yet implemented - starting with empty buffer)", YELLOW);
    kprint_newline();
    kprint_newline();
    editor_show_help();
    
    editor_loop();
    
    serial_write_string("[EDITOR] Session ended\n");
}

// Autosave callback (called by timer every ~10 seconds)
void editor_autosave_callback(void) {
    if (editor_active && editor_modified && editor_size > 0) {
        serial_write_string("[EDITOR] Autosaving...\n");
        
        // Save the file (no visual message to prevent screen corruption)
        editor_buffer[editor_size] = '\0';
        local_fs(editor_filename, editor_buffer);
        
        editor_modified = 0;
        serial_write_string("[EDITOR] Autosave complete\n");
    }
}

// Check if editor is active
int editor_is_active(void) {
    return editor_active;
}
