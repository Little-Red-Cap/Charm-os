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
    int null_fd = -1;
    struct stat st;

    errno = 55;
    if (isatty(0) != 1) return 91;
    if (errno != 55) return 92;

    errno = 66;
    if (isatty(1) != 0) return 93;
    if (errno != 66) return 94;

    if (fstat(0, &st) != 0) return 95;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 96;

    errno = 0;
    if (fstat(0, (struct stat*)0) != -1) return 532;
    if (errno != EINVAL) return 533;

    if (fstat(1, &st) != 0) return 97;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 98;

    errno = 88;
    if (getpid() <= 0) return 99;
    if (errno != 88) return 556;

    errno = 0;
    if (isatty(-1) != 0) return 125;
    if (errno != EBADF) return 126;

    errno = 0;
    if (read(-1, &ch, 1) != -1) return 100;
    if (errno != EBADF) return 101;

    errno = 0;
    if (write(-1, "x", 1) != -1) return 102;
    if (errno != EBADF) return 103;

    errno = 0;
    if (fstat(-1, &st) != -1) return 104;
    if (errno != EBADF) return 105;

    errno = 0;
    if (close(-1) != -1) return 106;
    if (errno != EBADF) return 107;

    stdin_fd = open("/dev/stdin", O_RDONLY, 0);
    if (stdin_fd < 0) return 108;
    if (isatty(stdin_fd) != 1) return 109;
    if (fstat(stdin_fd, &st) != 0) return 110;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 111;
    errno = 89;
    if (close(stdin_fd) != 0) return 112;
    if (errno != 89) return 557;

    stdout_fd = open("/dev/stdout", O_WRONLY, 0);
    if (stdout_fd < 0) return 113;
    errno = 77;
    if (isatty(stdout_fd) != 0) return 114;
    if (errno != 77) return 115;
    if (fstat(stdout_fd, &st) != 0) return 116;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 117;
    if (close(stdout_fd) != 0) return 118;

    stderr_fd = open("/dev/stderr", O_WRONLY, 0);
    if (stderr_fd < 0) return 119;
    if (isatty(stderr_fd) != 1) return 120;
    if (fstat(stderr_fd, &st) != 0) return 121;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 122;
    if (close(stderr_fd) != 0) return 123;

    tty_fd = open("/dev/tty", O_WRONLY, 0);
    if (tty_fd < 0) return 124;
    if (isatty(tty_fd) != 1) return 125;
    if (fstat(tty_fd, &st) != 0) return 126;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 127;
    errno = 90;
    if (write(tty_fd, "tty", 3) != 3) return 128;
    if (errno != 90) return 558;
    if (close(tty_fd) != 0) return 129;

    errno = 91;
    null_fd = open("/dev/null", O_RDONLY, 0);
    if (null_fd < 0) return 633;
    if (errno != 91) return 634;
    errno = 92;
    if (read(null_fd, &ch, 1) != 0) return 635;
    if (errno != 92) return 636;
    errno = 93;
    if (isatty(null_fd) != 0) return 637;
    if (errno != 93) return 638;
    if (fstat(null_fd, &st) != 0) return 639;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 640;
    errno = 0;
    if (lseek(null_fd, 0, SEEK_SET) != (off_t)-1) return 641;
    if (errno != ESPIPE) return 642;
    if (close(null_fd) != 0) return 643;

    errno = 94;
    null_fd = open("/dev/null", O_WRONLY, 0);
    if (null_fd < 0) return 644;
    if (errno != 94) return 645;
    errno = 95;
    if (write(null_fd, "null", 4) != 4) return 646;
    if (errno != 95) return 647;
    if (close(null_fd) != 0) return 648;

    return write(1, "newlib-syscall-ok\n", 18) == 18 ? 0 : 130;
}

int charm_posix_newlib_dup_entry(void) {
    errno = 92;
    int dup_fd = dup(1);
    if (dup_fd < 0) return 161;
    if (errno != 92) return 584;
    if (dup_fd == 1) return 162;
    if (write(dup_fd, "dup-", 4) != 4) return 163;
    if (close(dup_fd) != 0) return 164;

    errno = 0;
    if (dup(-1) != -1) return 165;
    if (errno != EBADF) return 166;

    return write(1, "newlib-dup-ok\n", 14) == 14 ? 0 : 167;
}

