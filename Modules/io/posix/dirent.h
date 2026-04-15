#ifndef CHARM_POSIX_DIRENT_H
#define CHARM_POSIX_DIRENT_H

#include "charm_posix_user_fs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef charm_posix_dir_t DIR;

typedef struct dirent {
    char d_name[CHARM_POSIX_DIRENT_NAME_MAX];
    unsigned char d_type;
    charm_posix_mode_t d_mode;
    unsigned long long d_size;
} dirent;

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_REG 8

DIR* opendir(const char* path);
struct dirent* readdir(DIR* dir);
int closedir(DIR* dir);

#ifdef __cplusplus
}
#endif

#endif
