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

// Forward declarations
static inline void list_services();

// Service handlers
static inline void service_fta(const char* host) {
    fs_test_auto();
}

static inline void service_shutdown(const char* host) {
    kprint("The host, '");
    kprint(host);
    kprint("' requested to shutdown the system via a service.");
    kprint_newline();
    kprint("Type enter to allow this shutdown.");
    kprint_newline();
    char* input = rec_input();
    if (strcmp(input, "") == 0) {
        kprint("Shutting down as requested by host '");
        kprint(host);
        kprint("'.");
        kprint_newline();
        shutdown();
    } else {
        kprint("Denied shutdown.");
        kprint_newline();
    }
}

// Filesystem services
static inline void service_fs_write(const char* host) {
    kprint("Enter filename: ");
    char* filename = rec_input();
    kprint("Enter content: ");
    char* content = rec_input();
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

// Initialize all services
static inline void init_services() {
    // Core system services
    register_service("shutdown", service_shutdown, 1);
    register_service("reboot", service_reboot, 1);
    register_service("clear", service_clear, 0);
    register_service("help", service_help, 0);
    
    // Filesystem services
    register_service("fta", service_fta, 0);
    register_service("fs-write", service_fs_write, 1);
    register_service("fs-read", service_fs_read, 0);
    register_service("fs-delete", service_fs_delete, 1);
    register_service("fs-list", service_fs_list, 0);
    register_service("fs-format", service_fs_format, 1);
    
    // Display services
    register_service("cursor-on", service_cursor_enable, 0);
    register_service("cursor-off", service_cursor_disable, 0);
    register_service("color", service_set_color, 0);
    
    // Debug services
    register_service("panic-test", service_panic_test, 1);
    register_service("dbg-except", service_exception_test, 1);
    register_service("mem-info", service_memory_info, 0);
    
    kprint("[SERVICES] Registered ");
    kprint_int(service_count);
    kprint(" services");
    kprint_newline();
}

// Call a service by name
static inline void callservice(const char* service_name, const char* host) {
    kprint_newline();
    
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, service_name) == 0) {
            if (services[i].active) {
                services[i].handler(host);
                return;
            } else {
                kprint("ERROR: Service '");
                kprint(service_name);
                kprint("' is disabled.");
                kprint_newline();
                return;
            }
        }
    }
    
    // Service not found
    kprint("INTERNAL ERROR: Service '");
    kprint(service_name);
    kprint("' not found.");
    kprint_newline();
    kprint("Since there wasn't a panic, you should be fine.");
    kprint_newline();
}

// List all available services
static inline void list_services() {
    kprint("Available services:");
    kprint_newline();
    for (int i = 0; i < service_count; i++) {
        kprint("  - ");
        kprint(services[i].name);
        if (services[i].requires_auth) {
            kprint(" (requires auth)");
        }
        if (!services[i].active) {
            kprint(" [DISABLED]");
        }
        kprint_newline();
    }
}

#endif // SERVICES_H