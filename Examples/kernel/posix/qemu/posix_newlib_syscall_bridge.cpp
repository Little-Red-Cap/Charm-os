#include <cerrno>
#include <cstddef>
#include <cstdarg>
#include <csignal>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    inline constexpr int kPosixOpenNonBlock = 0x800;
    inline constexpr int kRuntimeErrPerm = 1;
    inline constexpr int kRuntimeErrNoent = 2;
    inline constexpr int kRuntimeErrIo = 5;
    inline constexpr int kRuntimeErrBadf = 9;
    inline constexpr int kRuntimeErrAgain = 11;
    inline constexpr int kRuntimeErrNomem = 12;
    inline constexpr int kRuntimeErrAccess = 13;
    inline constexpr int kRuntimeErrBusy = 16;
    inline constexpr int kRuntimeErrExist = 17;
    inline constexpr int kRuntimeErrNotdir = 20;
    inline constexpr int kRuntimeErrIsdir = 21;
    inline constexpr int kRuntimeErrInval = 22;
    inline constexpr int kRuntimeErrNfile = 23;
    inline constexpr int kRuntimeErrMfile = 24;
    inline constexpr int kRuntimeErrNospc = 28;
    inline constexpr int kRuntimeErrSpipe = 29;
    inline constexpr int kRuntimeErrRofs = 30;
    inline constexpr int kRuntimeErrPipe = 32;
    inline constexpr int kRuntimeErrRange = 34;
    inline constexpr int kRuntimeErrNametoolong = 36;
    inline constexpr int kRuntimeErrNosys = 38;
    inline constexpr int kRuntimeErrNotempty = 39;
    inline constexpr int kRuntimeErrNotsup = 95;
    inline constexpr int kRuntimeErrTimedout = 110;

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
    inline constexpr std::size_t kNewlibHeapSize = 128u * 1024u;

    alignas(16) unsigned char g_newlib_heap[kNewlibHeapSize]{};
    unsigned char* g_newlib_brk = g_newlib_heap;
#endif

    int translate_runtime_errno_to_c(int runtime_err) noexcept;

    struct ErrnoScope {
        int c_errno{errno};
        int runtime_errno{*posix::user::errno_location()};

        void restore() const noexcept {
            errno = c_errno;
            *posix::user::errno_location() = runtime_errno;
        }

        int fail_from_runtime(int fallback = kRuntimeErrIo) const noexcept {
            int value = *posix::user::errno_location();
            if (value == 0) {
                value = fallback;
                *posix::user::errno_location() = value;
            }
            errno = translate_runtime_errno_to_c(value);
            return -1;
        }
    };

    int translate_runtime_errno_to_c(int runtime_err) noexcept {
        switch (runtime_err) {
            case kRuntimeErrPerm: return EPERM;
            case kRuntimeErrNoent: return ENOENT;
            case kRuntimeErrIo: return EIO;
            case kRuntimeErrBadf: return EBADF;
            case kRuntimeErrAgain: return EAGAIN;
            case kRuntimeErrNomem: return ENOMEM;
            case kRuntimeErrAccess: return EACCES;
            case kRuntimeErrBusy: return EBUSY;
            case kRuntimeErrExist: return EEXIST;
            case kRuntimeErrNotdir: return ENOTDIR;
            case kRuntimeErrIsdir: return EISDIR;
            case kRuntimeErrNfile: return ENFILE;
            case kRuntimeErrMfile: return EMFILE;
            case kRuntimeErrNospc: return ENOSPC;
            case kRuntimeErrSpipe: return ESPIPE;
            case kRuntimeErrRofs: return EROFS;
            case kRuntimeErrPipe: return EPIPE;
            case kRuntimeErrRange: return ERANGE;
            case kRuntimeErrNametoolong: return ENAMETOOLONG;
            case kRuntimeErrNosys: return ENOSYS;
            case kRuntimeErrNotempty: return ENOTEMPTY;
            case kRuntimeErrNotsup: return ENOTSUP;
            case kRuntimeErrTimedout: return ETIMEDOUT;
            case kRuntimeErrInval: return EINVAL;
            default: return EIO;
        }
    }

    void set_bridge_errno(int runtime_err) noexcept {
        *posix::user::errno_location() = runtime_err;
        errno = translate_runtime_errno_to_c(runtime_err);
    }

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
        if ((flags & O_NONBLOCK) != 0) out |= kPosixOpenNonBlock;
        return out;
    }

    int translate_runtime_status_flags_to_c(int flags) noexcept {
        int out = 0;
        switch (flags & O_ACCMODE) {
            case kPosixOpenWriteOnly:
                out |= O_WRONLY;
                break;
            case kPosixOpenReadWrite:
                out |= O_RDWR;
                break;
            case kPosixOpenReadOnly:
            default:
                out |= O_RDONLY;
                break;
        }
        if ((flags & kPosixOpenAppend) != 0) out |= O_APPEND;
        if ((flags & kPosixOpenNonBlock) != 0) out |= O_NONBLOCK;
        return out;
    }

    int translate_c_status_flags_to_runtime(int flags) noexcept {
        int out = 0;
        if ((flags & O_APPEND) != 0) out |= kPosixOpenAppend;
        if ((flags & O_NONBLOCK) != 0) out |= kPosixOpenNonBlock;
        return out;
    }

    bool has_only_access_mode_bits(int mode) noexcept {
        return (mode & ~(R_OK | W_OK | X_OK)) == 0;
    }

    bool has_any_perm(util::u32 mode_bits, util::u32 perm_mask) noexcept {
        return (mode_bits & perm_mask) != 0;
    }
}

