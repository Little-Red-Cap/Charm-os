module;

#include <cstdint>
#include <span>

export module hal_i2c;

import hal_core;
import util.core;

export namespace hal {
    struct I2cConfig {
        util::u32 hz{400000};
        bool ten_bit_addr{false};
    };

    struct I2cOps {
        Result (*init)(void* ctx, const I2cConfig& cfg) noexcept { nullptr };
        Result (*write)(void* ctx, util::u16 address, std::span<const util::u8> data) noexcept { nullptr };
        Result (*read)(void* ctx, util::u16 address, std::span<util::u8> data) noexcept { nullptr };
        Result (*write_read)(void* ctx,
                             util::u16 address,
                             std::span<const util::u8> tx,
                             std::span<util::u8> rx) noexcept { nullptr };
    };

    struct I2cHandle {
        void* ctx{nullptr};
        const I2cOps* ops{nullptr};
    };

    inline Result i2c_init(I2cHandle h, const I2cConfig& cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, cfg);
    }

    inline Result i2c_write(I2cHandle h, util::u16 address, std::span<const util::u8> data) noexcept {
        if (!h.ops || !h.ops->write) return err(Status::unsupported);
        return h.ops->write(h.ctx, address, data);
    }

    inline Result i2c_read(I2cHandle h, util::u16 address, std::span<util::u8> data) noexcept {
        if (!h.ops || !h.ops->read) return err(Status::unsupported);
        return h.ops->read(h.ctx, address, data);
    }

    inline Result i2c_write_read(I2cHandle h,
                                 util::u16 address,
                                 std::span<const util::u8> tx,
                                 std::span<util::u8> rx) noexcept {
        if (!h.ops || !h.ops->write_read) return err(Status::unsupported);
        return h.ops->write_read(h.ctx, address, tx, rx);
    }
}
