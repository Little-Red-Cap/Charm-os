module;

#include <cstddef>
#include <type_traits>
#include "charm_posix_user_fs.h"

#ifdef environ
#undef environ
#endif

export module posix.user_crt_c;

export import posix.user_crt;

import util.core;

namespace {
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

export extern "C" int charm_posix_argc(void) noexcept {
    return posix::user::argc();
}

export extern "C" char** charm_posix_argv(void) noexcept {
    return posix::user::argv();
}

export extern "C" char** charm_posix_envp(void) noexcept {
    return posix::user::envp();
}

export extern "C" char** charm_posix_environ(void) noexcept {
    return posix::user::environ();
}

export extern "C" const char* charm_posix_getenv(const char* key) noexcept {
    return key ? posix::user::getenv_cstr(key) : nullptr;
}

export extern "C" int* charm_posix_errno_location(void) noexcept {
    return posix::user::errno_location();
}

export extern "C" long long charm_posix_read(int fd, void* buf, std::size_t count) noexcept {
    return posix::user::read(fd, buf, static_cast<util::usize>(count));
}

export extern "C" long long charm_posix_write(int fd, const void* buf, std::size_t count) noexcept {
    return posix::user::write(fd, buf, static_cast<util::usize>(count));
}

export extern "C" int charm_posix_open(const char* path, int flags, int mode) noexcept {
    return posix::user::open(path, flags, mode);
}

export extern "C" int charm_posix_close(int fd) noexcept {
    return posix::user::close(fd);
}

export extern "C" int charm_posix_stat(const char* path, charm_posix_stat_t* out) noexcept {
    if (!out) {
        posix::set_errno(posix::EINVAL);
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

export extern "C" int charm_posix_fstat(int fd, charm_posix_stat_t* out) noexcept {
    if (!out) {
        posix::set_errno(posix::EINVAL);
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

export extern "C" int charm_posix_isatty(int fd) noexcept {
    auto* err = posix::user::errno_location();
    const int saved_errno = err ? *err : 0;
    const int rc = posix::user::isatty(fd);
    if (rc == 0 && err) {
        *err = saved_errno;
    }
    return rc;
}

export extern "C" long long charm_posix_lseek(int fd, long long offset, int whence) noexcept {
    return posix::user::lseek(fd, offset, whence);
}

export extern "C" int charm_posix_mkdir(const char* path) noexcept {
    return posix::user::mkdir(path);
}

export extern "C" int charm_posix_unlink(const char* path) noexcept {
    return posix::user::unlink(path);
}

export extern "C" int charm_posix_rmdir(const char* path) noexcept {
    return posix::user::rmdir(path);
}

export extern "C" int charm_posix_rename(const char* from, const char* to) noexcept {
    return posix::user::rename(from, to);
}

export extern "C" charm_posix_dir_t* charm_posix_opendir(const char* path) noexcept {
    return map_charm_posix_dir(posix::user::opendir(path));
}

export extern "C" const charm_posix_dirent_t* charm_posix_readdir(charm_posix_dir_t* dir) noexcept {
    return map_charm_posix_dirent(posix::user::readdir(map_posix_dir(dir)));
}

export extern "C" int charm_posix_closedir(charm_posix_dir_t* dir) noexcept {
    return posix::user::closedir(map_posix_dir(dir));
}

export extern "C" int charm_posix_chdir(const char* path) noexcept {
    return posix::user::chdir(path);
}

export extern "C" char* charm_posix_getcwd(char* buf, std::size_t size) noexcept {
    return posix::user::getcwd(buf, static_cast<util::usize>(size));
}

export extern "C" int charm_posix_getpid(void) noexcept {
    return posix::user::getpid();
}

export extern "C" int charm_posix_sleep(unsigned seconds) noexcept {
    return posix::user::sleep(seconds);
}

export extern "C" void charm_posix_exit(int code) noexcept {
    posix::user::exit(code);
}

export extern "C" void charm_posix_abort(void) noexcept {
    posix::user::abort();
}
