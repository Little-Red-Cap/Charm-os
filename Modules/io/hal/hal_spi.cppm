module;

#include <cstdint>
#include <span>

export module hal_spi;

import hal_core;
import util.core;

export namespace hal {
    enum class SpiMode : util::u8 { mode0, mode1, mode2, mode3 };
    enum class BitOrder : util::u8 { msb_first, lsb_first };

    struct SpiConfig {
        util::u32 hz{1000000};
        SpiMode mode{SpiMode::mode0};
        BitOrder bit_order{BitOrder::msb_first};
        util::u8 bits_per_word{8};
    };

    struct SpiOps {
        Result (*init)(void* ctx, const SpiConfig& cfg) noexcept { nullptr };
        Result (*transfer)(void* ctx, std::span<const util::u8> tx, std::span<util::u8> rx) noexcept { nullptr };
        Result (*set_speed)(void* ctx, util::u32 hz) noexcept { nullptr };
    };

    struct SpiHandle {
        void* ctx{nullptr};
        const SpiOps* ops{nullptr};
    };

    inline Result spi_init(SpiHandle h, const SpiConfig& cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, cfg);
    }

    inline Result spi_transfer(SpiHandle h, std::span<const util::u8> tx, std::span<util::u8> rx) noexcept {
        if (!h.ops || !h.ops->transfer) return err(Status::unsupported);
        return h.ops->transfer(h.ctx, tx, rx);
    }

    inline Result spi_set_speed(SpiHandle h, util::u32 hz) noexcept {
        if (!h.ops || !h.ops->set_speed) return err(Status::unsupported);
        return h.ops->set_speed(h.ctx, hz);
    }
}
