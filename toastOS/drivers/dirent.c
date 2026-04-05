/* toastOS POSIX dirent implementation - wraps FAT16 directory enumeration */

#include "dirent.h"
#include "fat16.h"
#include "mmu.h"
#include "toast_libc.h"

DIR *opendir(const char *name) {
    DIR *d = (DIR *)kmalloc(sizeof(DIR));
    if (!d) return (DIR *)0;

    memset(d, 0, sizeof(DIR));
    strncpy(d->path, name ? name : "/", 255);

    /* Enumerate all entries from FAT16 */
    fat16_enum_entry_t fat_entries[128];
    int count = fat16_enumerate_dir(name, fat_entries, 128);
    if (count < 0) {
        kfree(d);
        return (DIR *)0;
    }

    d->count = count;
    d->index = 0;

    for (int i = 0; i < count; i++) {
        d->entries[i].d_ino  = (uint32_t)(i + 1);
        d->entries[i].d_type = fat_entries[i].is_dir ? DT_DIR : DT_REG;
        strncpy(d->entries[i].d_name, fat_entries[i].name, 255);
        d->entries[i].d_name[255] = '\0';
    }

    return d;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp || dirp->index >= dirp->count)
        return (struct dirent *)0;

    return &dirp->entries[dirp->index++];
}

int closedir(DIR *dirp) {
    if (!dirp) return -1;
    kfree(dirp);
    return 0;
}

void rewinddir(DIR *dirp) {
    if (dirp) dirp->index = 0;
}
