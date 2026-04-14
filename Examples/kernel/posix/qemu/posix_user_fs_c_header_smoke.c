#include "charm_posix_user_fs.h"

#include <string.h>

int charm_posix_c_fs_header_entry(void) {
    char cwd[32] = {0};
    char small[2] = {0};
    char buf[4] = {0};
    charm_posix_stat_t st;
    charm_posix_dir_t* dir = 0;
    const charm_posix_dirent_t* ent = 0;
    int fd = -1;
    int* err = charm_posix_errno_location();
    int saw_cfs = 0;
    int saw_note = 0;

    if (err == 0) return 301;
    if (charm_posix_getpid() <= 0) return 302;

    if (charm_posix_getcwd(cwd, sizeof(cwd)) != cwd) return 303;
    if (strcmp(cwd, "/") != 0) return 304;

    *err = 41;
    if (charm_posix_isatty(1) != 0) return 305;
    if (*err != 41) return 306;

    if (charm_posix_fstat(1, &st) != 0) return 307;
    if ((st.st_mode & CHARM_POSIX_S_IFMT) != CHARM_POSIX_S_IFIFO) return 308;

    if (charm_posix_mkdir("/cfs") != 0) return 309;
    if (charm_posix_stat("/cfs", &st) != 0) return 310;
    if ((st.st_mode & CHARM_POSIX_S_IFMT) != CHARM_POSIX_S_IFDIR) return 311;

    dir = charm_posix_opendir("/");
    if (dir == 0) return 312;
    while ((ent = charm_posix_readdir(dir)) != 0) {
        if (strcmp(ent->d_name, "cfs") == 0) {
            saw_cfs = 1;
            if ((ent->d_mode & CHARM_POSIX_S_IFMT) != CHARM_POSIX_S_IFDIR) return 313;
        }
    }
    if (!saw_cfs) return 314;
    *err = 42;
    if (charm_posix_readdir(dir) != 0) return 315;
    if (*err != 42) return 316;
    if (charm_posix_closedir(dir) != 0) return 317;
    *err = 0;
    if (charm_posix_readdir(dir) != 0) return 318;
    if (*err != CHARM_POSIX_EINVAL) return 319;
    *err = 0;
    if (charm_posix_closedir(dir) != -1) return 320;
    if (*err != CHARM_POSIX_EINVAL) return 321;

    if (charm_posix_chdir("/cfs") != 0) return 312;
    if (charm_posix_getcwd(cwd, sizeof(cwd)) != cwd) return 313;
    if (strcmp(cwd, "/cfs") != 0) return 314;

    *err = 0;
    if (charm_posix_getcwd(small, sizeof(small)) != 0) return 315;
    if (*err != CHARM_POSIX_ERANGE) return 316;

    fd = charm_posix_open("note.txt", CHARM_POSIX_O_CREAT | CHARM_POSIX_O_TRUNC | CHARM_POSIX_O_WRONLY, 0);
    if (fd < 0) return 317;
    if (charm_posix_write(fd, "fs", 2) != 2) return 318;
    if (charm_posix_close(fd) != 0) return 319;

    if (charm_posix_stat("/cfs/note.txt", &st) != 0) return 320;
    if ((st.st_mode & CHARM_POSIX_S_IFMT) != CHARM_POSIX_S_IFREG) return 321;
    if (st.st_size != 2) return 322;

    *err = 0;
    if (charm_posix_opendir("/cfs/note.txt") != 0) return 323;
    if (*err != CHARM_POSIX_ENOTDIR) return 324;
    *err = 0;
    if (charm_posix_readdir(0) != 0) return 325;
    if (*err != CHARM_POSIX_EINVAL) return 326;
    *err = 0;
    if (charm_posix_closedir(0) != -1) return 327;
    if (*err != CHARM_POSIX_EINVAL) return 328;

    dir = charm_posix_opendir("/cfs");
    if (dir == 0) return 329;
    while ((ent = charm_posix_readdir(dir)) != 0) {
        if (strcmp(ent->d_name, "note.txt") == 0) {
            saw_note = 1;
            if ((ent->d_mode & CHARM_POSIX_S_IFMT) != CHARM_POSIX_S_IFREG) return 330;
            if (ent->d_size != 2) return 331;
        }
    }
    if (!saw_note) return 332;
    *err = 43;
    if (charm_posix_readdir(dir) != 0) return 333;
    if (*err != 43) return 334;
    if (charm_posix_closedir(dir) != 0) return 335;

    fd = charm_posix_open("note.txt", CHARM_POSIX_O_RDONLY, 0);
    if (fd < 0) return 336;
    if (charm_posix_lseek(fd, 0, CHARM_POSIX_SEEK_END) != 2) return 337;
    if (charm_posix_lseek(fd, 0, CHARM_POSIX_SEEK_SET) != 0) return 338;
    if (charm_posix_read(fd, buf, 2) != 2) return 339;
    if (memcmp(buf, "fs", 2) != 0) return 340;
    if (charm_posix_close(fd) != 0) return 341;

    *err = 0;
    if (charm_posix_chdir("/cfs/note.txt") != -1) return 342;
    if (*err != CHARM_POSIX_ENOTDIR) return 343;

    if (charm_posix_rename("/cfs/note.txt", "/cfs/renamed.txt") != 0) return 344;
    if (charm_posix_unlink("/cfs/renamed.txt") != 0) return 345;

    if (charm_posix_chdir("/") != 0) return 346;
    if (charm_posix_rmdir("/cfs") != 0) return 347;

    *err = 0;
    if (charm_posix_stat("/cfs", &st) != -1) return 348;
    if (*err != CHARM_POSIX_ENOENT) return 349;

    return charm_posix_write(1, "c-fs-header-ok\n", 15) == 15 ? 0 : 350;
}
