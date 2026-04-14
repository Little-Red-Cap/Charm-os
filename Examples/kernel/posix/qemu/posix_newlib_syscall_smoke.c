#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int charm_posix_newlib_syscall_probe_entry(void) {
    char ch = 0;
    int stdin_fd = -1;
    int stdout_fd = -1;
    int stderr_fd = -1;
    int tty_fd = -1;
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

    stdin_fd = open("/dev/stdin", O_RDONLY, 0);
    if (stdin_fd < 0) return 102;
    if (isatty(stdin_fd) != 1) return 103;
    if (fstat(stdin_fd, &st) != 0) return 104;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 105;
    if (close(stdin_fd) != 0) return 106;

    stdout_fd = open("/dev/stdout", O_WRONLY, 0);
    if (stdout_fd < 0) return 107;
    errno = 77;
    if (isatty(stdout_fd) != 0) return 108;
    if (errno != 77) return 109;
    if (fstat(stdout_fd, &st) != 0) return 110;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 111;
    if (close(stdout_fd) != 0) return 112;

    stderr_fd = open("/dev/stderr", O_WRONLY, 0);
    if (stderr_fd < 0) return 113;
    if (isatty(stderr_fd) != 1) return 114;
    if (fstat(stderr_fd, &st) != 0) return 115;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 116;
    if (close(stderr_fd) != 0) return 117;

    tty_fd = open("/dev/tty", O_WRONLY, 0);
    if (tty_fd < 0) return 118;
    if (isatty(tty_fd) != 1) return 119;
    if (fstat(tty_fd, &st) != 0) return 120;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 121;
    if (write(tty_fd, "tty", 3) != 3) return 122;
    if (close(tty_fd) != 0) return 123;

    return write(1, "newlib-syscall-ok\n", 18) == 18 ? 0 : 124;
}

int charm_posix_newlib_dup_entry(void) {
    int dup_fd = dup(1);
    if (dup_fd < 0) return 161;
    if (dup_fd == 1) return 162;
    if (write(dup_fd, "dup-", 4) != 4) return 163;
    if (close(dup_fd) != 0) return 164;

    errno = 0;
    if (dup(-1) != -1) return 165;
    if (errno != EBADF) return 166;

    return write(1, "newlib-dup-ok\n", 14) == 14 ? 0 : 167;
}

int charm_posix_newlib_dup2_entry(void) {
    if (dup2(1, 2) != 2) return 171;
    if (write(2, "dup2-", 5) != 5) return 172;

    errno = 0;
    if (dup2(-1, 4) != -1) return 173;
    if (errno != EBADF) return 174;

    if (dup2(2, 2) != 2) return 175;

    return write(1, "newlib-dup2-ok\n", 15) == 15 ? 0 : 176;
}

