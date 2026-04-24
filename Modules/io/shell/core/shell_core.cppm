module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module shell_core;

import util.core;
import util.error;

export namespace shell {
    typedef util::Errc Errc;

    struct Result {
        Errc err{Errc::ok};
        constexpr explicit operator bool() const noexcept { return err == Errc::ok; }
    };

    constexpr Result ok() noexcept { return Result{Errc::ok}; }
    constexpr Result err(Errc e) noexcept { return Result{e}; }

    struct Buffer {
        const char* data{nullptr};
        util::usize size{0};
    };

    using WriteFn = util::usize (*)(void*, Buffer) noexcept;

    struct Console {
        void* ctx{nullptr};
        WriteFn write{nullptr};
    };

    inline Console make_console(WriteFn fn, void* ctx = nullptr) noexcept {
        return Console{ctx, fn};
    }
}
