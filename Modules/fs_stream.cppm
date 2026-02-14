module;

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <span>

export module fs_stream;

import util.core;
import fs_errno;

export namespace fs {
    struct Status {
        Err err{Err::ok};
        constexpr explicit operator bool() const noexcept { return err == Err::ok; }
    };

    template <typename T>
    concept Stream = requires(T& s, std::span<util::u8> out, std::span<const util::u8> in, util::i64 off) {
        { s.read(out) } -> std::same_as<Status>;
        { s.write(in) } -> std::same_as<Status>;
        { s.flush() } -> std::same_as<Status>;
        { s.seek(off) } -> std::same_as<Status>;
    };
}
