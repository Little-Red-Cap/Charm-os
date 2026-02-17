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

    template <typename T>
    concept GpioDriver = requires(GpioPin pin, GpioConfig cfg, GpioLevel lvl) {
        { T::init(pin, cfg) } -> std::same_as<Result>;
        { T::write(pin, lvl) } -> std::same_as<Result>;
        { T::read(pin) } -> std::same_as<GpioLevel>;
    };
}
