/* toastOS POSIX File Descriptor Layer */

#include "posix.h"
#include "fat16.h"
#include "kio.h"
#include "toast_libc.h"
#include "mmu.h"

/* Global errno */
int errno = 0;

/* ---- File descriptor types ---- */
#define FD_TYPE_NONE    0
#define FD_TYPE_CONSOLE 1   /* stdin/stdout/stderr */
#define FD_TYPE_FILE    2   /* FAT16 regular file */

typedef struct {
    uint8_t  type;
    uint8_t  in_use;
    int      flags;         /* O_RDONLY, O_WRONLY, etc. */
    uint32_t offset;        /* current file position */
    uint32_t file_size;
    char     path[64];      /* file path for FAT16 */
    char    *buf;           /* read buffer for files */
    uint32_t buf_size;
} fd_entry_t;

static fd_entry_t fd_table[MAX_OPEN_FILES];

/* ---- Helpers ---- */

static int alloc_fd(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fd_table[i].in_use)
            return i;
    }
    return -1;
}

/* ---- Initialisation ---- */

void posix_init(void) {
    memset(fd_table, 0, sizeof(fd_table));

    /* fd 0 = stdin (console/keyboard) */
    fd_table[0].type   = FD_TYPE_CONSOLE;
    fd_table[0].in_use = 1;
    fd_table[0].flags  = O_RDONLY;

    /* fd 1 = stdout (VGA console) */
    fd_table[1].type   = FD_TYPE_CONSOLE;
    fd_table[1].in_use = 1;
    fd_table[1].flags  = O_WRONLY;

    /* fd 2 = stderr (VGA console) */
    fd_table[2].type   = FD_TYPE_CONSOLE;
    fd_table[2].in_use = 1;
    fd_table[2].flags  = O_WRONLY;
}

/* ---- open ---- */

int posix_open(const char *path, int flags) {
    if (!path) { errno = EINVAL; return -1; }

    int fd = alloc_fd();
    if (fd < 0) { errno = EMFILE; return -1; }

    /* Check if file exists */
    int exists = fat16_file_exists_at(path);

    if (!exists && !(flags & O_CREAT)) {
        errno = ENOENT;
        return -1;
    }

    if (!exists && (flags & O_CREAT)) {
        /* Create empty file */
        if (fat16_create_file_at(path, "") != 0) {
            errno = EIO;
            return -1;
        }
    }

    /* Read the file into a buffer so we can do offset-based I/O */
    char *filebuf = (char *)kmalloc(65536); /* 64KB max file size */
    if (!filebuf) { errno = ENOMEM; return -1; }
    memset(filebuf, 0, 65536);

    int file_size = fat16_read_file_at(path, filebuf, 65536);
    if (file_size < 0) file_size = 0;

    if (flags & O_TRUNC) {
        memset(filebuf, 0, 65536);
        file_size = 0;
    }

    fd_entry_t *e  = &fd_table[fd];
    e->type        = FD_TYPE_FILE;
    e->in_use      = 1;
    e->flags       = flags;
    e->offset      = (flags & O_APPEND) ? (uint32_t)file_size : 0;
    e->file_size   = (uint32_t)file_size;
    e->buf         = filebuf;
    e->buf_size    = 65536;
    strncpy(e->path, path, 63);

    return fd;
}

/* ---- close ---- */

int posix_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        errno = EBADF;
        return -1;
    }

    fd_entry_t *e = &fd_table[fd];

    /* If it's a file and was opened for writing, flush back to disk */
    if (e->type == FD_TYPE_FILE && (e->flags & (O_WRONLY | O_RDWR))) {
        /* Delete and recreate with new content */
        fat16_delete_at(e->path);
        /* Null-terminate for FAT16 */
        if (e->file_size < e->buf_size)
            e->buf[e->file_size] = '\0';
        fat16_create_file_at(e->path, e->buf);
    }

    if (e->buf) {
        kfree(e->buf);
        e->buf = (char *)0;
    }

    memset(e, 0, sizeof(fd_entry_t));
    return 0;
}

/* ---- read ---- */

ssize_t posix_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        errno = EBADF;
        return -1;
    }

    fd_entry_t *e = &fd_table[fd];

    if (e->type == FD_TYPE_CONSOLE && fd == STDIN_FILENO) {
        /* Read from keyboard - for now just return 0 (non-blocking) */
        /* A full implementation would block until input is available */
        (void)buf; (void)count;
        return 0;
    }

    if (e->type == FD_TYPE_FILE) {
        if (e->offset >= e->file_size)
            return 0; /* EOF */

        uint32_t avail = e->file_size - e->offset;
        if (count > avail) count = avail;

        memcpy(buf, e->buf + e->offset, count);
        e->offset += count;
        return (ssize_t)count;
    }

    errno = EBADF;
    return -1;
}