extern "C" int _read(int fd, char* ptr, int len) {
    ErrnoScope guard{};
    if (len < 0) {
        set_bridge_errno(kRuntimeErrInval);
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
        set_bridge_errno(kRuntimeErrNomem);
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
        set_bridge_errno(kRuntimeErrInval);
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

extern "C" int _pipe(int* fds) {
    ErrnoScope guard{};
    if (!fds) {
        set_bridge_errno(kRuntimeErrInval);
        return -1;
    }
    const int r = posix::user::pipe(fds);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int pipe(int* fds) {
    return _pipe(fds);
}

extern "C" int _dup(int fd) {
    ErrnoScope guard{};
    const int r = posix::user::dup(fd);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int dup(int fd) {
    return _dup(fd);
}

extern "C" int _dup2(int oldfd, int newfd) {
    ErrnoScope guard{};
    const int r = posix::user::dup2(oldfd, newfd);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int dup2(int oldfd, int newfd) {
    return _dup2(oldfd, newfd);
}

namespace {
    bool fcntl_requires_int_arg(int cmd) noexcept {
        return cmd == F_DUPFD || cmd == F_SETFD || cmd == F_SETFL;
    }

    int fcntl_bridge_call(int fd, int cmd, int arg) noexcept {
        ErrnoScope guard{};
        int runtime_arg = arg;
        if (cmd == F_SETFL) {
            constexpr int kAllowedStatusFlags = O_APPEND | O_NONBLOCK;
            if ((arg & ~kAllowedStatusFlags) != 0) {
                set_bridge_errno(kRuntimeErrInval);
                return -1;
            }
            runtime_arg = translate_c_status_flags_to_runtime(arg);
        }
        const int r = posix::user::fcntl(fd, cmd, runtime_arg);
        if (r < 0) {
            return guard.fail_from_runtime();
        }
        guard.restore();
        if (cmd == F_GETFL) {
            return translate_runtime_status_flags_to_c(r);
        }
        return r;
    }
}

extern "C" int _fcntl(int fd, int cmd, ...) {
    int arg = 0;
    if (fcntl_requires_int_arg(cmd)) {
        va_list args;
        va_start(args, cmd);
        arg = va_arg(args, int);
        va_end(args);
    }
    return fcntl_bridge_call(fd, cmd, arg);
}

extern "C" int fcntl(int fd, int cmd, ...) {
    int arg = 0;
    if (fcntl_requires_int_arg(cmd)) {
        va_list args;
        va_start(args, cmd);
        arg = va_arg(args, int);
        va_end(args);
    }
    return fcntl_bridge_call(fd, cmd, arg);
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
        set_bridge_errno(kRuntimeErrInval);
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
        set_bridge_errno(kRuntimeErrInval);
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
        errno = translate_runtime_errno_to_c(*posix::user::errno_location());
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

extern "C" int _rmdir(const char* path) {
    ErrnoScope guard{};
    const int r = posix::user::rmdir(path);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int rmdir(const char* path) {
    return _rmdir(path);
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

extern "C" int _chdir(const char* path) {
    ErrnoScope guard{};
    const int r = posix::user::chdir(path);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return r;
}

extern "C" int chdir(const char* path) {
    return _chdir(path);
}

extern "C" char* getcwd(char* buf, std::size_t size) {
    ErrnoScope guard{};
    auto* result = posix::user::getcwd(buf, static_cast<util::usize>(size));
    if (!result) {
        guard.fail_from_runtime();
        return nullptr;
    }
    guard.restore();
    return result;
}

extern "C" int _access(const char* path, int mode) {
    ErrnoScope guard{};
    if (!has_only_access_mode_bits(mode)) {
        set_bridge_errno(kRuntimeErrInval);
        return -1;
    }

    posix::PosixStat st{};
    if (posix::user::stat(path, &st) < 0) {
        return guard.fail_from_runtime();
    }

    if ((mode & R_OK) != 0 && !has_any_perm(st.mode, 0444u)) {
        set_bridge_errno(kRuntimeErrAccess);
        return -1;
    }
    if ((mode & W_OK) != 0 && !has_any_perm(st.mode, 0222u)) {
        set_bridge_errno(kRuntimeErrAccess);
        return -1;
    }
    if ((mode & X_OK) != 0 && !has_any_perm(st.mode, 0111u)) {
        set_bridge_errno(kRuntimeErrAccess);
        return -1;
    }

    guard.restore();
    return 0;
}

extern "C" int access(const char* path, int mode) {
    return _access(path, mode);
}

extern "C" int remove(const char* path) {
    ErrnoScope guard{};
    posix::PosixStat st{};
    if (posix::user::stat(path, &st) < 0) {
        return guard.fail_from_runtime();
    }
    const bool is_dir = (st.mode & S_IFMT) == S_IFDIR;
    const int r = is_dir ? posix::user::rmdir(path) : posix::user::unlink(path);
    if (r < 0) {
        return guard.fail_from_runtime();
    }
    guard.restore();
    return 0;
}
