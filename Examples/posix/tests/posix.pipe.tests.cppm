//
// Minimal smoke tests for posix.pipe (no framework).
//

module;
#include <array>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

export module posix.pipe.tests;

#if defined(POSIX_PIPE_SMOKE_TEST) && POSIX_PIPE_SMOKE_TEST

import posix.fd_table;
import posix.pipe;
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
        std::snprintf(buf, sizeof(buf), "[posix-smoke] pipe %s %s", label, ok ? "ok" : "fail");
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
                "[posix-smoke] pipe %s fail: expected=%lld actual=%lld",
                label, to_ll(b), to_ll(a));
            log_line(buf);
            fail();
        }
        log_step(label, true);
    }

    void test_pipe_read_write() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        check_true("create", r);
        const auto ends = r.value();

        auto read_entry = table.get(ends.read_fd);
        auto write_entry = table.get(ends.write_fd);
        check_true("get-read", read_entry);
        check_true("get-write", write_entry);

        std::array<util::u8, 1> empty{};
        auto w0 = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{empty.data(), 0});
        check_true("write-empty", w0);
        check_eq("write-empty-size", w0.value(), 0u);
        auto r0 = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{empty.data(), 0});
        check_true("read-empty", r0);
        check_eq("read-empty-size", r0.value(), 0u);

        const std::array<util::u8, 3> msg{ 'a', 'b', 'c' };
        auto w = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        check_true("write", w);
        check_eq("write-size", w.value(), msg.size());

        std::array<util::u8, 3> out{};
        auto rd = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{out.data(), out.size()});
        check_true("read", rd);
        check_eq("read-size", rd.value(), out.size());
        check_eq("read-0", out[0], msg[0]);
        check_eq("read-1", out[1], msg[1]);
        check_eq("read-2", out[2], msg[2]);

        (void)table.close(ends.write_fd);
        auto rd2 = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{out.data(), out.size()});
        check_true("read-eof", !rd2);
        check_eq("read-eof-err", rd2.error(), util::Errc::end_of_stream);
    }

    void test_pipe_closed_reader() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        check_true("close-create", r);
        const auto ends = r.value();

        auto write_entry = table.get(ends.write_fd);
        check_true("close-get-write", write_entry);

        (void)table.close(ends.read_fd);
        const std::array<util::u8, 1> msg{ 'x' };
        auto w = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        check_true("write-closed", !w);
        check_eq("write-closed-err", w.error(), util::Errc::closed);
    }

    void test_pipe_close_read_then_write() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        check_true("close-rw-create", r);
        const auto ends = r.value();

        auto rclose = table.close(ends.read_fd);
        check_true("close-rw-read-close", rclose);

        auto write_entry = table.get(ends.write_fd);
        check_true("close-rw-get-write", write_entry);
        const std::array<util::u8, 1> msg{ 'x' };
        auto w = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        check_true("close-rw-write", !w);
        check_eq("close-rw-write-err", w.error(), util::Errc::closed);
    }

    void test_pipe_close_write_then_read() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        check_true("close-wr-create", r);
        const auto ends = r.value();

        auto wclose = table.close(ends.write_fd);
        check_true("close-wr-write-close", wclose);

        auto read_entry = table.get(ends.read_fd);
        check_true("close-wr-get-read", read_entry);
        std::array<util::u8, 1> out{};
        auto rd = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{out.data(), out.size()});
        check_true("close-wr-read", !rd);
        check_eq("close-wr-read-err", rd.error(), util::Errc::end_of_stream);
    }

    void test_pipe_dup_reader() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        check_true("dup-create", r);
        const auto ends = r.value();

        auto rdup = table.dup2(ends.read_fd, 3);
        check_true("dup-read", rdup);
        auto rclose = table.close(ends.read_fd);
        check_true("dup-close-orig", rclose);

        auto write_entry = table.get(ends.write_fd);
        check_true("dup-get-write", write_entry);
        const std::array<util::u8, 1> msg{ 'x' };
        auto w = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        check_true("dup-write", w);

        auto read_entry = table.get(3);
        check_true("dup-get-read", read_entry);
        std::array<util::u8, 1> out{};
        auto rd = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{out.data(), out.size()});
        check_true("dup-read", rd);

        auto rclose2 = table.close(3);
        check_true("dup-close-last", rclose2);
        auto w2 = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        check_true("dup-write-closed", !w2);
        check_eq("dup-write-closed-err", w2.error(), util::Errc::closed);
    }
} // namespace

export void run_posix_pipe_smoke_tests() noexcept {
    log_line("[posix-smoke] pipe begin");
    test_pipe_read_write();
    test_pipe_closed_reader();
    test_pipe_close_read_then_write();
    test_pipe_close_write_then_read();
    test_pipe_dup_reader();
    log_line("[posix-smoke] pipe end ok");
}

#endif
