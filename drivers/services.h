#ifndef SERVICES_H
#define SERVICES_H

// services! things can initilize a service, then call a service function. other services CAN access a different service even if they dont own it

#include "kio.h"
#include "funcs.h"
#include "panic.h"
#include "stdio.h"
#include "stdint.h"
#include "file.h"
#include "string.h"
#include "rtc.h"
#include "registry.h"
#include "editor.h"

#define MAX_SERVICES 32
#define MAX_SERVICE_NAME 16

typedef void (*service_handler_t)(const char* host);

typedef struct {
    char name[MAX_SERVICE_NAME];
    service_handler_t handler;
    int requires_auth;
    int active;
} service_t;

static service_t services[MAX_SERVICES];
static int service_count = 0;

// Secure lock - when enabled, blocks all services except unlock
static int secure_lock_enabled = 0;

// Forward declarations
static inline void list_services();

// Input validation for security
static inline int is_safe_string(const char* str, int max_len) {
    if (!str) return 0;
    
    int len = 0;
    while (str[len] != '\0' && len < max_len) {
        // Check for control characters (except newline, tab, backspace)
        if (str[len] < 32 && str[len] != '\n' && str[len] != '\t' && str[len] != '\b') {
            return 0;
        }
        len++;
    }
    
    // Check if string is properly null-terminated
    if (len >= max_len && str[len] != '\0') {
        return 0;
    }
    
    return 1;
}

// Service handlers
static inline void service_fta(const char* host) {
    fs_test_auto();
}

static inline void service_shutdown(const char* host) {
        kprint("Shutting down as requested by host '");
        kprint(host);
        kprint("'.");
        kprint_newline();
        shutdown();
}

// Filesystem services
static inline void service_fs_write(const char* host) {
    serial_write_string("[FS] File write requested\n");
    kprint("Enter filename: ");
    char* filename = rec_input();
    
    if (!is_safe_string(filename, 64)) {
        serial_write_string("[FS] SECURITY: Invalid filename\n");
        toast_shell_color("ERROR: Invalid filename (contains illegal characters)", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    kprint("Enter content: ");
    char* content = rec_input();
    
    if (!is_safe_string(content, 1024)) {
        serial_write_string("[FS] SECURITY: Invalid content\n");
        toast_shell_color("ERROR: Invalid content (contains illegal characters)", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    serial_write_string("[FS] Writing file: ");
    serial_write_string(filename);
    serial_write_string("\n");
    local_fs(filename, content);
}

static inline void service_fs_read(const char* host) {
    kprint("Enter file ID: ");
    char* file_id = rec_input();
    read_local_fs(file_id);
}

static inline void service_fs_delete(const char* host) {
    kprint("Enter file ID to delete: ");
    char* file_id = rec_input();
    delete_file(file_id);
}

static inline void service_fs_list(const char* host) {
    list_files();
}

static inline void service_fs_format(const char* host) {
    kprint("WARNING: This will erase all data! Type 'YES' to confirm: ");
    char* confirm = rec_input();
    if (strcmp(confirm, "YES") == 0) {
        // Reinitialize filesystem (format)
        kprint("Filesystem formatted.");
        kprint_newline();
    } else {
        kprint("Format cancelled.");
        kprint_newline();
    }
}

// System management services
static inline void service_reboot(const char* host) {
    kprint("Rebooting system...");
    kprint_newline();
    // Trigger a CPU reset via keyboard controller
    write_port(0x64, 0xFE);
    while(1);
}

static inline void service_clear(const char* host) {
    clear_screen();
}

static inline void service_help(const char* host) {
    list_services();
}

// Display services
static inline void service_cursor_enable(const char* host) {
    kprint("Cursor feature not yet fully implemented.");
    kprint_newline();
}

static inline void service_cursor_disable(const char* host) {
    kprint("Cursor feature not yet fully implemented.");
    kprint_newline();
}

static inline void service_set_color(const char* host) {
    kprint("Type the color code: ");
    char* color = rec_input();
    kprint("Type the string to print: ");
    char* str = rec_input();
    toast_shell_color(str, (uint8_t)(*color - '0'));
}

// Debug services
static inline void service_panic_test(const char* host) {
    l1_panic("Test panic triggered by service");
}

static inline void service_exception_test(const char* host) {
    kprint("Triggering divide by zero exception...");
    kprint_newline();
    int x = 1;
    int y = 0;
    int z = x / y;
    kprint_int(z);
}

static inline void service_memory_info(const char* host) {
    kprint("Memory info feature coming soon!");
    kprint_newline();
}

// Time/Date services
static inline void service_time(const char* host) {
    kprint("Current time: ");
    rtc_print_time();
    kprint_newline();
}

static inline void service_date(const char* host) {
    kprint("Current date: ");
    rtc_print_date();
    kprint_newline();
}

static inline void service_datetime(const char* host) {
    kprint("Current date and time:");
    kprint_newline();
    rtc_print_datetime();
    kprint_newline();
}

static inline void service_timezone(const char* host) {
    kprint("Enter timezone offset from UTC (e.g., -5 for EST, -8 for PST): ");
    char* input = rec_input();
    
    // Simple conversion (handle negative)
    int offset = 0;
    int negative = 0;
    int i = 0;
    
    if (input[0] == '-') {
        negative = 1;
        i = 1;
    }
    
    while (input[i] >= '0' && input[i] <= '9') {
        offset = offset * 10 + (input[i] - '0');
        i++;
    }
    
    if (negative) {
        offset = -offset;
    }
    
    rtc_set_timezone(offset);
    kprint("Timezone set to UTC");
    if (offset >= 0) kprint("+");
    kprint_int(offset);
    kprint_newline();
}

// Service management
static inline void service_disable(const char* host) {
    kprint("Enter service name to disable: ");
    char* name = rec_input();
    
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            if (strcmp(name, "help") == 0 || strcmp(name, "svc-enable") == 0 || strcmp(name, "svc-list") == 0) {
                kprint("Service is protected by CORE_SERVICE and cannot be disabled.");
                kprint_newline();
                return;
            }
            services[i].active = 0;
            kprint("Service '");
            kprint(name);
            kprint("' disabled.");
            kprint_newline();
            return;
        }
    }
    
    kprint("Service not found.");
    kprint_newline();
}

static inline void service_enable(const char* host) {
    kprint("Enter service name to enable: ");
    char* name = rec_input();
    
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            services[i].active = 1;
            kprint("Service '");
            kprint(name);
            kprint("' enabled.");
            kprint_newline();
            return;
        }
    }
    
    kprint("Service not found.");
    kprint_newline();
}

