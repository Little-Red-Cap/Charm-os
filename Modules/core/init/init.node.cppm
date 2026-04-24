module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module init.node;

import util.core;
import util.error;

export namespace init {
    using CapId = util::u32;

    enum class Phase : util::u8 {
        early,
        core,
        service,
        app,
    };

    enum class Runlevel : util::u32 {
        none = 0,
        tiny = 1u << 0,
        full = 1u << 1,
        all = 0xFFFFFFFFu,
    };

    constexpr util::u32 operator|(Runlevel a, Runlevel b) noexcept {
        return static_cast<util::u32>(a) | static_cast<util::u32>(b);
    }

    using InitFn = util::Result<void> (*)(void* ctx) noexcept;
    using DeinitFn = void (*)(void* ctx) noexcept;

    struct Node {
        std::string_view name{};
        Phase phase{Phase::core};
        util::u32 runlevel_mask{static_cast<util::u32>(Runlevel::all)};
        std::span<const CapId> provides{};
        std::span<const CapId> requires_caps{};
        InitFn init{nullptr};
        DeinitFn deinit{nullptr};
        void* ctx{nullptr};
    };

    constexpr CapId cap_id(std::string_view sv) {
        CapId hash = 2166136261u;
        for (unsigned char c : sv) {
            hash ^= static_cast<CapId>(c);
            hash *= 16777619u;
        }
        return hash == 0 ? 1u : hash;
    }

    template <std::size_t N>
    consteval CapId cap_id(const char (&literal)[N]) {
        return cap_id(std::string_view{literal, N > 0 ? (N - 1) : 0});
    }
}
