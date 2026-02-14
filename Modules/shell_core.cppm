module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module shell_core;

import util.core;

export namespace shell {
    enum class Errno : util::u8 {
        ok = 0,
        perm,
        noent,
        io,
        busy,
        inval,
        nomem,
        nosys
    };

    struct Result {
        Errno err{Errno::ok};
        constexpr explicit operator bool() const noexcept { return err == Errno::ok; }
    };

    constexpr Result ok() noexcept { return Result{Errno::ok}; }
    constexpr Result err(Errno e) noexcept { return Result{e}; }

    struct Buffer {
        const char* data{nullptr};
        util::usize size{0};
    };

    using WriteFn = util::usize (*)(Buffer) noexcept;

    struct Console {
        WriteFn write{nullptr};
    };
}
