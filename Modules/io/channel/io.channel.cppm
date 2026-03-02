module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

export module io.channel;

import util.core;
import util.error;
import util.expected;

export namespace io {
    using ByteView = std::span<const util::u8>;
    using MutByteView = std::span<util::u8>;

    using errc = util::Errc;
    using result = util::Result<util::usize>;

    constexpr result ok(util::usize n) noexcept {
        return result{std::in_place, n};
    }

    constexpr result fail(errc e) noexcept {
        return util::unexpected(e);
    }

    using ReadFn = result (*)(void* ctx, MutByteView buf) noexcept;
    using WriteFn = result (*)(void* ctx, ByteView buf) noexcept;
    using FlushFn = result (*)(void* ctx) noexcept;

    struct ChannelOps {
        ReadFn read{};
        WriteFn write{};
        FlushFn flush{};
    };

    struct Channel {
        void* ctx{};
        ChannelOps ops{};

        result read(MutByteView buf) noexcept {
            if (!ops.read) return fail(errc::invalid);
            return ops.read(ctx, buf);
        }

        result write(ByteView buf) noexcept {
            if (!ops.write) return fail(errc::invalid);
            return ops.write(ctx, buf);
        }

        result flush() noexcept {
            if (!ops.flush) return ok(0);
            return ops.flush(ctx);
        }
    };

    Channel* default_console_channel() noexcept;
    void set_default_console_channel(Channel* ch) noexcept;

    using tick_t = util::u64;
    using NowFn = tick_t (*)(void* ctx) noexcept;
    tick_t now_ms() noexcept;
    void set_now_ms_provider(NowFn fn, void* ctx) noexcept;
}

namespace io::detail {
    inline Channel* g_default_console_channel = nullptr;
    inline NowFn g_now_ms_fn = nullptr;
    inline void* g_now_ms_ctx = nullptr;
}

export namespace io {
    inline Channel* default_console_channel() noexcept {
        return detail::g_default_console_channel;
    }

    inline void set_default_console_channel(Channel* ch) noexcept {
        detail::g_default_console_channel = ch;
    }

    inline tick_t now_ms() noexcept {
        if (!detail::g_now_ms_fn) return 0;
        return detail::g_now_ms_fn(detail::g_now_ms_ctx);
    }

    inline void set_now_ms_provider(NowFn fn, void* ctx) noexcept {
        detail::g_now_ms_fn = fn;
        detail::g_now_ms_ctx = ctx;
    }
}
