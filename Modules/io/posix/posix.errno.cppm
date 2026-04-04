module;

#include <cstdint>

export module posix.errno;

import util.core;
import util.error;

export namespace posix {
    // v1 policy:
    // - core APIs return util::Errc
    // - POSIX-facing wrappers call to_errno() when mapping to errno
    // - from_errno() is best-effort and intentionally lossy

    inline constexpr int EPERM = 1;
    inline constexpr int ENOENT = 2;
    inline constexpr int EIO = 5;
    inline constexpr int EBADF = 9;
    inline constexpr int EAGAIN = 11;
    inline constexpr int ENOMEM = 12;
    inline constexpr int EACCES = 13;
    inline constexpr int EBUSY = 16;
    inline constexpr int EEXIST = 17;
    inline constexpr int ENOTDIR = 20;
    inline constexpr int EISDIR = 21;
    inline constexpr int ENFILE = 23;
    inline constexpr int EMFILE = 24;
    inline constexpr int ENOSPC = 28;
    inline constexpr int EROFS = 30;
    inline constexpr int EPIPE = 32;
    inline constexpr int ENAMETOOLONG = 36;
    inline constexpr int ENOSYS = 38;
    inline constexpr int ENOTSUP = 95;
    inline constexpr int ETIMEDOUT = 110;
    inline constexpr int EINVAL = 22;

    inline int& errno_ref() noexcept {
#if defined(__arm__) || defined(__thumb__)
        static int value = 0;
#else
        static thread_local int value = 0;
#endif
        return value;
    }

    inline int get_errno() noexcept { return errno_ref(); }
    inline void set_errno(int value) noexcept { errno_ref() = value; }

    inline int to_errno(util::Errc err) noexcept {
        switch (err) {
            case util::Errc::ok: return 0;
            case util::Errc::perm: return EPERM;
            case util::Errc::noent: return ENOENT;
            case util::Errc::io: return EIO;
            case util::Errc::again: return EAGAIN;
            case util::Errc::nomem: return ENOMEM;
            case util::Errc::busy: return EBUSY;
            case util::Errc::exist: return EEXIST;
            case util::Errc::notdir: return ENOTDIR;
            case util::Errc::isdir: return EISDIR;
            case util::Errc::inval: return EINVAL;
            case util::Errc::rofs: return EROFS;
            case util::Errc::nametoolong: return ENAMETOOLONG;
            case util::Errc::nosys: return ENOSYS;
            case util::Errc::notsup: return ENOTSUP;
            case util::Errc::timeout: return ETIMEDOUT;
            case util::Errc::closed: return EIO;
            case util::Errc::buffer_overflow: return EIO;
            default: return EIO;
        }
    }

    inline util::Errc from_errno(int err) noexcept {
        switch (err) {
            case 0: return util::Errc::ok;
            case EPERM: return util::Errc::perm;
            case ENOENT: return util::Errc::noent;
            case EIO: return util::Errc::io;
            case EBADF: return util::Errc::noent;
            case EAGAIN: return util::Errc::again;
            case ENOMEM: return util::Errc::nomem;
            case EBUSY: return util::Errc::busy;
            case EEXIST: return util::Errc::exist;
            case ENOTDIR: return util::Errc::notdir;
            case EISDIR: return util::Errc::isdir;
            case EINVAL: return util::Errc::inval;
            case EROFS: return util::Errc::rofs;
            case ENAMETOOLONG: return util::Errc::nametoolong;
            case ENOSYS: return util::Errc::nosys;
            case ENOTSUP: return util::Errc::notsup;
            case ETIMEDOUT: return util::Errc::timeout;
            case EPIPE: return util::Errc::closed;
            case ENOSPC: return util::Errc::io;
            default: return util::Errc::io;
        }
    }
}
