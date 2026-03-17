module;

#include <span>
#include <type_traits>

export module fs_stream;

import util.core;
import fs_errno;

export namespace fs {
    struct Status {
        Errc err{Errc::ok};
        constexpr explicit operator bool() const noexcept { return err == Errc::ok; }
    };

    template <class A, class B>
    concept SameAs = std::is_same_v<A, B> && std::is_same_v<B, A>;

    template <typename T>
    concept Stream = requires(T& s, std::span<util::u8> out, std::span<const util::u8> in, util::i64 off) {
        { s.read(out) } -> SameAs<Status>;
        { s.write(in) } -> SameAs<Status>;
        { s.flush() } -> SameAs<Status>;
        { s.seek(off) } -> SameAs<Status>;
    };
}
