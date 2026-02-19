module;

#include <cstdint>
#include <concepts>
#include <concepts>

export module hal_gpio;

import hal_core;
import util.core;

export namespace hal {
    enum class GpioDirection : util::u8 { input, output };
    enum class GpioPull : util::u8 { none, up, down };
    enum class GpioLevel : util::u8 { low = 0, high = 1 };

    struct GpioPin {
        util::u8 port{0};
        util::u8 pin{0};
    };

    struct GpioConfig {
        GpioDirection direction{GpioDirection::input};
        GpioPull pull{GpioPull::none};
        GpioLevel init_level{GpioLevel::low};
    };

    struct GpioOps {
        Result (*init)(void* ctx, GpioPin pin, GpioConfig cfg) noexcept { nullptr };
        Result (*write)(void* ctx, GpioPin pin, GpioLevel lvl) noexcept { nullptr };
        Result (*read)(void* ctx, GpioPin pin, GpioLevel& out) noexcept { nullptr };
    };

    struct GpioIoHandle {
        void* ctx{nullptr};
        const GpioOps* ops{nullptr};
    };

    inline Result gpio_init(GpioIoHandle h, GpioPin pin, GpioConfig cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, pin, cfg);
    }

    inline Result gpio_write(GpioIoHandle h, GpioPin pin, GpioLevel lvl) noexcept {
        if (!h.ops || !h.ops->write) return err(Status::unsupported);
        return h.ops->write(h.ctx, pin, lvl);
    }

    inline Result gpio_read(GpioIoHandle h, GpioPin pin, GpioLevel& out) noexcept {
        if (!h.ops || !h.ops->read) return err(Status::unsupported);
        return h.ops->read(h.ctx, pin, out);
    }

    template <typename T>
    concept GpioDriver = requires(GpioPin pin, GpioConfig cfg, GpioLevel lvl) {
        { T::init(pin, cfg) } -> std::same_as<Result>;
        { T::write(pin, lvl) } -> std::same_as<Result>;
        { T::read(pin) } -> std::same_as<GpioLevel>;
    };
}
