#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
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

    if (access("/newlib-path.txt", F_OK) != 0) return 137;
    if (access("/newlib-path.txt", R_OK) != 0) return 138;
    if (access("/newlib-path.txt", W_OK) != 0) return 139;
    errno = 0;
    if (access("/newlib-path.txt", X_OK) != -1) return 140;
    if (errno != EACCES) return 141;
    errno = 0;
    if (access("/newlib-missing.txt", F_OK) != -1) return 142;
    if (errno != ENOENT) return 143;

    if (mkdir("/newlib-dir", 0) != 0) return 144;

    if (stat("/newlib-dir", &st) != 0) return 145;
    if ((st.st_mode & S_IFMT) != S_IFDIR) return 146;
    if (st.st_size != 0) return 147;
    if (access("/newlib-dir", X_OK) != 0) return 148;

    fd = open("/newlib-dir/probe.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 149;
    if (write(fd, "x", 1) != 1) return 150;
    if (close(fd) != 0) return 151;
    if (unlink("/newlib-dir/probe.txt") != 0) return 152;

    errno = 0;
    if (mkdir("/newlib-dir", 0) != -1) return 153;
    if (errno != EEXIST) return 154;

    if (rename("/newlib-path.txt", "/newlib-renamed.txt") != 0) return 155;

    errno = 0;
    if (stat("/newlib-path.txt", &st) != -1) return 156;
    if (errno != ENOENT) return 157;

    if (stat("/newlib-renamed.txt", &st) != 0) return 158;
    if ((st.st_mode & S_IFMT) != S_IFREG) return 159;
    if (st.st_size != 5) return 160;
    if (access("/newlib-renamed.txt", F_OK) != 0) return 161;

    fd = open("/newlib-renamed.txt", O_RDONLY, 0);
    if (fd < 0) return 162;
    if (read(fd, buf, 5) != 5) return 163;
    if (memcmp(buf, "bravo", 5) != 0) return 164;
    if (close(fd) != 0) return 165;

    if (unlink("/newlib-renamed.txt") != 0) return 166;
    errno = 0;
    if (stat("/newlib-renamed.txt", &st) != -1) return 167;
    if (errno != ENOENT) return 168;
    errno = 0;
    if (access("/newlib-renamed.txt", F_OK) != -1) return 169;
    if (errno != ENOENT) return 170;

    return write(1, "newlib-path-ok\n", 15) == 15 ? 0 : 171;
}

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE

int charm_posix_newlib_stdio_entry(void) {
    FILE* fp = fopen("/newlib-stdio.txt", "wb");
    if (fp == NULL) return 151;
    if (fwrite("echo", 1, 4, fp) != 4) return 152;
    if (fputc('\n', fp) == EOF) return 153;
    if (fflush(fp) != 0) return 154;
    if (fclose(fp) != 0) return 155;

    fp = fopen("/newlib-stdio.txt", "rb");
    if (fp == NULL) return 156;
    if (fseek(fp, 0, SEEK_END) != 0) return 157;
    if (ftell(fp) != 5) return 158;
    if (fseek(fp, 0, SEEK_SET) != 0) return 159;

    char buf[8] = {0};
    if (fread(buf, 1, 5, fp) != 5) return 160;
    if (memcmp(buf, "echo\n", 5) != 0) return 161;
    if (fclose(fp) != 0) return 162;

    if (remove("/newlib-stdio.txt") != 0) return 163;

    if (fputs("newlib-stdio-ok\n", stdout) == EOF) return 164;
    if (fflush(stdout) != 0) return 165;
    return 0;
}
#endif
