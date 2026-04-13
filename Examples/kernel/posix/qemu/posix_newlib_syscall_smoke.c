#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int charm_posix_newlib_syscall_probe_entry(void) {
    char ch = 0;
    struct stat st;

    errno = 55;
    if (isatty(0) != 1) return 91;
    if (errno != 55) return 92;

    errno = 66;
    if (isatty(1) != 0) return 93;
    if (errno != 66) return 94;

    if (fstat(0, &st) != 0) return 95;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 96;

    if (fstat(1, &st) != 0) return 97;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 98;

    if (getpid() <= 0) return 99;

    errno = 0;
    if (read(-1, &ch, 1) != -1) return 100;
    if (errno != EBADF) return 101;

    return write(1, "newlib-syscall-ok\n", 18) == 18 ? 0 : 102;
}

int charm_posix_newlib_kill_self_entry(void) {
    if (write(1, "newlib-kill\n", 12) != 12) return 111;
    (void)kill(getpid(), SIGTERM);
    return 112;
}

int charm_posix_newlib_lseek_entry(void) {
    char buf[6] = {0};
    off_t end = lseek(0, 0, SEEK_END);
    if (end != 5) return 121;
    if (lseek(0, 0, SEEK_SET) != 0) return 122;
    if (read(0, buf, 5) != 5) return 123;
    if (memcmp(buf, "alpha", 5) != 0) return 124;

    errno = 0;
    if (lseek(1, 0, SEEK_SET) != (off_t)-1) return 125;
    if (errno != ESPIPE) return 126;

    return write(1, "newlib-lseek-ok\n", 16) == 16 ? 0 : 127;
}

int charm_posix_newlib_path_entry(void) {
    struct stat st;
    char buf[6] = {0};
    int fd = open("/newlib-path.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 131;
    if (write(fd, "bravo", 5) != 5) return 132;
    if (close(fd) != 0) return 133;

    if (stat("/newlib-path.txt", &st) != 0) return 134;
    if ((st.st_mode & S_IFMT) != S_IFREG) return 135;
    if (st.st_size != 5) return 136;

    fd = open("/newlib-path.txt", O_RDONLY, 0);
    if (fd < 0) return 137;
    if (read(fd, buf, 5) != 5) return 138;
    if (memcmp(buf, "bravo", 5) != 0) return 139;
    if (close(fd) != 0) return 140;

    if (unlink("/newlib-path.txt") != 0) return 141;
    errno = 0;
    if (stat("/newlib-path.txt", &st) != -1) return 142;
    if (errno == 0) return 143;

    return write(1, "newlib-path-ok\n", 15) == 15 ? 0 : 144;
}
