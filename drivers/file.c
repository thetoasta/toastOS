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

#define toastFS 1.0
#define MAX_FILES 10
#define MAX_ID_LEN 256
#define MAX_CONTENT_LEN 1024

typedef struct {
    char id[MAX_ID_LEN];
    char content[MAX_CONTENT_LEN];
} file_entry;

static file_entry file_storage[MAX_FILES];
int toastFS_dataheld = 0;

void local_fs(const char* filename, const char* content) {
    if (toastFS_dataheld >= MAX_FILES) {
        l1_panic("Error: File storage is full.\n");
        return;
    }

    kprint_newline();
    
    char file_id_buffer[MAX_ID_LEN];
    snprintf(file_id_buffer, sizeof(file_id_buffer), "%s-%d", filename, toastFS_dataheld + 1);

    // Store the file
    strncpy(file_storage[toastFS_dataheld].id, file_id_buffer, MAX_ID_LEN);
    strncpy(file_storage[toastFS_dataheld].content, content, MAX_CONTENT_LEN);
    
    toastFS_dataheld++;

    kprint("Your file, ");
    kprint(filename);
    kprint(", has been created with the following content: ");
    kprint_newline();
    kprint(content);
    kprint_newline();
    kprint("Your file ID is: ");
    kprint(file_id_buffer);
    kprint_newline();
}

void read_local_fs(const char* file_id) {
    kprint("Reading file with ID: ");
    kprint(file_id);
    kprint_newline();

    for (int i = 0; i < toastFS_dataheld; i++) {
        if (strcmp(file_storage[i].id, file_id) == 0) {
            kprint("File content: ");
            kprint(file_storage[i].content);
            kprint_newline();
            return;
        }
    }

    l1_panic("Error: File not found.\n");
}