int charm_posix_newlib_fcntl_entry(void) {
    int dup_fd = fcntl(1, F_DUPFD, 3);
    int flags = 0;
    if (dup_fd < 3) return 181;
    if (write(dup_fd, "fcntl-", 6) != 6) return 182;
    if (close(dup_fd) != 0) return 183;

    errno = 0;
    if (fcntl(-1, F_DUPFD, 3) != -1) return 184;
    if (errno != EBADF) return 185;

    errno = 0;
    if (fcntl(1, F_DUPFD, -1) != -1) return 186;
    if (errno != EINVAL) return 187;

    if (fcntl(1, F_GETFD) != 0) return 188;
    if (fcntl(1, F_SETFD, FD_CLOEXEC) != 0) return 189;
    if (fcntl(1, F_GETFD) != FD_CLOEXEC) return 190;

    errno = 0;
    if (fcntl(-1, F_GETFD) != -1) return 191;
    if (errno != EBADF) return 192;

    errno = 0;
    if (fcntl(1, F_SETFD, 2) != -1) return 193;
    if (errno != EINVAL) return 194;

    dup_fd = dup(1);
    if (dup_fd < 0) return 195;
    if (fcntl(dup_fd, F_GETFD) != 0) return 196;
    if (close(dup_fd) != 0) return 197;

    if (dup2(1, 2) != 2) return 198;
    if (fcntl(2, F_GETFD) != 0) return 199;

    dup_fd = fcntl(1, F_DUPFD, 3);
    if (dup_fd < 3) return 200;
    if (fcntl(dup_fd, F_GETFD) != 0) return 201;
    if (close(dup_fd) != 0) return 202;

    if (fcntl(1, F_SETFD, 0) != 0) return 203;
    if (fcntl(1, F_GETFD) != 0) return 204;

    flags = fcntl(1, F_GETFL);
    if (flags < 0) return 205;
    if ((flags & O_ACCMODE) != O_WRONLY) return 206;
    if ((flags & O_NONBLOCK) != 0) return 207;

    if (fcntl(1, F_SETFL, O_NONBLOCK) != 0) return 208;
    flags = fcntl(1, F_GETFL);
    if ((flags & O_NONBLOCK) == 0) return 209;

    dup_fd = dup(1);
    if (dup_fd < 0) return 210;
    if ((fcntl(dup_fd, F_GETFL) & O_NONBLOCK) == 0) return 211;
    if (fcntl(dup_fd, F_SETFL, 0) != 0) return 212;
    if ((fcntl(1, F_GETFL) & O_NONBLOCK) != 0) return 213;
    if (close(dup_fd) != 0) return 214;

    errno = 0;
    if (fcntl(1, F_SETFL, O_CREAT) != -1) return 215;
    if (errno != EINVAL) return 216;

    errno = 0;
    if (fcntl(-1, F_GETFL) != -1) return 217;
    if (errno != EBADF) return 218;

    return write(1, "newlib-fcntl-ok\n", 16) == 16 ? 0 : 219;
}

int charm_posix_newlib_pipe_entry(void) {
    int fds[2] = {-1, -1};
    int dup_fd = -1;
    int flags = 0;
    int filled = 0;
    int drained = 0;
    char ch = 0;
    char buf[8] = {0};

    if (pipe(fds) != 0) return 221;

    flags = fcntl(fds[0], F_GETFL);
    if (flags < 0) return 222;
    if ((flags & O_ACCMODE) != O_RDONLY) return 223;

    flags = fcntl(fds[1], F_GETFL);
    if (flags < 0) return 224;
    if ((flags & O_ACCMODE) != O_WRONLY) return 225;

    errno = 0;
    if (read(fds[0], &ch, 1) != -1) return 226;
    if (errno != EAGAIN) return 227;

    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) != 0) return 228;
    if ((fcntl(fds[0], F_GETFL) & O_NONBLOCK) == 0) return 229;

    dup_fd = dup(fds[0]);
    if (dup_fd < 0) return 230;
    if ((fcntl(dup_fd, F_GETFL) & O_NONBLOCK) == 0) return 231;
    if (close(dup_fd) != 0) return 232;

    for (;;) {
        int w = write(fds[1], "x", 1);
        if (w == 1) {
            ++filled;
            if (filled > 1024) return 233;
            continue;
        }
        if (w == -1 && errno == EAGAIN) {
            break;
        }
        return 234;
    }
    if (filled <= 0) return 235;

    while (drained < filled) {
        int want = filled - drained;
        if (want > (int)sizeof(buf)) want = (int)sizeof(buf);
        int r = read(fds[0], buf, want);
        if (r <= 0) return 236;
        drained += r;
    }

    if (close(fds[1]) != 0) return 237;
    if (read(fds[0], &ch, 1) != 0) return 238;
    if (close(fds[0]) != 0) return 239;

    return write(1, "newlib-pipe-ok\n", 15) == 15 ? 0 : 240;
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

    errno = 0;
    if (unlink("/newlib-dir") != -1) return 149;
    if (errno != EISDIR) return 150;

    errno = 0;
    if (rmdir("/newlib-path.txt") != -1) return 151;
    if (errno != ENOTDIR) return 152;

    fd = open("/newlib-dir/probe.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 153;
    if (write(fd, "x", 1) != 1) return 154;
    if (close(fd) != 0) return 155;

    errno = 0;
    if (rmdir("/newlib-dir") != -1) return 156;
    if (errno != ENOTEMPTY) return 157;

    if (unlink("/newlib-dir/probe.txt") != 0) return 158;
    if (rmdir("/newlib-dir") != 0) return 159;

    errno = 0;
    if (stat("/newlib-dir", &st) != -1) return 160;
    if (errno != ENOENT) return 161;

    errno = 0;
    if (mkdir("/newlib-dir", 0) != 0) return 162;

    if (rename("/newlib-path.txt", "/newlib-renamed.txt") != 0) return 163;

    errno = 0;
    if (stat("/newlib-path.txt", &st) != -1) return 164;
    if (errno != ENOENT) return 165;

    if (stat("/newlib-renamed.txt", &st) != 0) return 166;
    if ((st.st_mode & S_IFMT) != S_IFREG) return 167;
    if (st.st_size != 5) return 168;
    if (access("/newlib-renamed.txt", F_OK) != 0) return 169;

    fd = open("/newlib-renamed.txt", O_RDONLY, 0);
    if (fd < 0) return 170;
    if (read(fd, buf, 5) != 5) return 171;
    if (memcmp(buf, "bravo", 5) != 0) return 172;
    if (close(fd) != 0) return 173;

    if (unlink("/newlib-renamed.txt") != 0) return 174;
    errno = 0;
    if (stat("/newlib-renamed.txt", &st) != -1) return 175;
    if (errno != ENOENT) return 176;
    errno = 0;
    if (access("/newlib-renamed.txt", F_OK) != -1) return 177;
    if (errno != ENOENT) return 178;

    return write(1, "newlib-path-ok\n", 15) == 15 ? 0 : 179;
}

