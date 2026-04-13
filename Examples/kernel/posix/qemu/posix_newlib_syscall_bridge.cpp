#include <cerrno>
#include <cstdarg>
#include <csignal>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
#include <cstddef>
#endif
#include <unistd.h>

import posix.fd_table;
import posix.user_crt;
import posix.user_runtime;
import util.core;

namespace {
    inline constexpr int kPosixOpenReadOnly = 0x0;
    inline constexpr int kPosixOpenWriteOnly = 0x1;
    inline constexpr int kPosixOpenReadWrite = 0x2;
    inline constexpr int kPosixOpenCreate = 0x40;
    inline constexpr int kPosixOpenTrunc = 0x200;
    inline constexpr int kPosixOpenAppend = 0x400;

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
    inline constexpr std::size_t kNewlibHeapSize = 128u * 1024u;

    alignas(16) unsigned char g_newlib_heap[kNewlibHeapSize]{};
    unsigned char* g_newlib_brk = g_newlib_heap;
#endif

    struct ErrnoScope {
        int c_errno{errno};
        int runtime_errno{*posix::user::errno_location()};

        void restore() const noexcept {
            errno = c_errno;
            *posix::user::errno_location() = runtime_errno;
        }

        int fail_from_runtime(int fallback = EIO) const noexcept {
            int value = *posix::user::errno_location();
            if (value == 0) {
                value = fallback;
                *posix::user::errno_location() = value;
            }
            errno = value;
            return -1;
        }
    };

    void map_stat(const posix::PosixStat& in, struct stat& out) noexcept {
        out = {};
        out.st_mode = static_cast<mode_t>(in.mode);
        out.st_size = static_cast<off_t>(in.size);
        out.st_nlink = 1;
    }

    int translate_open_flags(int flags) noexcept {
        int out = 0;
        switch (flags & O_ACCMODE) {
            case O_WRONLY:
                out |= kPosixOpenWriteOnly;
                break;
            case O_RDWR:
                out |= kPosixOpenReadWrite;
                break;
            case O_RDONLY:
            default:
                out |= kPosixOpenReadOnly;
                break;
        }
        if ((flags & O_CREAT) != 0) out |= kPosixOpenCreate;
        if ((flags & O_TRUNC) != 0) out |= kPosixOpenTrunc;
        if ((flags & O_APPEND) != 0) out |= kPosixOpenAppend;
        return out;
    }
}

extern "C" int _read(int fd, char* ptr, int len) {
    ErrnoScope guard{};
    if (len < 0) {
        errno = EINVAL;
        *posix::user::errno_location() = EINVAL;
        return -1;
    }
    auto r = posix::user::read(fd, ptr, static_cast<util::usize>(len));
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return static_cast<int>(r);
}

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
extern "C" caddr_t _sbrk(int incr) {
    const auto used = static_cast<std::ptrdiff_t>(g_newlib_brk - g_newlib_heap);
    const auto next = used + static_cast<std::ptrdiff_t>(incr);
    if (next < 0 || static_cast<std::size_t>(next) > kNewlibHeapSize) {
        errno = ENOMEM;
        *posix::user::errno_location() = ENOMEM;
        return reinterpret_cast<caddr_t>(-1);
    }
    auto* previous = g_newlib_heap + used;
    g_newlib_brk = g_newlib_heap + next;
    return reinterpret_cast<caddr_t>(previous);
}
#endif

extern "C" int _write(int fd, char* ptr, int len) {
    ErrnoScope guard{};
    if (len < 0) {
        errno = EINVAL;
        *posix::user::errno_location() = EINVAL;
        return -1;
    }
    auto r = posix::user::write(fd, ptr, static_cast<util::usize>(len));
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return static_cast<int>(r);
}

extern "C" int _close(int fd) {
    ErrnoScope guard{};
    const int r = posix::user::close(fd);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int _open(const char* path, int flags, ...) {
    ErrnoScope guard{};
    int mode = 0;
    if ((flags & O_CREAT) != 0) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    const int r = posix::user::open(path, translate_open_flags(flags), mode);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int _fstat(int fd, struct stat* st) {
    ErrnoScope guard{};
    if (!st) {
        errno = EINVAL;
        *posix::user::errno_location() = EINVAL;
        return -1;
    }
    posix::PosixStat pst{};
    const int r = posix::user::fstat(fd, &pst);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    map_stat(pst, *st);
    guard.restore();
    return 0;
}

extern "C" int _stat(const char* path, struct stat* st) {
    ErrnoScope guard{};
    if (!st) {
        errno = EINVAL;
        *posix::user::errno_location() = EINVAL;
        return -1;
    }
    posix::PosixStat pst{};
    const int r = posix::user::stat(path, &pst);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    map_stat(pst, *st);
    guard.restore();
    return 0;
}

extern "C" int _isatty(int fd) {
    ErrnoScope guard{};
    const int r = posix::user::isatty(fd);
    if (r == 0 && *posix::user::errno_location() != 0) {
        errno = *posix::user::errno_location();
        return 0;
    }
    guard.restore();
    return r;
}

extern "C" off_t _lseek(int fd, off_t ptr, int dir) {
    ErrnoScope guard{};
    auto r = posix::user::lseek(fd, static_cast<util::i64>(ptr), dir);
    if (r < 0) {
        guard.fail_from_runtime();
        return static_cast<off_t>(-1);
    }
    guard.restore();
    return static_cast<off_t>(r);
}

extern "C" int _getpid(void) {
    ErrnoScope guard{};
    const int pid = posix::user::getpid();
    guard.restore();
    return pid;
}

extern "C" int _kill(int pid, int sig) {
    ErrnoScope guard{};
    const int r = posix::user::kill(pid, sig);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int _unlink(const char* path) {
    ErrnoScope guard{};
    const int r = posix::user::unlink(path);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int _mkdir(const char* path, int) {
    ErrnoScope guard{};
    const int r = posix::user::mkdir(path);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int mkdir(const char* path, mode_t mode) {
    return _mkdir(path, static_cast<int>(mode));
}

extern "C" int _rename(const char* from, const char* to) {
    ErrnoScope guard{};
    const int r = posix::user::rename(from, to);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int rename(const char* from, const char* to) {
    return _rename(from, to);
}