/* ---- write ---- */

ssize_t posix_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        errno = EBADF;
        return -1;
    }

    fd_entry_t *e = &fd_table[fd];

    if (e->type == FD_TYPE_CONSOLE) {
        /* Write to VGA console */
        const char *s = (const char *)buf;
        for (size_t i = 0; i < count; i++) {
            if (s[i] == '\n')
                kprint_newline();
            else {
                char tmp[2] = { s[i], '\0' };
                kprint(tmp);
            }
        }
        return (ssize_t)count;
    }

    if (e->type == FD_TYPE_FILE) {
        if (e->flags & O_APPEND)
            e->offset = e->file_size;

        /* Check bounds */
        if (e->offset + count > e->buf_size) {
            count = e->buf_size - e->offset;
            if (count == 0) { errno = ENOSPC; return -1; }
        }

        memcpy(e->buf + e->offset, buf, count);
        e->offset += count;
        if (e->offset > e->file_size)
            e->file_size = e->offset;

        return (ssize_t)count;
    }

    errno = EBADF;
    return -1;
}

/* ---- lseek ---- */

off_t posix_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        errno = EBADF;
        return (off_t)-1;
    }

    fd_entry_t *e = &fd_table[fd];
    if (e->type != FD_TYPE_FILE) {
        errno = EINVAL;
        return (off_t)-1;
    }

    int32_t new_off;
    switch (whence) {
        case SEEK_SET: new_off = (int32_t)offset; break;
        case SEEK_CUR: new_off = (int32_t)e->offset + (int32_t)offset; break;
        case SEEK_END: new_off = (int32_t)e->file_size + (int32_t)offset; break;
        default: errno = EINVAL; return (off_t)-1;
    }

    if (new_off < 0) { errno = EINVAL; return (off_t)-1; }

    e->offset = (uint32_t)new_off;
    return (off_t)e->offset;
}

/* ---- stat ---- */

int posix_stat(const char *path, struct posix_stat *st) {
    if (!path || !st) { errno = EINVAL; return -1; }

    if (!fat16_file_exists_at(path)) {
        errno = ENOENT;
        return -1;
    }

    memset(st, 0, sizeof(struct posix_stat));

    /* Read file to get size */
    char tmp[1];
    int sz = fat16_read_file_at(path, tmp, 0);
    if (sz < 0) sz = 0;

    st->st_mode    = S_IFREG | S_IRUSR | S_IWUSR;
    st->st_nlink   = 1;
    st->st_size    = (off_t)sz;
    st->st_blksize = 512;
    st->st_blocks  = (sz + 511) / 512;

    return 0;
}

/* ---- fstat ---- */

int posix_fstat(int fd, struct posix_stat *st) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use || !st) {
        errno = EBADF;
        return -1;
    }

    memset(st, 0, sizeof(struct posix_stat));
    fd_entry_t *e = &fd_table[fd];

    if (e->type == FD_TYPE_CONSOLE) {
        st->st_mode = S_IFCHR | S_IRUSR | S_IWUSR;
        st->st_nlink = 1;
        return 0;
    }

    if (e->type == FD_TYPE_FILE) {
        st->st_mode    = S_IFREG | S_IRUSR | S_IWUSR;
        st->st_nlink   = 1;
        st->st_size    = (off_t)e->file_size;
        st->st_blksize = 512;
        st->st_blocks  = (e->file_size + 511) / 512;
        return 0;
    }

    errno = EBADF;
    return -1;
}

/* ---- dup / dup2 ---- */

int posix_dup(int oldfd) {
    if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || !fd_table[oldfd].in_use) {
        errno = EBADF;
        return -1;
    }

    int newfd = alloc_fd();
    if (newfd < 0) { errno = EMFILE; return -1; }

    memcpy(&fd_table[newfd], &fd_table[oldfd], sizeof(fd_entry_t));
    /* Note: shares the same buffer pointer - not a deep copy */
    return newfd;
}

int posix_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || !fd_table[oldfd].in_use) {
        errno = EBADF;
        return -1;
    }
    if (newfd < 0 || newfd >= MAX_OPEN_FILES) {
        errno = EBADF;
        return -1;
    }

    if (oldfd == newfd) return newfd;

    if (fd_table[newfd].in_use)
        posix_close(newfd);

    memcpy(&fd_table[newfd], &fd_table[oldfd], sizeof(fd_entry_t));
    return newfd;
}

/* ---- isatty ---- */

int posix_isatty(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        errno = EBADF;
        return 0;
    }
    return fd_table[fd].type == FD_TYPE_CONSOLE ? 1 : 0;
}