int charm_posix_newlib_cwd_entry(void) {
    char cwd[32] = {0};
    char small[2] = {0};
    int fd = -1;
    struct stat st;

    if (getcwd(cwd, sizeof(cwd)) != cwd) return 181;
    if (strcmp(cwd, "/") != 0) return 182;

    if (mkdir("/newlib-cwd", 0) != 0) return 183;
    if (mkdir("/newlib-cwd/sub", 0) != 0) return 184;

    if (chdir("/newlib-cwd") != 0) return 185;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 186;
    if (strcmp(cwd, "/newlib-cwd") != 0) return 187;

    fd = open("child.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 188;
    if (write(fd, "cwd", 3) != 3) return 189;
    if (close(fd) != 0) return 190;

    if (stat("/newlib-cwd/child.txt", &st) != 0) return 191;
    if ((st.st_mode & S_IFMT) != S_IFREG) return 192;
    if (st.st_size != 3) return 193;

    if (chdir("sub") != 0) return 194;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 195;
    if (strcmp(cwd, "/newlib-cwd/sub") != 0) return 196;

    fd = open("../parent.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 197;
    if (write(fd, "up", 2) != 2) return 198;
    if (close(fd) != 0) return 199;

    if (stat("/newlib-cwd/parent.txt", &st) != 0) return 200;
    if (st.st_size != 2) return 201;

    errno = 0;
    if (getcwd(small, sizeof(small)) != NULL) return 202;
    if (errno != ERANGE) return 203;

    if (chdir("..") != 0) return 204;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 205;
    if (strcmp(cwd, "/newlib-cwd") != 0) return 206;

    errno = 0;
    if (chdir("/missing-cwd") != -1) return 207;
    if (errno != ENOENT) return 208;

    if (chdir("/") != 0) return 209;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 210;
    if (strcmp(cwd, "/") != 0) return 211;

    return write(1, "newlib-cwd-ok\n", 14) == 14 ? 0 : 212;
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

    if (mkdir("/newlib-stdio-dir", 0) != 0) return 164;
    if (remove("/newlib-stdio-dir") != 0) return 165;

    if (fputs("newlib-stdio-ok\n", stdout) == EOF) return 166;
    if (fflush(stdout) != 0) return 167;
    return 0;
}
#endif
