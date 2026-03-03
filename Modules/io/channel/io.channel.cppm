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
            auto r = ops.read(ctx, buf);
            if (r && r.value() == 0) util::halt();
            return r;
        }

        result write(ByteView buf) noexcept {
            if (!ops.write) return fail(errc::invalid);
            auto r = ops.write(ctx, buf);
            if (r && r.value() == 0) util::halt();
            return r;
        }

        result flush() noexcept {
            if (!ops.flush) return fail(errc::not_supported);
            return ops.flush(ctx);
        }
    };

}

namespace io::detail {
}

export namespace io {
}
