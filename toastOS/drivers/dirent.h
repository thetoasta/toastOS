/* toastOS POSIX dirent - directory entry interface */
#ifndef DIRENT_H
#define DIRENT_H

#include "stdint.h"
#include "toast_libc.h"

#define DT_UNKNOWN  0
#define DT_REG      8
#define DT_DIR      4

struct dirent {
    uint32_t d_ino;
    uint8_t  d_type;         /* DT_REG or DT_DIR */
    char     d_name[256];
};

typedef struct {
    char     path[256];
    int      index;          /* current entry index */
    int      count;          /* total entries found */
    struct dirent entries[128]; /* max 128 entries per dir */
} DIR;

DIR           *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int            closedir(DIR *dirp);
void           rewinddir(DIR *dirp);

#endif /* DIRENT_H */
