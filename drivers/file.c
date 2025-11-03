/* 
    * this uses lots of sourced content. 
    * DO NOT REPLICATE THIS EXPECTING IT TO ACTUALLY WORK.
*/

#include "kio.h"
#include "funcs.h"
#include "stdint.h"
#include "panic.h"

#define toastFS 1.0
int toastFS_dataheld = 0;

void local_fs(const char* filename, const char* content) {
    toastFS_dataheld++;
    kprint("Your file, ");
    kprint(filename);
    kprint(", has been created with the following content: ");
    kprint_newline();
    kprint(content);
    kprint_newline();
    char base1[256];
    snprintf(base1, sizeof(base1), "%s-%d", filename, toastFS_dataheld);
    kprint("Your file ID is: ");
    kprint(base1);
    snfprintf(base1, sizeof(base1), " (ID: %d)", toastFS_dataheld);
    kprint_newline();
}