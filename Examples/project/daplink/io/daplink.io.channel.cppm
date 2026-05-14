module;

#include <span>
#include <utility>

export module daplink.io.channel;

import daplink.base.core;
import daplink.base.error;
import daplink.base.expected;
import daplink.base.types;

export namespace daplink::io {
    using ByteView = std::span<const daplink::base::u8>;
    using MutByteView = std::span<daplink::base::u8>;

    using errc = daplink::base::Errc;
    using result = daplink::base::Result<daplink::base::usize>;

    constexpr result ok(daplink::base::usize n) noexcept {
        return result{std::in_place, n};
    }

    constexpr result fail(errc e) noexcept {
        return daplink::base::unexpected(e);
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
            if (!ops.read) {
                return fail(errc::invalid);
            }
            auto r = ops.read(ctx, buf);
            if (r && r.value() == 0) {
                daplink::base::halt();
            }
            return r;
        }

        result write(ByteView buf) noexcept {
            if (!ops.write) {
                return fail(errc::invalid);
            }
            auto r = ops.write(ctx, buf);
            if (r && r.value() == 0) {
                daplink::base::halt();
            }
            return r;
        }

        result flush() noexcept {
            if (!ops.flush) {
                return fail(errc::not_supported);
            }
            return ops.flush(ctx);
        }
    };
}
