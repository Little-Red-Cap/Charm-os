//
// Minimal smoke tests for posix.fd_table (no framework).
//

module;
#include <array>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

export module posix.fd_table.tests;

#if defined(POSIX_FD_TABLE_SMOKE_TEST) && POSIX_FD_TABLE_SMOKE_TEST

import posix.fd_table;
import util.core;
import util.error;

namespace {
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void log_step(const char* label, bool ok) noexcept {
        std::printf("[posix-smoke] fd_table %s %s\n", label, ok ? "ok" : "fail");
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
            std::printf("[posix-smoke] fd_table %s fail: expected=%lld actual=%lld\n",
                label, to_ll(b), to_ll(a));
            fail();
        }
        log_step(label, true);
    }

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
        check_true("attach-0", r0);
        check_eq("attach-0-id", r0.value(), 0);

        posix::FdEntry e1 = e0;
        e1.ctx = &b;
        auto r1 = table.attach(e1, 1);
        check_true("attach-1", r1);
        check_eq("attach-1-id", r1.value(), 1);

        posix::FdEntry e2 = e0;
        e2.ctx = &a;
        auto r2 = table.attach(e2, 2);
        check_true("attach-2", r2);
        check_eq("attach-2-id", r2.value(), 2);

        auto rdup = table.dup2(1, 2);
        check_true("dup2-1-2", rdup);
        check_eq("dup2-close", a.closes, 1u);

        auto rget = table.get(2);
        check_true("get-2", rget);
        check_true("get-2-ctx", rget.value()->ctx == &b);

        auto rclose = table.close(0);
        check_true("close-0", rclose);
        check_eq("close-0-count", a.closes, 2u);
        check_true("get-0-closed", !table.get(0));
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
        check_true("clone-attach-0", r0);

        auto clone = table.clone_entry(0);
        check_true("clone-entry", clone);
        check_eq("clone-id", clone.value().id, -1);
        check_true("clone-ops", clone.value().ops == &ops);
    }
} // namespace

export void run_posix_fd_table_smoke_tests() noexcept {
    std::printf("[posix-smoke] fd_table begin\n");
    test_attach_dup_close();
    test_clone_entry();
    std::printf("[posix-smoke] fd_table end ok\n");
}

#endif
