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
        std::snprintf(buf, sizeof(buf), "[posix-smoke] fd_table %s %s", label, ok ? "ok" : "fail");
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
    template <class T>
    inline void check_true(const char* label, const T& v) noexcept {
        if (!static_cast<bool>(v)) {
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
                "[posix-smoke] fd_table %s fail: expected=%lld actual=%lld",
                label, to_ll(b), to_ll(a));
            log_line(buf);
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

    util::Result<void> dummy_dup(void*) noexcept { return {}; }

    void test_attach_dup_close() noexcept {
        posix::FdTable<4> table{};
        table.init();

        Counter a{};
        Counter b{};
        posix::FdOps ops{
            &dummy_read,
            &dummy_write,
            &dummy_close,
            &dummy_stat,
            &dummy_dup
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
            &dummy_stat,
            &dummy_dup
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

    void test_attach_errors() noexcept {
        posix::FdTable<2> table{};
        table.init();

        posix::FdEntry bad{};
        auto rbad = table.attach(bad, 0);
        check_true("err-null-ops", !rbad);
        check_eq("err-null-ops-code", rbad.error(), util::Errc::invalid_arg);

        posix::FdOps ops{
            &dummy_read,
            &dummy_write,
            &dummy_close,
            &dummy_stat,
            &dummy_dup
        };
        posix::FdEntry e{};
        e.kind = posix::FdKind::file;
        e.ops = &ops;

        auto r0 = table.attach(e, 0);
        check_true("err-attach-0", r0);
        auto rexist = table.attach(e, 0);
        check_true("err-exist", !rexist);
        check_eq("err-exist-code", rexist.error(), util::Errc::exist);

        auto rbadidx = table.attach(e, 5);
        check_true("err-bad-index", !rbadidx);
        check_eq("err-bad-index-code", rbadidx.error(), util::Errc::invalid_arg);

        auto r1 = table.attach(e, 1);
        check_true("err-attach-1", r1);
        auto rfull = table.attach(e);
        check_true("err-full", !rfull);
        check_eq("err-full-code", rfull.error(), util::Errc::buffer_overflow);
    }
} // namespace

export void run_posix_fd_table_smoke_tests() noexcept {
    log_line("[posix-smoke] fd_table begin");
    test_attach_dup_close();
    test_clone_entry();
    test_attach_errors();
    log_line("[posix-smoke] fd_table end ok");
}

#endif
