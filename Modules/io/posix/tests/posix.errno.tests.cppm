//
// Minimal smoke tests for posix.errno (no framework).
//

module;
#include <cstdlib>

export module posix.errno.tests;

#if defined(POSIX_ERRNO_SMOKE_TEST) && POSIX_ERRNO_SMOKE_TEST

import posix.errno;
import util.error;

namespace {
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void assert_true(bool v) noexcept { if (!v) fail(); }
    template <class A, class B>
    inline void assert_eq(const A& a, const B& b) noexcept { if (!(a == b)) fail(); }

    void test_errno_roundtrip() noexcept {
        posix::set_errno(0);
        assert_eq(posix::get_errno(), 0);
        posix::set_errno(posix::ENOENT);
        assert_eq(posix::get_errno(), posix::ENOENT);
    }

    void test_mapping() noexcept {
        assert_eq(posix::to_errno(util::Errc::noent), posix::ENOENT);
        assert_eq(posix::from_errno(posix::ENOENT), util::Errc::noent);
        assert_eq(posix::from_errno(posix::EPIPE), util::Errc::closed);
        assert_eq(posix::to_errno(util::Errc::nosys), posix::ENOSYS);
        assert_eq(posix::to_errno(util::Errc::notsup), posix::ENOTSUP);
        assert_eq(posix::to_errno(util::Errc::timeout), posix::ETIMEDOUT);
        assert_eq(posix::to_errno(util::Errc::exist), posix::EEXIST);
        assert_eq(posix::from_errno(posix::EACCES), util::Errc::io);
        assert_eq(posix::from_errno(123456), util::Errc::io);
    }
} // namespace

export void run_posix_errno_smoke_tests() noexcept {
    test_errno_roundtrip();
    test_mapping();
}

#endif
