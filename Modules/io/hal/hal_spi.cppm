module;

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <span>

export module hal_spi;

import hal_core;
import util.core;

export namespace hal {
    enum class SpiMode : util::u8 { mode0, mode1, mode2, mode3 };
    enum class SpiBitOrder : util::u8 { msb_first, lsb_first };

    struct SpiConfig {
        util::u32 hz{1000000};
        SpiMode mode{SpiMode::mode0};
        SpiBitOrder bit_order{SpiBitOrder::msb_first};
        util::u8 bits{8};
    };

    struct SpiHandle {
        util::usize id{0};
        void* impl{nullptr};
    };

    struct SpiOps {
        Result (*init)(void* ctx, const SpiConfig& cfg) noexcept { nullptr };
        Result (*enable)(void* ctx) noexcept { nullptr };
        Result (*disable)(void* ctx) noexcept { nullptr };
        Result (*transfer)(void* ctx,
                           std::span<const util::u8> tx,
                           std::span<util::u8> rx) noexcept { nullptr };
    };

    struct SpiIoHandle {
        void* ctx{nullptr};
        const SpiOps* ops{nullptr};
    };

    inline Result spi_init(SpiIoHandle h, const SpiConfig& cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, cfg);
    }

    inline Result spi_enable(SpiIoHandle h) noexcept {
        if (!h.ops || !h.ops->enable) return err(Status::unsupported);
        return h.ops->enable(h.ctx);
    }

    inline Result spi_disable(SpiIoHandle h) noexcept {
        if (!h.ops || !h.ops->disable) return err(Status::unsupported);
        return h.ops->disable(h.ctx);
    }

    inline Result spi_transfer(SpiIoHandle h,
                               std::span<const util::u8> tx,
                               std::span<util::u8> rx) noexcept {
        if (!h.ops || !h.ops->transfer) return err(Status::unsupported);
        return h.ops->transfer(h.ctx, tx, rx);
    }

    template <typename T>
    concept SpiDriver = requires(SpiHandle h, SpiConfig cfg,
                                 std::span<const util::u8> tx,
                                 std::span<util::u8> rx) {
        { T::init(h, cfg) } -> std::same_as<Result>;
        { T::enable(h) } -> std::same_as<Result>;
        { T::disable(h) } -> std::same_as<Result>;
        { T::transfer(h, tx, rx) } -> std::same_as<Result>;
    };
}