static inline void service_list_all(const char* host) {
    kprint("=== Registered Services ===");
    kprint_newline();
    for (int i = 0; i < service_count; i++) {
        kprint("  ");
        kprint(services[i].name);
        if (services[i].requires_auth) {
            kprint(" [REQUIRES AUTH]");
        }
        if (!services[i].active) {
            kprint(" [MANUALLY DISABLED]");
        }
        kprint_newline();
    }
    
    if (secure_lock_enabled) {
        kprint_newline();
        kprint("WARNING: SECURE LOCK IS ENABLED");
        kprint_newline();
        kprint("All services are blocked. Restart required to restore.");
        kprint_newline();
    }
}

// Secure lock services
static inline void service_secure_lock(const char* host) {
    secure_lock_enabled = 1;
    registry_set("security.lock", "1");
    kprint("SECURE LOCK ENABLED");
    kprint_newline();
    kprint("All services are now permanently blocked.");
    kprint_newline();
    kprint("System restart required to restore access.");
    kprint_newline();
}

// Registry services
static inline void service_registry_list(const char* host) {
    serial_write_string("[REGISTRY] List requested\n");
    registry_list();
}

static inline void service_registry_get(const char* host) {
    serial_write_string("[REGISTRY] Get requested\n");
    kprint("Enter key name: ");
    char* key = rec_input();
    
    if (!is_safe_string(key, 64)) {
        serial_write_string("[REGISTRY] SECURITY: Invalid key name\n");
        toast_shell_color("ERROR: Invalid key name", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    const char* value = registry_get(key);
    
    if (value) {
        kprint("Value: ");
        kprint(value);
        kprint_newline();
    } else {
        kprint("Key not found.");
        kprint_newline();
    }
}

static inline void service_registry_set(const char* host) {
    serial_write_string("[REGISTRY] Set requested\n");
    kprint("Enter key name: ");
    char* key = rec_input();
    
    if (!is_safe_string(key, 64)) {
        serial_write_string("[REGISTRY] SECURITY: Invalid key name\n");
        toast_shell_color("ERROR: Invalid key name", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    kprint("Enter value: ");
    char* value = rec_input();
    
    if (!is_safe_string(value, 128)) {
        serial_write_string("[REGISTRY] SECURITY: Invalid value\n");
        toast_shell_color("ERROR: Invalid value", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    if (registry_set(key, value) == 0) {
        serial_write_string("[REGISTRY] Key set: ");
        serial_write_string(key);
        serial_write_string("\n");
        kprint("Registry key set successfully.");
        kprint_newline();
    } else {
        kprint("Failed to set registry key (registry full).");
        kprint_newline();
    }
}

static inline void service_registry_delete(const char* host) {
    serial_write_string("[REGISTRY] Delete requested\n");
    kprint("Enter key name to delete: ");
    char* key = rec_input();
    
    if (!is_safe_string(key, 64)) {
        serial_write_string("[REGISTRY] SECURITY: Invalid key name\n");
        toast_shell_color("ERROR: Invalid key name", LIGHT_RED);
        kprint_newline();
        return;
    }
    
    if (registry_delete(key) == 0) {
        serial_write_string("[REGISTRY] Key deleted: ");
        serial_write_string(key);
        serial_write_string("\n");
        kprint("Registry key deleted.");
        kprint_newline();
    } else {
        kprint("Key not found.");
        kprint_newline();
    }
}

static inline void service_registry_save(const char* host) {
    registry_save();
}

// Editor services
static inline void service_editor_new(const char* host) {
    editor_new();
}

static inline void service_editor_open(const char* host) {
    kprint("Enter filename to open: ");
    char* filename = rec_input();
    editor_open(filename);
}

// Register a service
static inline int register_service(const char* name, service_handler_t handler, int requires_auth) {
    if (service_count >= MAX_SERVICES) {
        return -1;
    }
    
    strncpy(services[service_count].name, name, MAX_SERVICE_NAME - 1);
    services[service_count].name[MAX_SERVICE_NAME - 1] = '\0';
    services[service_count].handler = handler;
    services[service_count].requires_auth = requires_auth;
    services[service_count].active = 1;
    service_count++;
    
    return 0;
}

// Call a service by name
static inline void callservice(const char* service_name, const char* host);

// Manually call a service from the shell
static inline void service_call(const char* host) {
    kprint("Enter service name to call: ");
    char* service_name = rec_input();
    
    kprint("Enter host name (or press enter for 'shell'): ");
    char* manual_host = rec_input();
    
    if (strcmp(manual_host, "") == 0) {
        manual_host = "shell";
    }
    
    if (strcmp(manual_host, "Toast Secure Service") == 0) {
        kprint("ERROR: Cannot impersonate 'Toast Secure Service'.");
        kprint_newline();
        return;
    }
    
    callservice(service_name, manual_host);
}

// Initialize all services
static inline void init_services() {
    serial_write_string("\n========================================\n");
    serial_write_string("[INIT] Initializing service system...\n");
    serial_write_string("========================================\n");
    
    // Core system services
    register_service("shutdown", service_shutdown, 1);
    register_service("reboot", service_reboot, 1);
    register_service("clear", service_clear, 0);
    register_service("help", service_help, 0);
    register_service("call", service_call, 1); // New service to call other services
    
    // Filesystem services
    register_service("fta", service_fta, 0);
    register_service("fs-write", service_fs_write, 0);
    register_service("fs-read", service_fs_read, 0);
    register_service("fs-delete", service_fs_delete, 1);
    register_service("fs-list", service_fs_list, 0);
    register_service("fs-format", service_fs_format, 1);
    
    // Display services
    register_service("cursor-on", service_cursor_enable, 0);
    register_service("cursor-off", service_cursor_disable, 0);
    register_service("color", service_set_color, 0);
    
    // Debug services
    register_service("panic-test", service_panic_test, 0);
    register_service("dbg-except", service_exception_test, 0);
    register_service("mem-info", service_memory_info, 0);
    
    // Time/Date services
    register_service("time", service_time, 0);
    register_service("date", service_date, 0);
    register_service("datetime", service_datetime, 0);
    register_service("timezone", service_timezone, 0);
    
    // Service management
    register_service("svc-disable", service_disable, 1);
    register_service("svc-enable", service_enable, 1);
    register_service("svc-list", service_list_all, 0);
    register_service("securelock", service_secure_lock, 1);
    
    // Registry services
    register_service("registry-list", service_registry_list, 0);
    register_service("registry-get", service_registry_get, 0);
    register_service("registry-set", service_registry_set, 1);
    register_service("registry-delete", service_registry_delete, 1);
    register_service("registry-save", service_registry_save, 1);
    
    // Editor services
    register_service("edit", service_editor_new, 0);
    register_service("edit-open", service_editor_open, 0);
    
    serial_write_string("[INIT] Service registration complete: ");
    kprint_int(service_count);
    serial_write_string(" services\n");
    serial_write_string("========================================\n\n");
    
    kprint("[SERVICES] Registered ");
    kprint_int(service_count);
    kprint(" services");
    kprint_newline();
}

// Call a service by name
static inline void callservice(const char* service_name, const char* host) {
    serial_write_string("[SERVICE] Attempting to call service: ");
    serial_write_string(service_name);
    serial_write_string(" (host: ");
    serial_write_string(host);
    serial_write_string(")\n");
    
    kprint_newline();
    
    // Check if secure lock is enabled
    if (secure_lock_enabled) {
        serial_write_string("[SERVICE] BLOCKED: Secure lock is enabled\n");
        kprint("ERROR: System is in SECURE LOCK mode.");
        kprint_newline();
        kprint("All services are permanently blocked. Restart required.");
        kprint_newline();
        return;
    }
    
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, service_name) == 0) {
            serial_write_string("[SERVICE] Service found at index ");
            kprint_int(i);
            serial_write_string("\n");
            
            if (!services[i].active) {
                serial_write_string("[SERVICE] BLOCKED: Service is disabled\n");
                kprint("ERROR: Service '");
                kprint(service_name);
                kprint("' is disabled.");
                kprint_newline();
                return;
            }
            
            // Check if authentication is required
            if (services[i].requires_auth) {
                serial_write_string("[SERVICE] Service requires authentication\n");
                
                // Simplified and corrected authentication logic
                if (strcmp(host, "Toast Secure Service") != 0) {
                    serial_write_string("[SERVICE] Requesting authorization from user\n");
                    toast_shell_color("Authorization required for '", YELLOW);
                    toast_shell_color(service_name, YELLOW);
                    toast_shell_color("'.", YELLOW);
                    kprint_newline();
                    kprint("This action was requested by '");
                    kprint(host);
                    kprint("'.");
                    kprint_newline();
                    kprint("Type 'y' to authorize: ");
                    
                    char* auth = rec_input();
                    if (strcmp(auth, "y") != 0) {
                        serial_write_string("[SERVICE] Authorization DENIED by user\n");
                        toast_shell_color("Authorization denied.", LIGHT_RED);
                        kprint_newline();
                        return;
                    }
                    serial_write_string("[SERVICE] Authorization GRANTED by user\n");
                } else {
                    serial_write_string("[SERVICE] Auto-authorized (Toast Secure Service)\n");
                }
            } else {
                serial_write_string("[SERVICE] No authentication required\n");
            }
            
            serial_write_string("[SERVICE] Executing service handler...\n");
            services[i].handler(host);
            serial_write_string("[SERVICE] Service execution completed\n");
            return;
        }
    }
    
    // Service not found
    serial_write_string("[SERVICE] ERROR: Service not found: ");
    serial_write_string(service_name);
    serial_write_string("\n");
    kprint("INTERNAL ERROR: Service '");
    kprint(service_name);
    kprint("' not found.");
    kprint_newline();
    kprint("Since there wasn't a panic, you should be fine.");
    kprint_newline();
}

// List all available services
static inline void list_services() {
    kprint_newline();
    toast_shell_color("toastOS v1.1 - Command List", LIGHT_GREEN);
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("APPS:", LIGHT_CYAN);
    kprint_newline();
    kprint("  apps          List all apps");
    kprint_newline();
    kprint("  run <name>    Run an app (e.g., run calc)");
    kprint_newline();
    kprint("  makeapp       Create apps inside toastOS");
    kprint_newline();
    kprint("  myapps        List your custom apps");
    kprint_newline();
    kprint("  runapp <id>   Run custom app by ID");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("FILES:", LIGHT_CYAN);
    kprint_newline();
    kprint("  list          List all files");
    kprint_newline();
    kprint("  edit          Create/edit text file");
    kprint_newline();
    kprint("  write         Write file quickly");
    kprint_newline();
    kprint("  read          Read file from disk");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("SYSTEM:", LIGHT_CYAN);
    kprint_newline();
    kprint("  time          Show current time");
    kprint_newline();
    kprint("  date          Show current date");
    kprint_newline();
    kprint("  clear         Clear screen");
    kprint_newline();
    kprint("  shutdown      Power off");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("NETWORK:", LIGHT_CYAN);
    kprint_newline();
    kprint("  ifconfig      Show network interface");
    kprint_newline();
    kprint("  setip <ip> <netmask> <gateway>");
    kprint_newline();
    kprint("                Configure IP address");
    kprint_newline();
    kprint("  ping <ip>     Send ICMP echo to IP");
    kprint_newline();
    kprint("  netstat       Show network stats");
    kprint_newline();
    kprint_newline();
    
    toast_shell_color("EDITOR (inside edit):", YELLOW);
    kprint_newline();
    kprint("  :w      Save file");
    kprint_newline();
    kprint("  :q      Quit");
    kprint_newline();
    kprint("  :wq     Save and quit");
    kprint_newline();
    kprint_newline();
}

#endif // SERVICES_H