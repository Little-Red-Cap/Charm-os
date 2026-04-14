#include "charm_posix_user_fs.h"

#include <string.h>

int charm_posix_c_fs_header_entry(void) {
    char cwd[32] = {0};
    char small[2] = {0};
    char buf[4] = {0};
    charm_posix_stat_t st;
    int fd = -1;
    int* err = charm_posix_errno_location();

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

    fd = charm_posix_open("note.txt", CHARM_POSIX_O_RDONLY, 0);
    if (fd < 0) return 323;
    if (charm_posix_lseek(fd, 0, CHARM_POSIX_SEEK_END) != 2) return 324;
    if (charm_posix_lseek(fd, 0, CHARM_POSIX_SEEK_SET) != 0) return 325;
    if (charm_posix_read(fd, buf, 2) != 2) return 326;
    if (memcmp(buf, "fs", 2) != 0) return 327;
    if (charm_posix_close(fd) != 0) return 328;

    *err = 0;
    if (charm_posix_chdir("/cfs/note.txt") != -1) return 329;
    if (*err != CHARM_POSIX_ENOTDIR) return 330;

    if (charm_posix_rename("/cfs/note.txt", "/cfs/renamed.txt") != 0) return 331;
    if (charm_posix_unlink("/cfs/renamed.txt") != 0) return 332;

    if (charm_posix_chdir("/") != 0) return 333;
    if (charm_posix_rmdir("/cfs") != 0) return 334;

    *err = 0;
    if (charm_posix_stat("/cfs", &st) != -1) return 335;
    if (*err != CHARM_POSIX_ENOENT) return 336;

    return charm_posix_write(1, "c-fs-header-ok\n", 15) == 15 ? 0 : 337;
}
