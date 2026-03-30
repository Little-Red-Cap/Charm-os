//
// Minimal smoke tests for posix.fd_table (no framework).
//

module;
#include <array>
#include <cstdlib>

export module posix.fd_table.tests;

#if defined(POSIX_FD_TABLE_SMOKE_TEST) && POSIX_FD_TABLE_SMOKE_TEST

import posix.fd_table;
import util.core;
import util.error;

namespace {
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void assert_true(bool v) noexcept { if (!v) fail(); }
    template <class A, class B>
    inline void assert_eq(const A& a, const B& b) noexcept { if (!(a == b)) fail(); }

    struct Counter {
        util::u32 closes{0};
    };

    util::Result<util::usize> dummy_read(void*, posix::MutByteView) noexcept {
        return util::unexpected(util::Errc::would_block);
    }

    util::Result<util::usize> dummy_write(void*, posix::ByteView) noexcept {
        return util::unexpected(util::Errc::would_block);
    }

    util::Result<void> dummy_close(void* ctx) noexcept {
        auto* c = static_cast<Counter*>(ctx);
        if (c) {
            ++c->closes;
        }
        return {};
    }

    util::Result<void> dummy_stat(void*, posix::PosixStat&) noexcept {
        return {};
    }

    void test_attach_dup_close() noexcept {
        posix::FdTable<4> table{};
        table.init();

        Counter a{};
        Counter b{};
        posix::FdOps ops{
            &dummy_read,
            &dummy_write,
            &dummy_close,
            &dummy_stat
        };

        posix::FdEntry e0{};
        e0.kind = posix::FdKind::term;
        e0.ops = &ops;
        e0.ctx = &a;
        auto r0 = table.attach(e0, 0);
        assert_true(r0);
        assert_eq(r0.value(), 0);

        posix::FdEntry e1 = e0;
        e1.ctx = &b;
        auto r1 = table.attach(e1, 1);
        assert_true(r1);
        assert_eq(r1.value(), 1);

        posix::FdEntry e2 = e0;
        e2.ctx = &a;
        auto r2 = table.attach(e2, 2);
        assert_true(r2);
        assert_eq(r2.value(), 2);

        auto rdup = table.dup2(1, 2);
        assert_true(rdup);
        assert_eq(a.closes, 1u);

        auto rget = table.get(2);
        assert_true(rget);
        assert_true(rget.value()->ctx == &b);

        auto rclose = table.close(0);
        assert_true(rclose);
        assert_eq(a.closes, 2u);
        assert_true(!table.get(0));
    }

    void test_clone_entry() noexcept {
        posix::FdTable<2> table{};
        table.init();

        Counter c{};
        posix::FdOps ops{
            &dummy_read,
            &dummy_write,
            &dummy_close,
            &dummy_stat
        };

        posix::FdEntry e0{};
        e0.kind = posix::FdKind::file;
        e0.ops = &ops;
        e0.ctx = &c;
        auto r0 = table.attach(e0, 0);
        assert_true(r0);

        auto clone = table.clone_entry(0);
        assert_true(clone);
        assert_eq(clone.value().id, -1);
        assert_true(clone.value().ops == &ops);
    }
} // namespace

export void run_posix_fd_table_smoke_tests() noexcept {
    test_attach_dup_close();
    test_clone_entry();
}

#endif
