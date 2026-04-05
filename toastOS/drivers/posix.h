/* toastOS POSIX File Descriptor Layer */
#ifndef POSIX_H
#define POSIX_H

#include "stdint.h"
#include "toast_libc.h"

/* ---- POSIX types ---- */
typedef int32_t  ssize_t;
typedef uint32_t mode_t;
typedef uint32_t off_t;
typedef uint32_t ino_t;
typedef uint32_t dev_t;
typedef uint32_t nlink_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t blksize_t;
typedef uint32_t blkcnt_t;

/* ---- File descriptor limits ---- */
#define MAX_OPEN_FILES  64
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* ---- open() flags (POSIX subset) ---- */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

/* ---- lseek() whence ---- */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* ---- File types for stat ---- */
#define S_IFREG     0x8000
#define S_IFDIR     0x4000
#define S_IFCHR     0x2000
#define S_IFIFO     0x1000
#define S_IRWXU     0x01C0
#define S_IRUSR     0x0100
#define S_IWUSR     0x0080
#define S_IXUSR     0x0040

/* ---- errno values ---- */
#define ENOENT      2
#define EIO         5
#define EBADF       9
#define ENOMEM      12
#define EACCES      13
#define EEXIST      17
#define ENOTDIR     20
#define EISDIR      21
#define EINVAL      22
#define EMFILE      24
#define ENOSPC      28
#define EPIPE       32
#define ENOSYS      38
#define ENOTEMPTY   39

/* Global errno */
extern int errno;

/* ---- stat structure ---- */
struct posix_stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;
    uint32_t  st_atime;
    uint32_t  st_mtime;
    uint32_t  st_ctime;
};

/* ---- Initialise the fd table (sets up stdin/stdout/stderr) ---- */
void posix_init(void);

/* ---- POSIX file operations ---- */
int     posix_open(const char *path, int flags);
int     posix_close(int fd);
ssize_t posix_read(int fd, void *buf, size_t count);
ssize_t posix_write(int fd, const void *buf, size_t count);
off_t   posix_lseek(int fd, off_t offset, int whence);
int     posix_stat(const char *path, struct posix_stat *st);
int     posix_fstat(int fd, struct posix_stat *st);
int     posix_dup(int oldfd);
int     posix_dup2(int oldfd, int newfd);
int     posix_isatty(int fd);

#endif /* POSIX_H */
