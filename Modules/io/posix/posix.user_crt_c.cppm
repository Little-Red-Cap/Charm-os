module;

#include <stddef.h>
#include <cstddef>
#include <type_traits>

#ifdef environ
#undef environ
#endif

export module posix.user_crt_c;

export import posix.user_crt;
import util.core;

#define CHARM_POSIX_HEADER_SKIP_STDDEF 1
export {
#include "charm_posix_user_fs.h"
}
#undef CHARM_POSIX_HEADER_SKIP_STDDEF

namespace {
    inline void sync_user_errno(int value) noexcept {
        posix::set_errno(value);
        if (auto* err = posix::user::errno_location()) {
            *err = value;
        }
    }

    inline void map_charm_posix_stat(const posix::PosixStat& in, charm_posix_stat_t& out) noexcept {
        out.st_mode = static_cast<charm_posix_mode_t>(in.mode);
        out.st_size = static_cast<unsigned long long>(in.size);
    }

    static_assert(std::is_standard_layout_v<charm_posix_dir_t>);
    static_assert(std::is_standard_layout_v<charm_posix_dirent_t>);
    static_assert(sizeof(charm_posix_dir_t) == sizeof(posix::PosixDir));
    static_assert(alignof(charm_posix_dir_t) == alignof(posix::PosixDir));
    static_assert(sizeof(charm_posix_dirent_t) == sizeof(posix::PosixDirent));
    static_assert(alignof(charm_posix_dirent_t) == alignof(posix::PosixDirent));

    inline charm_posix_dir_t* map_charm_posix_dir(posix::PosixDir* dir) noexcept {
        return reinterpret_cast<charm_posix_dir_t*>(dir);
    }

    inline const charm_posix_dirent_t* map_charm_posix_dirent(const posix::PosixDirent* ent) noexcept {
        return reinterpret_cast<const charm_posix_dirent_t*>(ent);
    }

    inline posix::PosixDir* map_posix_dir(charm_posix_dir_t* dir) noexcept {
        return reinterpret_cast<posix::PosixDir*>(dir);
    }
}

extern "C" int charm_posix_argc(void) noexcept {
    return posix::user::argc();
}

extern "C" char** charm_posix_argv(void) noexcept {
    return posix::user::argv();
}

extern "C" char** charm_posix_envp(void) noexcept {
    return posix::user::envp();
}

extern "C" char** charm_posix_environ(void) noexcept {
    return posix::user::environ();
}

extern "C" const char* charm_posix_getenv(const char* key) noexcept {
    return key ? posix::user::getenv_cstr(key) : nullptr;
}

extern "C" int* charm_posix_errno_location(void) noexcept {
    return posix::user::errno_location();
}

extern "C" long long charm_posix_read(int fd, void* buf, std::size_t count) noexcept {
    return posix::user::read(fd, buf, static_cast<util::usize>(count));
}

extern "C" long long charm_posix_write(int fd, const void* buf, std::size_t count) noexcept {
    return posix::user::write(fd, buf, static_cast<util::usize>(count));
}

extern "C" int charm_posix_open(const char* path, int flags, int mode) noexcept {
    return posix::user::open(path, flags, mode);
}

extern "C" int charm_posix_close(int fd) noexcept {
    return posix::user::close(fd);
}

extern "C" int charm_posix_stat(const char* path, charm_posix_stat_t* out) noexcept {
    if (!out) {
        sync_user_errno(posix::EINVAL);
        return -1;
    }
    posix::PosixStat st{};
    const int rc = posix::user::stat(path, &st);
    if (rc < 0) {
        return rc;
    }
    map_charm_posix_stat(st, *out);
    return 0;
}

extern "C" int charm_posix_fstat(int fd, charm_posix_stat_t* out) noexcept {
    if (!out) {
        sync_user_errno(posix::EINVAL);
        return -1;
    }
    posix::PosixStat st{};
    const int rc = posix::user::fstat(fd, &st);
    if (rc < 0) {
        return rc;
    }
    map_charm_posix_stat(st, *out);
    return 0;
}

extern "C" int charm_posix_isatty(int fd) noexcept {
    auto* err = posix::user::errno_location();
    const int saved_errno = err ? *err : 0;
    const int rc = posix::user::isatty(fd);
    if (rc == 0 && err && *err == 0) {
        *err = saved_errno;
    }
    return rc;
}

extern "C" long long charm_posix_lseek(int fd, long long offset, int whence) noexcept {
    return posix::user::lseek(fd, offset, whence);
}

extern "C" int charm_posix_mkdir(const char* path) noexcept {
    return posix::user::mkdir(path);
}

extern "C" int charm_posix_unlink(const char* path) noexcept {
    return posix::user::unlink(path);
}

extern "C" int charm_posix_rmdir(const char* path) noexcept {
    return posix::user::rmdir(path);
}

extern "C" int charm_posix_rename(const char* from, const char* to) noexcept {
    return posix::user::rename(from, to);
}

extern "C" charm_posix_dir_t* charm_posix_opendir(const char* path) noexcept {
    return map_charm_posix_dir(posix::user::opendir(path));
}

extern "C" const charm_posix_dirent_t* charm_posix_readdir(charm_posix_dir_t* dir) noexcept {
    return map_charm_posix_dirent(posix::user::readdir(map_posix_dir(dir)));
}

extern "C" int charm_posix_closedir(charm_posix_dir_t* dir) noexcept {
    return posix::user::closedir(map_posix_dir(dir));
}

extern "C" int charm_posix_chdir(const char* path) noexcept {
    return posix::user::chdir(path);
}

extern "C" char* charm_posix_getcwd(char* buf, std::size_t size) noexcept {
    return posix::user::getcwd(buf, static_cast<util::usize>(size));
}

extern "C" int charm_posix_getpid(void) noexcept {
    return posix::user::getpid();
}

extern "C" int charm_posix_sleep(unsigned seconds) noexcept {
    return posix::user::sleep(seconds);
}

extern "C" void charm_posix_exit(int code) noexcept {
    posix::user::exit(code);
}

extern "C" void charm_posix_abort(void) noexcept {
    posix::user::abort();
}
