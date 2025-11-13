/*
 * toastOS App Builder - Self-hosting IDE
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * 
 * This allows users to write apps INSIDE toastOS and run them immediately!
 * It's like having a mini C IDE built into the OS.
 */

#include "app_builder.h"
#include "toast_api.h"

// Simple C interpreter - executes basic app structure
// For a full implementation, this would need a real interpreter/JIT
// For now, we'll create a template-based system

#define MAX_USER_APPS 8
#define MAX_APP_CODE 2048

typedef struct {
    char name[32];
    char code[MAX_APP_CODE];
    void (*func)(void);
    int active;
} user_app_t;

static user_app_t user_apps[MAX_USER_APPS];
static int user_app_count = 0;

// Forward declarations
extern void kprint(char* str);
extern void toast_shell_color(char* text, unsigned char color);
extern char* rec_input(void);
extern void clear_screen(void);
extern void local_fs(char* file_name, char* file_contents);
extern void read_local_fs(char* file_id);
extern int app_register(const char* name, void (*func)(void));

// Helper to print int
static void print_int(int num) {
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
    
    // Reverse
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    
    kprint(buffer);
}

// String compare
static int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// String copy
static void my_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Initialize app builder
void app_builder_init(void) {
    user_app_count = 0;
    for (int i = 0; i < MAX_USER_APPS; i++) {
        user_apps[i].name[0] = '\0';
        user_apps[i].code[0] = '\0';
        user_apps[i].func = 0;
        user_apps[i].active = 0;
    }
}

// Dynamic app executor - runs user-created apps
static void execute_user_app(int app_id) {
    if (app_id < 0 || app_id >= MAX_USER_APPS || !user_apps[app_id].active) {
        toast_shell_color("Error: Invalid app ID\n", 0xC);
        return;
    }
    
    user_app_t* app = &user_apps[app_id];
    
    // For now, we'll interpret simple commands
    // In a full implementation, this would be a real interpreter
    
    clear_screen();
    toast_shell_color("=== ", 0xB);
    toast_shell_color(app->name, 0xE);
    toast_shell_color(" ===\n", 0xB);
    kprint("Running user app...\n\n");
    
    // Simple command interpreter
    char* code = app->code;
    char line[256];
    int line_idx = 0;
    
    while (*code) {
        // Read line
        line_idx = 0;
        while (*code && *code != '\n' && line_idx < 255) {
            line[line_idx++] = *code++;
        }
        line[line_idx] = '\0';
        if (*code == '\n') code++;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '/' || line[0] == '#') continue;
        
        // Parse commands
        if (line[0] == 'p' && line[1] == 'r' && line[2] == 'i' && line[3] == 'n' && line[4] == 't') {
            // print "text"
            char* start = line;
            while (*start && *start != '"') start++;
            if (*start == '"') {
                start++;
                char* end = start;
                while (*end && *end != '"') end++;
                *end = '\0';
                kprint(start);
            }
        }
        else if (line[0] == 'c' && line[1] == 'o' && line[2] == 'l' && line[3] == 'o' && line[4] == 'r') {
            // color "text" red
            char* start = line;
            while (*start && *start != '"') start++;
            if (*start == '"') {
                start++;
                char* end = start;
                while (*end && *end != '"') end++;
                *end = '\0';
                
                // Determine color (simplified)
                unsigned char color = 0xF; // white default
                if (line[line_idx - 3] == 'r' && line[line_idx - 2] == 'e' && line[line_idx - 1] == 'd')
                    color = 0xC;
                else if (line[line_idx - 4] == 'b' && line[line_idx - 3] == 'l' && line[line_idx - 2] == 'u' && line[line_idx - 1] == 'e')
                    color = 0x9;
                else if (line[line_idx - 5] == 'g' && line[line_idx - 4] == 'r' && line[line_idx - 3] == 'e' && line[line_idx - 2] == 'e' && line[line_idx - 1] == 'n')
                    color = 0xA;
                else if (line[line_idx - 6] == 'y' && line[line_idx - 5] == 'e' && line[line_idx - 4] == 'l' && line[line_idx - 3] == 'l' && line[line_idx - 2] == 'o' && line[line_idx - 1] == 'w')
                    color = 0xE;
                
                toast_shell_color(start, color);
            }
        }
        else if (line[0] == 'i' && line[1] == 'n' && line[2] == 'p' && line[3] == 'u' && line[4] == 't') {
            // input
            rec_input();
        }
        else if (line[0] == 'c' && line[1] == 'l' && line[2] == 'e' && line[3] == 'a' && line[4] == 'r') {
            // clear
            clear_screen();
        }
    }
    
    kprint("\n\n");
    toast_shell_color("[App finished. Press Enter]", 0x8);
    rec_input();
}

// Public wrapper for shell command
void execute_user_app_by_id(int app_id) {
    execute_user_app(app_id);
}

