module;

#include <cstdint>

export module util.error;

import util.core;
export import util.expected;

export namespace util {
    // Unified error codes (core model).
    // Negative values follow common POSIX-style meanings for compatibility.
    enum class Errc : util::i32 {
        ok = 0,
        perm = -1,
        noent = -2,
        io = -5,
        again = -11,
        nomem = -12,
        busy = -16,
        exist = -17,
        inval = -22,
        rofs = -30,
        nametoolong = -36,
        nosys = -38,
        notsup = -95,
        timeout = -110,

        // Aliases (same numeric values)
        io_error = io,
        not_supported = notsup,
        invalid = inval,
        invalid_arg = inval,
        no_memory = nomem,
        would_block = again,

        // Non-POSIX extended codes
        end_of_stream = 1001,
        decode_error = 1002,
        bad_state = 1003,
        crc_error = 1004,
        format_error = 1005,
        canceled = 1006,
        closed = 1007,
        buffer_overflow = 1008,
        invalid_format = 1009,
    };

    template <class T>
    using Result = expected<T, Errc>;

    constexpr bool ok(Errc e) noexcept { return e == Errc::ok; }
}
