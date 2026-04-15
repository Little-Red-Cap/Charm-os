#ifndef CHARM_POSIX_USER_FS_H
#define CHARM_POSIX_USER_FS_H

#include "charm_posix_user_crt.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#define CHARM_POSIX_FS_NOEXCEPT noexcept
#else
#define CHARM_POSIX_FS_NOEXCEPT
#endif

typedef long long charm_posix_off_t;
typedef unsigned int charm_posix_mode_t;

typedef struct charm_posix_stat_t {
    charm_posix_mode_t st_mode;
    unsigned long long st_size;
} charm_posix_stat_t;

#define CHARM_POSIX_DIRENT_NAME_MAX 64

typedef struct charm_posix_dirent_t {
    char d_name[CHARM_POSIX_DIRENT_NAME_MAX];
    charm_posix_mode_t d_mode;
    unsigned long long d_size;
} charm_posix_dirent_t;

typedef struct charm_posix_dir_t {
    size_t slot;
} charm_posix_dir_t;

#define CHARM_POSIX_ENOENT 2
#define CHARM_POSIX_EACCES 13
#define CHARM_POSIX_EBUSY 16
#define CHARM_POSIX_EEXIST 17
#define CHARM_POSIX_ENOTDIR 20
#define CHARM_POSIX_EISDIR 21
#define CHARM_POSIX_EINVAL 22
#define CHARM_POSIX_ESPIPE 29
#define CHARM_POSIX_ERANGE 34
#define CHARM_POSIX_ENOTEMPTY 39

#define CHARM_POSIX_S_IFMT 0170000u
#define CHARM_POSIX_S_IFIFO 0010000u
#define CHARM_POSIX_S_IFCHR 0020000u
#define CHARM_POSIX_S_IFDIR 0040000u
#define CHARM_POSIX_S_IFREG 0100000u

#define CHARM_POSIX_SEEK_SET 0
#define CHARM_POSIX_SEEK_CUR 1
#define CHARM_POSIX_SEEK_END 2

#define CHARM_POSIX_O_RDONLY 0x0
#define CHARM_POSIX_O_WRONLY 0x1
#define CHARM_POSIX_O_RDWR 0x2
#define CHARM_POSIX_O_ACCMODE 0x3
#define CHARM_POSIX_O_CREAT 0x40
#define CHARM_POSIX_O_EXCL 0x80
#define CHARM_POSIX_O_TRUNC 0x200
#define CHARM_POSIX_O_APPEND 0x400
#define CHARM_POSIX_O_NONBLOCK 0x800

int charm_posix_open(const char* path, int flags, int mode) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_close(int fd) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_stat(const char* path, charm_posix_stat_t* out) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_fstat(int fd, charm_posix_stat_t* out) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_isatty(int fd) CHARM_POSIX_FS_NOEXCEPT;
charm_posix_off_t charm_posix_lseek(int fd, charm_posix_off_t offset, int whence) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_mkdir(const char* path) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_unlink(const char* path) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_rmdir(const char* path) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_rename(const char* from, const char* to) CHARM_POSIX_FS_NOEXCEPT;
charm_posix_dir_t* charm_posix_opendir(const char* path) CHARM_POSIX_FS_NOEXCEPT;
const charm_posix_dirent_t* charm_posix_readdir(charm_posix_dir_t* dir) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_closedir(charm_posix_dir_t* dir) CHARM_POSIX_FS_NOEXCEPT;
int charm_posix_chdir(const char* path) CHARM_POSIX_FS_NOEXCEPT;
char* charm_posix_getcwd(char* buf, size_t size) CHARM_POSIX_FS_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef CHARM_POSIX_FS_NOEXCEPT

#endif
