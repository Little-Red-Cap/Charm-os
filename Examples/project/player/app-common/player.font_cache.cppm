module;

#include <cstdint>
#include <string_view>

export module player.font_cache;

import charm.font;

export namespace player::font_cache {
    struct Stats {
        std::uint32_t requests{};
        std::uint32_t cached{};
        std::uint32_t missing{};
    };

    struct Backend {
        using InitFn = bool (*)(void* ctx) noexcept;
        using FallbackFontFn = const Font* (*)(void* ctx) noexcept;
        using EnsureTextFn = void (*)(void* ctx, std::string_view text) noexcept;
        using ReadyFn = bool (*)(void* ctx) noexcept;
        using StatsFn = Stats (*)(void* ctx) noexcept;
        using ResetStatsFn = void (*)(void* ctx) noexcept;

        void* ctx{nullptr};
        InitFn init_fn{nullptr};
        FallbackFontFn fallback_font_fn{nullptr};
        EnsureTextFn ensure_text_fn{nullptr};
        ReadyFn ready_fn{nullptr};
        StatsFn stats_fn{nullptr};
        ResetStatsFn reset_stats_fn{nullptr};
    };

    namespace detail {
        inline Backend backend{};
    }

    inline void set_backend(Backend backend) noexcept {
        detail::backend = backend;
    }

    inline void reset_backend() noexcept {
        detail::backend = {};
    }

    inline bool init() noexcept {
        const auto& backend = detail::backend;
        return backend.init_fn && backend.init_fn(backend.ctx);
    }

    inline const Font* fallback_font() noexcept {
        const auto& backend = detail::backend;
        return backend.fallback_font_fn ? backend.fallback_font_fn(backend.ctx) : nullptr;
    }

    inline void ensure_text(std::string_view text) noexcept {
        const auto& backend = detail::backend;
        if (backend.ensure_text_fn && !text.empty()) {
            backend.ensure_text_fn(backend.ctx, text);
        }
    }

    inline void ensure_text(const char* text) noexcept {
        ensure_text(text ? std::string_view{text} : std::string_view{});
    }

    inline bool ready() noexcept {
        const auto& backend = detail::backend;
        return backend.ready_fn && backend.ready_fn(backend.ctx);
    }

    inline Stats stats() noexcept {
        const auto& backend = detail::backend;
        return backend.stats_fn ? backend.stats_fn(backend.ctx) : Stats{};
    }

    inline void reset_stats() noexcept {
        const auto& backend = detail::backend;
        if (backend.reset_stats_fn) {
            backend.reset_stats_fn(backend.ctx);
        }
    }
}
