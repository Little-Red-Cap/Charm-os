module;

#include <cstddef>

#ifdef environ
#undef environ
#endif

export module posix.user_crt_c;

export import posix.user_crt;

import util.core;

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

export extern "C" int charm_posix_getpid(void) noexcept {
    return posix::user::getpid();
}

export extern "C" int charm_posix_sleep(unsigned seconds) noexcept {
    return posix::user::sleep(seconds);
}

export extern "C" [[noreturn]] void charm_posix_exit(int code) noexcept {
    posix::user::exit(code);
}

export extern "C" [[noreturn]] void charm_posix_abort(void) noexcept {
    posix::user::abort();
}