int charm_posix_newlib_dup2_entry(void) {
    errno = 93;
    if (dup2(1, 2) != 2) return 171;
    if (errno != 93) return 585;
    if (write(2, "dup2-", 5) != 5) return 172;

    errno = 0;
    if (dup2(-1, 4) != -1) return 173;
    if (errno != EBADF) return 174;

    errno = 94;
    if (dup2(2, 2) != 2) return 175;
    if (errno != 94) return 586;

    return write(1, "newlib-dup2-ok\n", 15) == 15 ? 0 : 176;
}

int charm_posix_newlib_fcntl_entry(void) {
    errno = 95;
    int dup_fd = fcntl(1, F_DUPFD, 3);
    int alias_fd = -1;
    int stdin_pipe[2] = {-1, -1};
    int flags = 0;
    struct stat st;
    if (dup_fd < 3) return 181;
    if (errno != 95) return 587;
    if (write(dup_fd, "fcntl-", 6) != 6) return 182;
    if (close(dup_fd) != 0) return 183;

    alias_fd = open("/dev/stdout", O_WRONLY, 0);
    if (alias_fd < 0) return 184;
    if (isatty(alias_fd) != 0) return 185;
    if (fstat(alias_fd, &st) != 0) return 186;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 187;
    if ((fcntl(alias_fd, F_GETFL) & O_ACCMODE) != O_WRONLY) return 188;
    if (close(alias_fd) != 0) return 189;

    errno = 0;
    if (fcntl(-1, F_DUPFD, 3) != -1) return 190;
    if (errno != EBADF) return 191;

    errno = 0;
    if (fcntl(1, F_DUPFD, -1) != -1) return 192;
    if (errno != EINVAL) return 193;

    errno = 96;
    if (fcntl(1, F_GETFD) != 0) return 194;
    if (errno != 96) return 588;
    errno = 97;
    if (fcntl(1, F_SETFD, FD_CLOEXEC) != 0) return 195;
    if (errno != 97) return 589;
    if (fcntl(1, F_GETFD) != FD_CLOEXEC) return 196;

    errno = 0;
    if (fcntl(-1, F_GETFD) != -1) return 197;
    if (errno != EBADF) return 198;

    errno = 0;
    if (fcntl(1, F_SETFD, 2) != -1) return 199;
    if (errno != EINVAL) return 200;

    dup_fd = dup(1);
    if (dup_fd < 0) return 201;
    if (fcntl(dup_fd, F_GETFD) != 0) return 202;
    if (close(dup_fd) != 0) return 203;

    if (fcntl(0, F_SETFL, O_NONBLOCK) != 0) return 204;
    alias_fd = open("/dev/tty", O_RDONLY, 0);
    if (alias_fd < 0) return 205;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_RDONLY) return 206;
    if ((flags & O_NONBLOCK) == 0) return 207;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 208;
    if ((fcntl(0, F_GETFL) & O_NONBLOCK) != 0) return 209;
    if (close(alias_fd) != 0) return 210;

    flags = fcntl(2, F_GETFL);
    if (flags < 0) return 211;
    if ((flags & O_ACCMODE) != O_WRONLY) return 212;
    if ((flags & O_NONBLOCK) != 0) return 213;

    if (fcntl(2, F_SETFL, O_NONBLOCK) != 0) return 214;
    alias_fd = open("/dev/stderr", O_WRONLY, 0);
    if (alias_fd < 0) return 215;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_WRONLY) return 216;
    if ((flags & O_NONBLOCK) == 0) return 217;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 218;
    if ((fcntl(2, F_GETFL) & O_NONBLOCK) != 0) return 219;
    if (close(alias_fd) != 0) return 220;

    if (fcntl(2, F_SETFL, O_NONBLOCK) != 0) return 221;
    alias_fd = open("/dev/tty", O_WRONLY, 0);
    if (alias_fd < 0) return 222;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_WRONLY) return 223;
    if ((flags & O_NONBLOCK) == 0) return 224;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 225;
    if ((fcntl(2, F_GETFL) & O_NONBLOCK) != 0) return 226;
    if (close(alias_fd) != 0) return 227;

    if (fcntl(0, F_SETFL, O_NONBLOCK) != 0) return 300;
    alias_fd = open("/dev/console", O_RDONLY, 0);
    if (alias_fd < 0) return 301;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_RDONLY) return 302;
    if ((flags & O_NONBLOCK) == 0) return 303;
    if (isatty(alias_fd) != 1) return 304;
    if (fstat(alias_fd, &st) != 0) return 305;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 306;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 307;
    if ((fcntl(0, F_GETFL) & O_NONBLOCK) != 0) return 308;
    if (close(alias_fd) != 0) return 309;

    if (fcntl(2, F_SETFL, O_NONBLOCK) != 0) return 310;
    alias_fd = open("/dev/console", O_WRONLY, 0);
    if (alias_fd < 0) return 311;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_WRONLY) return 312;
    if ((flags & O_NONBLOCK) == 0) return 313;
    if (isatty(alias_fd) != 1) return 314;
    if (fstat(alias_fd, &st) != 0) return 315;
    if ((st.st_mode & S_IFMT) != S_IFCHR) return 316;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 317;
    if ((fcntl(2, F_GETFL) & O_NONBLOCK) != 0) return 318;
    if (close(alias_fd) != 0) return 319;

    if (dup2(1, 2) != 2) return 228;
    if (fcntl(2, F_GETFD) != 0) return 229;

    dup_fd = fcntl(1, F_DUPFD, 3);
    if (dup_fd < 3) return 230;
    if (fcntl(dup_fd, F_GETFD) != 0) return 231;
    if (close(dup_fd) != 0) return 232;

    if (fcntl(1, F_SETFD, 0) != 0) return 233;
    if (fcntl(1, F_GETFD) != 0) return 234;

    errno = 98;
    flags = fcntl(1, F_GETFL);
    if (flags < 0) return 235;
    if (errno != 98) return 590;
    if ((flags & O_ACCMODE) != O_WRONLY) return 236;
    if ((flags & O_NONBLOCK) != 0) return 237;

    errno = 99;
    if (fcntl(1, F_SETFL, O_NONBLOCK) != 0) return 238;
    if (errno != 99) return 591;
    flags = fcntl(1, F_GETFL);
    if ((flags & O_NONBLOCK) == 0) return 239;

    dup_fd = dup(1);
    if (dup_fd < 0) return 240;
    if ((fcntl(dup_fd, F_GETFL) & O_NONBLOCK) == 0) return 241;
    if (fcntl(dup_fd, F_SETFL, 0) != 0) return 242;
    if ((fcntl(1, F_GETFL) & O_NONBLOCK) != 0) return 243;
    if (close(dup_fd) != 0) return 244;

    errno = 0;
    if (fcntl(1, F_SETFL, O_CREAT) != -1) return 245;
    if (errno != EINVAL) return 246;

    errno = 0;
    if (fcntl(-1, F_GETFL) != -1) return 247;
    if (errno != EBADF) return 248;

    flags = fcntl(2, F_GETFL);
    if (flags < 0) return 249;
    if ((flags & O_ACCMODE) != O_WRONLY) return 250;
    if ((flags & O_NONBLOCK) != 0) return 251;

    if (fcntl(2, F_SETFL, O_NONBLOCK) != 0) return 252;
    if ((fcntl(2, F_GETFL) & O_NONBLOCK) == 0) return 253;

    dup_fd = dup(2);
    if (dup_fd < 0) return 254;
    if ((fcntl(dup_fd, F_GETFL) & O_NONBLOCK) == 0) return 255;
    if (fcntl(dup_fd, F_SETFL, 0) != 0) return 256;
    if ((fcntl(2, F_GETFL) & O_NONBLOCK) != 0) return 257;
    if (close(dup_fd) != 0) return 258;

    if (fcntl(2, F_SETFL, O_NONBLOCK) != 0) return 259;
    alias_fd = open("/dev/stderr", O_WRONLY, 0);
    if (alias_fd < 0) return 260;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_WRONLY) return 261;
    if ((flags & O_NONBLOCK) == 0) return 262;
    if (isatty(alias_fd) != 0) return 263;
    if (fstat(alias_fd, &st) != 0) return 264;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 265;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 266;
    if ((fcntl(2, F_GETFL) & O_NONBLOCK) != 0) return 267;
    if (close(alias_fd) != 0) return 268;

    if (fcntl(1, F_SETFL, O_NONBLOCK) != 0) return 269;
    alias_fd = open("/dev/stdout", O_WRONLY, 0);
    if (alias_fd < 0) return 270;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_WRONLY) return 271;
    if ((flags & O_NONBLOCK) == 0) return 272;
    if (isatty(alias_fd) != 0) return 273;
    if (fstat(alias_fd, &st) != 0) return 274;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 275;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 276;
    if ((fcntl(1, F_GETFL) & O_NONBLOCK) != 0) return 277;
    if (close(alias_fd) != 0) return 278;

    if (pipe(stdin_pipe) != 0) return 279;
    if (dup2(stdin_pipe[0], 0) != 0) return 280;
    if (close(stdin_pipe[0]) != 0) return 281;
    if (fcntl(0, F_SETFL, O_NONBLOCK) != 0) return 282;
    alias_fd = open("/dev/stdin", O_RDONLY, 0);
    if (alias_fd < 0) return 283;
    flags = fcntl(alias_fd, F_GETFL);
    if ((flags & O_ACCMODE) != O_RDONLY) return 284;
    if ((flags & O_NONBLOCK) == 0) return 285;
    errno = 88;
    if (isatty(alias_fd) != 0) return 286;
    if (errno != 88) return 287;
    if (fstat(alias_fd, &st) != 0) return 288;
    if ((st.st_mode & S_IFMT) != S_IFIFO) return 289;
    if (fcntl(alias_fd, F_SETFL, 0) != 0) return 290;
    if ((fcntl(0, F_GETFL) & O_NONBLOCK) != 0) return 291;
    if (close(alias_fd) != 0) return 292;
    if (close(stdin_pipe[1]) != 0) return 293;

    errno = 0;
    if (open("/dev/console", O_RDONLY, 0) != -1) return 320;
    if (errno != ENOENT) return 321;

    errno = 0;
    if (open("/dev/console", O_WRONLY, 0) != -1) return 322;
    if (errno != ENOENT) return 323;

    errno = 0;
    if (open("/dev/tty", O_RDONLY, 0) != -1) return 324;
    if (errno != ENOENT) return 325;

    errno = 0;
    if (open("/dev/tty", O_WRONLY, 0) != -1) return 326;
    if (errno != ENOENT) return 327;

    errno = 0;
    if (stat("/dev/console", &st) != -1) return 328;
    if (errno != ENOENT) return 329;

    errno = 0;
    if (stat("/dev/tty", &st) != -1) return 330;
    if (errno != ENOENT) return 331;

    return write(1, "newlib-fcntl-ok\n", 16) == 16 ? 0 : 294;
}

