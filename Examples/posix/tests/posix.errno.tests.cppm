//
// Minimal smoke tests for posix.errno (no framework).
//

module;
#include <cstdio>
#include <cstdlib>
#include <type_traits>

export module posix.errno.tests;

#if defined(POSIX_ERRNO_SMOKE_TEST) && POSIX_ERRNO_SMOKE_TEST

import posix.errno;
import util.error;

namespace {
#if defined(POSIX_SMOKE_USE_UART) && POSIX_SMOKE_USE_UART
    extern "C" void posix_smoke_emit(const char* msg) noexcept;
#endif
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void log_line(const char* msg) noexcept {
#if defined(POSIX_SMOKE_USE_UART) && POSIX_SMOKE_USE_UART
        posix_smoke_emit(msg);
#else
        std::printf("%s\n", msg);
#endif
    }
    inline void log_step(const char* label, bool ok) noexcept {
        char buf[96]{};
        std::snprintf(buf, sizeof(buf), "[posix-smoke] errno %s %s", label, ok ? "ok" : "fail");
        log_line(buf);
    }
    template <class T>
    inline long long to_ll(const T& v) noexcept {
        if constexpr (std::is_enum_v<T>) {
            return static_cast<long long>(static_cast<std::underlying_type_t<T>>(v));
        } else {
            return static_cast<long long>(v);
        }
    }
    inline void check_true(const char* label, bool v) noexcept {
        if (!v) {
            log_step(label, false);
            fail();
        }
        log_step(label, true);
    }
    template <class A, class B>
    inline void check_eq(const char* label, const A& a, const B& b) noexcept {
        if (!(a == b)) {
            char buf[128]{};
            std::snprintf(buf, sizeof(buf),
                "[posix-smoke] errno %s fail: expected=%lld actual=%lld",
                label, to_ll(b), to_ll(a));
            log_line(buf);
            fail();
        }
        log_step(label, true);
    }

    void test_errno_roundtrip() noexcept {
        posix::set_errno(0);
        check_eq("roundtrip-0", posix::get_errno(), 0);
        posix::set_errno(posix::ENOENT);
        check_eq("roundtrip-enoent", posix::get_errno(), posix::ENOENT);
    }

    void test_mapping() noexcept {
        check_eq("map-noent", posix::to_errno(util::Errc::noent), posix::ENOENT);
        check_eq("map-enoent-back", posix::from_errno(posix::ENOENT), util::Errc::noent);
        check_eq("map-epipe-back", posix::from_errno(posix::EPIPE), util::Errc::closed);
        check_eq("map-nosys", posix::to_errno(util::Errc::nosys), posix::ENOSYS);
        check_eq("map-notsup", posix::to_errno(util::Errc::notsup), posix::ENOTSUP);
        check_eq("map-timeout", posix::to_errno(util::Errc::timeout), posix::ETIMEDOUT);
        check_eq("map-exist", posix::to_errno(util::Errc::exist), posix::EEXIST);
        check_eq("map-eacces-back", posix::from_errno(posix::EACCES), util::Errc::io);
        check_eq("map-unknown-back", posix::from_errno(123456), util::Errc::io);
    }
} // namespace

export void run_posix_errno_smoke_tests() noexcept {
    log_line("[posix-smoke] errno begin");
    test_errno_roundtrip();
    test_mapping();
    log_line("[posix-smoke] errno end ok");
}

#endif
