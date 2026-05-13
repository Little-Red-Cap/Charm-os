module;

#include <cstdint>

export module daplink.base.error;

import daplink.base.expected;
import daplink.base.types;

export namespace daplink::base {
    enum class Errc : daplink::base::i32 {
        ok = 0,
        perm = -1,
        noent = -2,
        io = -5,
        again = -11,
        nomem = -12,
        busy = -16,
        exist = -17,
        notdir = -20,
        isdir = -21,
        inval = -22,
        rofs = -30,
        nametoolong = -36,
        nosys = -38,
        notsup = -95,
        timeout = -110,

        io_error = io,
        not_supported = notsup,
        invalid = inval,
        invalid_arg = inval,
        not_dir = notdir,
        is_dir = isdir,
        no_memory = nomem,
        would_block = again,

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

    constexpr bool ok(Errc e) noexcept {
        return e == Errc::ok;
    }
}
