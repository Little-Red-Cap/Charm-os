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
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void log_step(const char* label, bool ok) noexcept {
        std::printf("[posix-smoke] pipe %s %s\n", label, ok ? "ok" : "fail");
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
            std::printf("[posix-smoke] pipe %s fail: expected=%lld actual=%lld\n",
                label, to_ll(b), to_ll(a));
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
} // namespace

export void run_posix_pipe_smoke_tests() noexcept {
    std::printf("[posix-smoke] pipe begin\n");
    test_pipe_read_write();
    test_pipe_closed_reader();
    std::printf("[posix-smoke] pipe end ok\n");
}

#endif
