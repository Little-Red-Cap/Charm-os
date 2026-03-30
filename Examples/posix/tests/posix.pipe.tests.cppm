//
// Minimal smoke tests for posix.pipe (no framework).
//

module;
#include <array>
#include <cstdlib>

export module posix.pipe.tests;

#if defined(POSIX_PIPE_SMOKE_TEST) && POSIX_PIPE_SMOKE_TEST

import posix.fd_table;
import posix.pipe;
import util.core;
import util.error;

namespace {
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void assert_true(bool v) noexcept { if (!v) fail(); }
    template <class A, class B>
    inline void assert_eq(const A& a, const B& b) noexcept { if (!(a == b)) fail(); }

    void test_pipe_read_write() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        assert_true(r);
        const auto ends = r.value();

        auto read_entry = table.get(ends.read_fd);
        auto write_entry = table.get(ends.write_fd);
        assert_true(read_entry);
        assert_true(write_entry);

        const std::array<util::u8, 3> msg{ 'a', 'b', 'c' };
        auto w = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        assert_true(w);
        assert_eq(w.value(), msg.size());

        std::array<util::u8, 3> out{};
        auto rd = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{out.data(), out.size()});
        assert_true(rd);
        assert_eq(rd.value(), out.size());
        assert_eq(out[0], msg[0]);
        assert_eq(out[1], msg[1]);
        assert_eq(out[2], msg[2]);

        (void)table.close(ends.write_fd);
        auto rd2 = read_entry.value()->ops->read(read_entry.value()->ctx,
            posix::MutByteView{out.data(), out.size()});
        assert_true(!rd2);
        assert_eq(rd2.error(), util::Errc::end_of_stream);
    }

    void test_pipe_closed_reader() noexcept {
        posix::FdTable<8> table{};
        table.init();
        posix::PipeService<2, 8> pipes{};
        pipes.init();

        auto r = pipes.create(table);
        assert_true(r);
        const auto ends = r.value();

        auto write_entry = table.get(ends.write_fd);
        assert_true(write_entry);

        (void)table.close(ends.read_fd);
        const std::array<util::u8, 1> msg{ 'x' };
        auto w = write_entry.value()->ops->write(write_entry.value()->ctx,
            posix::ByteView{msg.data(), msg.size()});
        assert_true(!w);
        assert_eq(w.error(), util::Errc::closed);
    }
} // namespace

export void run_posix_pipe_smoke_tests() noexcept {
    test_pipe_read_write();
    test_pipe_closed_reader();
}

#endif
