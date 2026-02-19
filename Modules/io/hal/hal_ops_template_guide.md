# HAL Ops Binding Templates (Draft)

This note shows the minimal "ops/vtable" binding shape for HAL backends.
Keep the backend-specific details outside the interface module.

## SPI binding template

```cpp
// platform/<vendor>/<board>/hal_spi_backend.cppm
module;

#include <span>

export module hal_spi_backend;

import hal_core;
import hal_spi;
import util.core;

namespace {
    struct SpiCtx {
        // platform-specific state (register base, dma handle, etc.)
    };

    hal::Result spi_init_impl(void* ctx, const hal::SpiConfig& cfg) noexcept {
        auto* self = static_cast<SpiCtx*>(ctx);
        (void)self;
        (void)cfg;
        return hal::ok();
    }

    hal::Result spi_transfer_impl(void* ctx,
                                  std::span<const util::u8> tx,
                                  std::span<util::u8> rx) noexcept {
        auto* self = static_cast<SpiCtx*>(ctx);
        (void)self;
        (void)tx;
        (void)rx;
        return hal::ok();
    }

    hal::Result spi_set_speed_impl(void* ctx, util::u32 hz) noexcept {
        auto* self = static_cast<SpiCtx*>(ctx);
        (void)self;
        (void)hz;
        return hal::ok();
    }

    constexpr hal::SpiOps kSpiOps{
        &spi_init_impl,
        &spi_transfer_impl,
        &spi_set_speed_impl,
    };
}

export namespace hal {
    inline SpiHandle make_spi_handle(SpiCtx& ctx) noexcept {
        return SpiHandle{&ctx, &kSpiOps};
    }
}
```

## GPIO binding template

```cpp
// platform/<vendor>/<board>/hal_gpio_backend.cppm
module;

export module hal_gpio_backend;

import hal_core;
import hal_gpio;
import util.core;

namespace {
    struct GpioCtx {};

    hal::Result gpio_init_impl(void* ctx, hal::GpioPin pin, hal::GpioConfig cfg) noexcept {
        (void)ctx; (void)pin; (void)cfg;
        return hal::ok();
    }

    hal::Result gpio_write_impl(void* ctx, hal::GpioPin pin, hal::GpioLevel lvl) noexcept {
        (void)ctx; (void)pin; (void)lvl;
        return hal::ok();
    }

    hal::Result gpio_read_impl(void* ctx, hal::GpioPin pin, hal::GpioLevel& out) noexcept {
        (void)ctx; (void)pin;
        out = hal::GpioLevel::low;
        return hal::ok();
    }

    constexpr hal::GpioOps kGpioOps{
        &gpio_init_impl,
        &gpio_write_impl,
        &gpio_read_impl,
    };
}

export namespace hal {
    inline GpioIoHandle make_gpio_handle(GpioCtx& ctx) noexcept {
        return GpioIoHandle{&ctx, &kGpioOps};
    }
}
```

## Notes

- Keep ISR/clock/reset/pinmux in platform code; do not leak into HAL interfaces.
- Handles are cheap: `{ctx, ops}` only.
- If a backend does not support a feature, return `Status::unsupported`.