int charm_posix_newlib_pipe_entry(void) {
    int fds[2] = {-1, -1};
    int dup_fd = -1;
    int flags = 0;
    int filled = 0;
    int drained = 0;
    char ch = 0;
    char buf[8] = {0};

    errno = 0;
    if (pipe((int*)0) != -1) return 534;
    if (errno != EINVAL) return 535;

    errno = 100;
    if (pipe(fds) != 0) return 221;
    if (errno != 100) return 592;

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
    int self = getpid();
    if (self <= 0) return 111;

    errno = 0;
    if (kill(self, 1) != -1) return 112;
    if (errno != EINVAL) return 113;

    errno = 0;
    if (kill(self + 4096, SIGTERM) != -1) return 114;
    if (errno != ENOENT) return 115;

    if (write(1, "newlib-kill\n", 12) != 12) return 116;
    (void)kill(self, SIGTERM);
    return 117;
}

int charm_posix_newlib_lseek_entry(void) {
    char buf[6] = {0};
    off_t end = lseek(0, 0, SEEK_END);
    if (end != 5) return 121;
    if (lseek(0, 0, SEEK_SET) != 0) return 122;
    errno = 91;
    if (read(0, buf, 5) != 5) return 123;
    if (errno != 91) return 559;
    if (memcmp(buf, "alpha", 5) != 0) return 124;

    errno = 0;
    if (lseek(1, 0, SEEK_SET) != (off_t)-1) return 125;
    if (errno != ESPIPE) return 126;

    errno = 0;
    if (lseek(-1, 0, SEEK_SET) != (off_t)-1) return 127;
    if (errno != EBADF) return 128;

    errno = 0;
    if (lseek(0, 0, 99) != (off_t)-1) return 129;
    if (errno != EINVAL) return 130;

    return write(1, "newlib-lseek-ok\n", 16) == 16 ? 0 : 131;
}

