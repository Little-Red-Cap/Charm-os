module;

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <span>

export module hal_i2c;

import hal_core;
import util.core;

export namespace hal {
    enum class I2cAddrMode : util::u8 { addr7, addr10 };

    struct I2cConfig {
        util::u32 hz{100000};
        I2cAddrMode addr_mode{I2cAddrMode::addr7};
    };

    struct I2cHandle {
        util::usize id{0};
        void* impl{nullptr};
    };

    struct I2cOps {
        Result (*init)(void* ctx, const I2cConfig& cfg) noexcept { nullptr };
        Result (*enable)(void* ctx) noexcept { nullptr };
        Result (*disable)(void* ctx) noexcept { nullptr };
        Result (*write)(void* ctx, util::u16 addr,
                        std::span<const util::u8> data) noexcept { nullptr };
        Result (*read)(void* ctx, util::u16 addr,
                       std::span<util::u8> data) noexcept { nullptr };
        Result (*write_read)(void* ctx, util::u16 addr,
                             std::span<const util::u8> tx,
                             std::span<util::u8> rx) noexcept { nullptr };
    };

    struct I2cIoHandle {
        void* ctx{nullptr};
        const I2cOps* ops{nullptr};
    };

    inline Result i2c_init(I2cIoHandle h, const I2cConfig& cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, cfg);
    }

    inline Result i2c_enable(I2cIoHandle h) noexcept {
        if (!h.ops || !h.ops->enable) return err(Status::unsupported);
        return h.ops->enable(h.ctx);
    }

    inline Result i2c_disable(I2cIoHandle h) noexcept {
        if (!h.ops || !h.ops->disable) return err(Status::unsupported);
        return h.ops->disable(h.ctx);
    }

    inline Result i2c_write(I2cIoHandle h, util::u16 addr,
                            std::span<const util::u8> data) noexcept {
        if (!h.ops || !h.ops->write) return err(Status::unsupported);
        return h.ops->write(h.ctx, addr, data);
    }

    inline Result i2c_read(I2cIoHandle h, util::u16 addr,
                           std::span<util::u8> data) noexcept {
        if (!h.ops || !h.ops->read) return err(Status::unsupported);
        return h.ops->read(h.ctx, addr, data);
    }

    inline Result i2c_write_read(I2cIoHandle h, util::u16 addr,
                                 std::span<const util::u8> tx,
                                 std::span<util::u8> rx) noexcept {
        if (!h.ops || !h.ops->write_read) return err(Status::unsupported);
        return h.ops->write_read(h.ctx, addr, tx, rx);
    }

    template <typename T>
    concept I2cDriver = requires(I2cHandle h, I2cConfig cfg,
                                 util::u16 addr,
                                 std::span<const util::u8> tx,
                                 std::span<util::u8> rx) {
        { T::init(h, cfg) } -> std::same_as<Result>;
        { T::enable(h) } -> std::same_as<Result>;
        { T::disable(h) } -> std::same_as<Result>;
        { T::write(h, addr, tx) } -> std::same_as<Result>;
        { T::read(h, addr, rx) } -> std::same_as<Result>;
        { T::write_read(h, addr, tx, rx) } -> std::same_as<Result>;
    };
}
