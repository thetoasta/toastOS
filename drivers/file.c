/* 
    * this uses lots of sourced content. 
    * DO NOT REPLICATE THIS EXPECTING IT TO ACTUALLY WORK.
*/

#include "kio.h"
#include "funcs.h"
#include "stdint.h"
#include "panic.h"
#include "stdio.h"
#include "string.h"
#include "file.h"

#define toastFS_VERSION "1.0"
#define MAX_FILES 10
#define MAX_ID_LEN 256
#define MAX_CONTENT_LEN 1024

typedef struct {
    char id[MAX_ID_LEN];
    char filename[MAX_ID_LEN];
    char content[MAX_CONTENT_LEN];
    int in_use;
} file_entry;

static file_entry file_storage[MAX_FILES];
static int toastFS_dataheld = 0;

// Helper to convert int to string
static void int_to_str(int num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int i = 0;
    int temp = num;
    while (temp > 0) {
        temp /= 10;
        i++;
    }
    
    str[i] = '\0';
    while (num > 0) {
        str[--i] = '0' + (num % 10);
        num /= 10;
    }
}

void local_fs(const char* filename, const char* content) {
    if (toastFS_dataheld >= MAX_FILES) {
        kprint("ERROR: File storage is full (max ");
        char num_str[12];
        int_to_str(MAX_FILES, num_str);
        kprint(num_str);
        kprint(" files).");
        kprint_newline();
        return;
    }

    // Generate file ID: filename-N
    char file_id_buffer[MAX_ID_LEN];
    char num_str[12];
    int_to_str(toastFS_dataheld + 1, num_str);
    
    int i = 0, j = 0;
    while (filename[i] && j < MAX_ID_LEN - 20) {
        file_id_buffer[j++] = filename[i++];
    }
    file_id_buffer[j++] = '-';
    i = 0;
    while (num_str[i] && j < MAX_ID_LEN - 1) {
        file_id_buffer[j++] = num_str[i++];
    }
    file_id_buffer[j] = '\0';

    // Store the file
    strncpy(file_storage[toastFS_dataheld].id, file_id_buffer, MAX_ID_LEN);
    strncpy(file_storage[toastFS_dataheld].filename, filename, MAX_ID_LEN);
    strncpy(file_storage[toastFS_dataheld].content, content, MAX_CONTENT_LEN);
    file_storage[toastFS_dataheld].in_use = 1;
    
    toastFS_dataheld++;

    kprint("[FS] File created: '");
    kprint(filename);
    kprint("' with ID: '");
    kprint(file_id_buffer);
    kprint("'");
    kprint_newline();
    kprint("[FS] Content: ");
    kprint(content);
    kprint_newline();
}

void read_local_fs(const char* file_id) {
    kprint("[FS] Reading file with ID: '");
    kprint(file_id);
    kprint("'");
    kprint_newline();

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_storage[i].in_use && strcmp(file_storage[i].id, file_id) == 0) {
            kprint("[FS] Filename: ");
            kprint(file_storage[i].filename);
            kprint_newline();
            kprint("[FS] Content: ");
            kprint(file_storage[i].content);
            kprint_newline();
            return;
        }
    }

    kprint("ERROR: File '");
    kprint(file_id);
    kprint("' not found.");
    kprint_newline();
}

void list_files(void) {
    kprint("[FS] toastFS v");
    kprint(toastFS_VERSION);
    kprint(" - Listing all files:");
    kprint_newline();
    
    if (toastFS_dataheld == 0) {
        kprint("  (No files stored)");
        kprint_newline();
        return;
    }
    
    char num_str[12];
    int_to_str(toastFS_dataheld, num_str);
    kprint("  Total files: ");
    kprint(num_str);
    kprint_newline();
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_storage[i].in_use) {
            kprint("  - ID: '");
            kprint(file_storage[i].id);
            kprint("' | Name: '");
            kprint(file_storage[i].filename);
            kprint("'");
            kprint_newline();
        }
    }
}

void delete_file(const char* file_id) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_storage[i].in_use && strcmp(file_storage[i].id, file_id) == 0) {
            file_storage[i].in_use = 0;
            file_storage[i].id[0] = '\0';
            file_storage[i].filename[0] = '\0';
            file_storage[i].content[0] = '\0';
            toastFS_dataheld--;
            
            kprint("[FS] File '");
            kprint(file_id);
            kprint("' deleted successfully.");
            kprint_newline();
            return;
        }
    }
    
    kprint("ERROR: File '");
    kprint(file_id);
    kprint("' not found.");
    kprint_newline();
}

// Automated filesystem test - runs without user input
void fs_test_auto(void) {
    kprint("========================================");
    kprint_newline();
    kprint("[AUTO-TEST] Starting filesystem tests...");
    kprint_newline();
    kprint("========================================");
    kprint_newline();
    kprint_newline();
    
    // Test 1: Create files
    kprint("[TEST 1] Creating test files...");
    kprint_newline();
    local_fs("test1.txt", "Hello from toastOS!");
    local_fs("data.dat", "Binary data goes here");
    local_fs("readme.md", "Welcome to toastOS filesystem");
    kprint_newline();
    
    // Test 2: List files
    kprint("[TEST 2] Listing all files...");
    kprint_newline();
    list_files();
    kprint_newline();
    
    // Test 3: Read files
    kprint("[TEST 3] Reading files...");
    kprint_newline();
    read_local_fs("test1.txt-1");
    kprint_newline();
    read_local_fs("data.dat-2");
    kprint_newline();
    
    // Test 4: Try reading non-existent file
    kprint("[TEST 4] Testing error handling...");
    kprint_newline();
    read_local_fs("nonexistent-999");
    kprint_newline();
    
    // Test 5: Delete a file
    kprint("[TEST 5] Deleting a file...");
    kprint_newline();
    delete_file("data.dat-2");
    kprint_newline();
    
    // Test 6: List files after deletion
    kprint("[TEST 6] Listing files after deletion...");
    kprint_newline();
    list_files();
    kprint_newline();
    
    // Test 7: Create more files
    kprint("[TEST 7] Creating more files...");
    kprint_newline();
    local_fs("config.cfg", "debug=true, version=1.0");
    local_fs("log.txt", "System started successfully");
    kprint_newline();
    
    // Final list
    kprint("[FINAL] Final filesystem state:");
    kprint_newline();
    list_files();
    kprint_newline();
    
    kprint("========================================");
    kprint_newline();
    kprint("[AUTO-TEST] All tests completed!");
    kprint_newline();
    kprint("========================================");
    kprint_newline();
}