int charm_posix_newlib_path_entry(void) {
    struct stat st;
    char buf[8] = {0};
    errno = 61;
    int fd = open("/newlib-path.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 131;
    if (errno != 61) return 550;
    if (write(fd, "bravo", 5) != 5) return 132;
    if (close(fd) != 0) return 133;

    errno = 71;
    if (stat("/newlib-path.txt", &st) != 0) return 134;
    if (errno != 71) return 542;
    if ((st.st_mode & S_IFMT) != S_IFREG) return 135;
    if (st.st_size != 5) return 136;

    errno = 0;
    if (stat("/newlib-path.txt", (struct stat*)0) != -1) return 530;
    if (errno != EINVAL) return 531;

    errno = 0;
    if (open((const char*)0, O_RDONLY, 0) != -1) return 560;
    if (errno != EINVAL) return 561;

    errno = 0;
    if (stat((const char*)0, &st) != -1) return 562;
    if (errno != EINVAL) return 563;

    errno = 77;
    if (access("/newlib-path.txt", F_OK) != 0) return 137;
    if (errno != 77) return 138;
    if (access("/newlib-path.txt", R_OK) != 0) return 138;
    if (errno != 77) return 543;
    if (access("/newlib-path.txt", W_OK) != 0) return 139;
    if (errno != 77) return 544;
    errno = 0;
    if (access("/newlib-path.txt", X_OK) != -1) return 140;
    if (errno != EACCES) return 141;
    errno = 0;
    if (access("/newlib-path.txt", 8) != -1) return 526;
    if (errno != EINVAL) return 527;
    errno = 0;
    if (access("/newlib-missing.txt", F_OK) != -1) return 142;
    if (errno != ENOENT) return 143;

    errno = 0;
    if (access((const char*)0, F_OK) != -1) return 564;
    if (errno != EINVAL) return 565;

    errno = 0;
    if (open("/newlib-missing.txt", O_RDONLY, 0) != -1) return 536;
    if (errno != ENOENT) return 537;

    errno = 62;
    if (mkdir("/newlib-dir", 0) != 0) return 144;
    if (errno != 62) return 551;

    errno = 0;
    if (mkdir("/newlib-dir", 0) != -1) return 522;
    if (errno != EEXIST) return 523;

    errno = 0;
    if (mkdir("/newlib-path.txt", 0) != -1) return 524;
    if (errno != EEXIST) return 525;

    errno = 0;
    if (mkdir((const char*)0, 0) != -1) return 566;
    if (errno != EINVAL) return 567;

    errno = 0;
    if (open("/newlib-dir", O_WRONLY, 0) != -1) return 538;
    if (errno != EISDIR) return 539;

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

    errno = 0;
    if (unlink((const char*)0) != -1) return 568;
    if (errno != EINVAL) return 569;

    errno = 0;
    if (rmdir((const char*)0) != -1) return 570;
    if (errno != EINVAL) return 571;

    errno = 0;
    if (mkdir("/newlib-path.txt/sub", 0) != -1) return 506;
    if (errno != ENOTDIR) return 507;

    errno = 0;
    if (unlink("/newlib-path.txt/sub") != -1) return 508;
    if (errno != ENOTDIR) return 509;

    errno = 0;
    if (rmdir("/newlib-path.txt/sub") != -1) return 510;
    if (errno != ENOTDIR) return 511;

    errno = 0;
    if (stat("/newlib-path.txt/sub", &st) != -1) return 512;
    if (errno != ENOTDIR) return 513;

    errno = 0;
    if (access("/newlib-path.txt/sub", F_OK) != -1) return 514;
    if (errno != ENOTDIR) return 515;

    errno = 0;
    if (open("/newlib-path.txt/sub", O_RDONLY, 0) != -1) return 540;
    if (errno != ENOTDIR) return 541;

    errno = 63;
    fd = open("/newlib-dir/probe.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (fd < 0) return 153;
    if (errno != 63) return 552;
    if (write(fd, "x", 1) != 1) return 154;
    if (close(fd) != 0) return 155;

    errno = 0;
    if (remove("/newlib-remove-missing.txt") != -1) return 516;
    if (errno != ENOENT) return 517;

    errno = 0;
    if (remove("/newlib-path.txt/sub") != -1) return 518;
    if (errno != ENOTDIR) return 519;

    errno = 0;
    if (remove("/newlib-dir") != -1) return 520;
    if (errno != ENOTEMPTY) return 521;

    errno = 0;
    if (remove((const char*)0) != -1) return 572;
    if (errno != EINVAL) return 573;

    errno = 0;
    if (rmdir("/newlib-dir") != -1) return 156;
    if (errno != ENOTEMPTY) return 157;

    if (unlink("/newlib-dir/probe.txt") != 0) return 158;
    errno = 66;
    if (rmdir("/newlib-dir") != 0) return 159;
    if (errno != 66) return 555;

    errno = 0;
    if (stat("/newlib-dir", &st) != -1) return 160;
    if (errno != ENOENT) return 161;

    errno = 0;
    if (mkdir("/newlib-dir", 0) != 0) return 162;

    errno = 0;
    if (rename("/newlib-missing.txt", "/newlib-dir/missing.txt") != -1) return 500;
    if (errno != ENOENT) return 501;

    errno = 0;
    if (rename("/newlib-path.txt/sub", "/newlib-dir/from-notdir.txt") != -1) return 502;
    if (errno != ENOTDIR) return 503;

    errno = 0;
    if (rename("/newlib-path.txt", "/newlib-path.txt/sub") != -1) return 504;
    if (errno != ENOTDIR) return 505;

    errno = 0;
    if (rename((const char*)0, "/newlib-dir/from-null.txt") != -1) return 574;
    if (errno != EINVAL) return 575;

    errno = 0;
    if (rename("/newlib-path.txt", (const char*)0) != -1) return 576;
    if (errno != EINVAL) return 577;

    errno = 64;
    if (rename("/newlib-path.txt", "/newlib-renamed.txt") != 0) return 163;
    if (errno != 64) return 553;

    errno = 0;
    if (stat("/newlib-path.txt", &st) != -1) return 164;
    if (errno != ENOENT) return 165;

    if (stat("/newlib-renamed.txt", &st) != 0) return 166;
    if ((st.st_mode & S_IFMT) != S_IFREG) return 167;
    if (st.st_size != 5) return 168;
    if (access("/newlib-renamed.txt", F_OK) != 0) return 169;

    errno = 67;
    fd = open("/newlib-renamed.txt", O_RDONLY | O_NONBLOCK, 0);
    if (fd < 0) return 593;
    if (errno != 67) return 594;
    if ((fcntl(fd, F_GETFL) & O_NONBLOCK) == 0) return 595;
    if (close(fd) != 0) return 596;

    errno = 68;
    fd = open("/newlib-renamed.txt", O_WRONLY | O_APPEND, 0);
    if (fd < 0) return 597;
    if (errno != 68) return 598;
    if (write(fd, "++", 2) != 2) return 599;
    if (close(fd) != 0) return 600;

    if (stat("/newlib-renamed.txt", &st) != 0) return 601;
    if (st.st_size != 7) return 602;

    fd = open("/newlib-renamed.txt", O_RDONLY, 0);
    if (fd < 0) return 170;
    if (read(fd, buf, 7) != 7) return 171;
    if (memcmp(buf, "bravo++", 7) != 0) return 172;
    if (close(fd) != 0) return 173;

    memset(buf, 0, sizeof(buf));
    errno = 69;
    fd = open("/newlib-rw.txt", O_CREAT | O_TRUNC | O_RDWR, 0);
    if (fd < 0) return 603;
    if (errno != 69) return 604;
    if ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDWR) return 605;
    if (write(fd, "rw", 2) != 2) return 606;
    if (lseek(fd, 0, SEEK_SET) != 0) return 607;
    if (read(fd, buf, 2) != 2) return 608;
    if (memcmp(buf, "rw", 2) != 0) return 609;
    if (lseek(fd, 0, SEEK_END) != 2) return 610;
    if (write(fd, "+", 1) != 1) return 611;
    if (close(fd) != 0) return 612;
    if (stat("/newlib-rw.txt", &st) != 0) return 613;
    if (st.st_size != 3) return 614;

    memset(buf, 0, sizeof(buf));
    fd = open("/newlib-rw.txt", O_RDONLY, 0);
    if (fd < 0) return 615;
    if (read(fd, buf, 3) != 3) return 616;
    if (memcmp(buf, "rw+", 3) != 0) return 617;
    if (close(fd) != 0) return 618;
    if (unlink("/newlib-rw.txt") != 0) return 619;

    errno = 70;
    fd = open("/newlib-excl.txt", O_CREAT | O_EXCL | O_WRONLY, 0);
    if (fd < 0) return 620;
    if (errno != 70) return 621;
    if (close(fd) != 0) return 622;

    errno = 0;
    if (open("/newlib-excl.txt", O_CREAT | O_EXCL | O_WRONLY, 0) != -1) return 623;
    if (errno != EEXIST) return 624;
    if (unlink("/newlib-excl.txt") != 0) return 625;

    errno = 71;
    fd = open("/newlib-create-readonly.txt", O_RDONLY | O_CREAT, 0);
    if (fd < 0) return 626;
    if (errno != 71) return 627;
    if ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY) return 628;
    if (close(fd) != 0) return 629;
    if (stat("/newlib-create-readonly.txt", &st) != 0) return 630;
    if (st.st_size != 0) return 631;
    if (unlink("/newlib-create-readonly.txt") != 0) return 632;

    errno = 65;
    if (unlink("/newlib-renamed.txt") != 0) return 174;
    if (errno != 65) return 554;
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

    errno = 81;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 181;
    if (errno != 81) return 545;
    if (strcmp(cwd, "/") != 0) return 182;

    if (mkdir("/newlib-cwd", 0) != 0) return 183;
    if (mkdir("/newlib-cwd/sub", 0) != 0) return 184;

    errno = 82;
    if (chdir("/newlib-cwd") != 0) return 185;
    if (errno != 82) return 546;
    errno = 83;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 186;
    if (errno != 83) return 547;
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

    errno = 0;
    if (chdir((const char*)0) != -1) return 578;
    if (errno != EINVAL) return 579;

    errno = 0;
    if (getcwd((char*)0, sizeof(cwd)) != NULL) return 580;
    if (errno != EINVAL) return 581;

    errno = 0;
    if (getcwd(cwd, 0) != NULL) return 582;
    if (errno != EINVAL) return 583;

    if (chdir("..") != 0) return 204;
    if (getcwd(cwd, sizeof(cwd)) != cwd) return 205;
    if (strcmp(cwd, "/newlib-cwd") != 0) return 206;

    errno = 0;
    if (chdir("/missing-cwd") != -1) return 207;
    if (errno != ENOENT) return 208;

    errno = 0;
    if (chdir("/newlib-cwd/child.txt") != -1) return 212;
    if (errno != ENOTDIR) return 213;

    errno = 0;
    if (chdir("/newlib-cwd/child.txt/sub") != -1) return 214;
    if (errno != ENOTDIR) return 215;

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

    errno = 91;
    if (remove("/newlib-stdio.txt") != 0) return 163;
    if (errno != 91) return 548;

    if (mkdir("/newlib-stdio-dir", 0) != 0) return 164;
    errno = 92;
    if (remove("/newlib-stdio-dir") != 0) return 165;
    if (errno != 92) return 549;

    if (fputs("newlib-stdio-ok\n", stdout) == EOF) return 166;
    if (fflush(stdout) != 0) return 167;
    return 0;
}
#endif
