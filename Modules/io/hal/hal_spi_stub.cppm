module;

#include <span>

export module hal_spi_stub;

import hal_core;
import hal_spi;
import util.core;

export namespace hal {
    struct StubSpi {
        static Result init(SpiHandle, const SpiConfig&) noexcept { return ok(); }
        static Result transfer(SpiHandle, std::span<const util::u8>, std::span<util::u8>) noexcept {
            return ok();
        }
        static Result set_speed(SpiHandle, util::u32) noexcept { return ok(); }
    };
}