// Build app from source
int build_app_from_source(const char* app_name, const char* source_code) {
    if (user_app_count >= MAX_USER_APPS) {
        return -1; // No space
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_USER_APPS; i++) {
        if (!user_apps[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return -1;
    
    // Store app
    my_strcpy(user_apps[slot].name, app_name);
    my_strcpy(user_apps[slot].code, source_code);
    user_apps[slot].active = 1;
    user_app_count++;
    
    // Save to disk
    char filename[64];
    my_strcpy(filename, "app_");
    char* fn = filename + 4;
    my_strcpy(fn, app_name);
    fn = filename;
    while (*fn) fn++;
    my_strcpy(fn, ".tapp");
    
    local_fs(filename, (char*)source_code);
    
    return slot;
}

// List user apps
void list_user_apps(void) {
    toast_shell_color("\n=== User-Created Apps ===\n", 0xB);
    
    if (user_app_count == 0) {
        toast_shell_color("No user apps yet. Use 'makeapp' to create one!\n", 0x8);
        return;
    }
    
    for (int i = 0; i < MAX_USER_APPS; i++) {
        if (user_apps[i].active) {
            toast_shell_color("  [", 0x7);
            print_int(i);
            toast_shell_color("] ", 0x7);
            toast_shell_color(user_apps[i].name, 0xE);
            kprint("\n");
        }
    }
    kprint("\n");
}

// Main app builder interface
void app_builder(void) {
    clear_screen();
    toast_shell_color("==========================================\n", 0xB);
    toast_shell_color("     toastOS App Builder v1.0\n", 0xE);
    toast_shell_color("   Write apps directly in toastOS!\n", 0xA);
    toast_shell_color("==========================================\n", 0xB);
    kprint("\n");
    
    toast_shell_color("What would you like to do?\n", 0xF);
    kprint("  1. Create new app\n");
    kprint("  2. Run user app\n");
    kprint("  3. List user apps\n");
    kprint("  4. View tutorial\n");
    kprint("  5. Exit\n\n");
    
    toast_shell_color("Choice: ", 0xE);
    char* choice = rec_input();
    
    if (my_strcmp(choice, "1") == 0) {
        // Create new app
        clear_screen();
        toast_shell_color("=== Create New App ===\n\n", 0xB);
        
        toast_shell_color("App name: ", 0xE);
        char* app_name = rec_input();
        
        kprint("\n");
        toast_shell_color("Enter your app code (simple script language):\n", 0xA);
        toast_shell_color("Commands: print \"text\", color \"text\" red, input, clear\n", 0x8);
        toast_shell_color("Type 'done' on a new line when finished.\n\n", 0x8);
        
        char code[MAX_APP_CODE];
        int code_len = 0;
        
        while (1) {
            toast_shell_color("> ", 0x7);
            char* line = rec_input();
            
            if (my_strcmp(line, "done") == 0) break;
            
            // Add line to code
            char* l = line;
            while (*l && code_len < MAX_APP_CODE - 2) {
                code[code_len++] = *l++;
            }
            code[code_len++] = '\n';
        }
        code[code_len] = '\0';
        
        // Build app
        int app_id = build_app_from_source(app_name, code);
        
        if (app_id >= 0) {
            kprint("\n");
            toast_shell_color("[SUCCESS] App '", 0xA);
            toast_shell_color(app_name, 0xE);
            toast_shell_color("' created successfully!\n", 0xA);
            toast_shell_color("Run it with: runapp ", 0x7);
            print_int(app_id);
            kprint("\n");
        } else {
            toast_shell_color("\n[ERROR] Failed to create app (no space)\n", 0xC);
        }
        
        kprint("\n");
        toast_shell_color("[Press Enter]", 0x8);
        rec_input();
    }
    else if (my_strcmp(choice, "2") == 0) {
        // Run user app
        kprint("\n");
        list_user_apps();
        
        if (user_app_count > 0) {
            toast_shell_color("Enter app ID to run: ", 0xE);
            char* id_str = rec_input();
            
            // Convert to int
            int app_id = 0;
            for (int i = 0; id_str[i] >= '0' && id_str[i] <= '9'; i++) {
                app_id = app_id * 10 + (id_str[i] - '0');
            }
            
            execute_user_app(app_id);
        }
    }
    else if (my_strcmp(choice, "3") == 0) {
        // List apps
        list_user_apps();
        toast_shell_color("[Press Enter]", 0x8);
        rec_input();
    }
    else if (my_strcmp(choice, "4") == 0) {
        // Tutorial
        clear_screen();
        toast_shell_color("=== App Builder Tutorial ===\n\n", 0xB);
        
        toast_shell_color("toastOS App Builder lets you write simple apps!\n\n", 0xF);
        
        toast_shell_color("Available Commands:\n", 0xE);
        kprint("  print \"text\"         - Print text\n");
        kprint("  color \"text\" red     - Print colored text\n");
        kprint("  input                - Get user input\n");
        kprint("  clear                - Clear screen\n\n");
        
        toast_shell_color("Example App:\n", 0xA);
        kprint("  print \"Hello, World!\\n\"\n");
        kprint("  color \"Welcome to my app!\\n\" green\n");
        kprint("  print \"Enter your name: \"\n");
        kprint("  input\n");
        kprint("  print \"Nice to meet you!\\n\"\n\n");
        
        toast_shell_color("Colors: red, blue, green, yellow\n\n", 0x8);
        
        toast_shell_color("[Press Enter]", 0x8);
        rec_input();
    }
}
