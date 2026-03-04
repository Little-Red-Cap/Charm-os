module;

#include <span>

export module hal_stm32_stub;

import hal_core;
import hal_gpio;
import hal_uart;
import hal_timer;
import util.core;

// Stub implementations to keep the build/link pipeline working.
export namespace hal::stm32_stub {
    struct Delay {
        static void delay_ms(tick_t) noexcept {}
    };

    struct Gpio {
        static Result init(GpioPin, GpioConfig) noexcept { return ok(); }
        static Result write(GpioPin, GpioLevel) noexcept { return ok(); }
        static GpioLevel read(GpioPin) noexcept { return GpioLevel::low; }
    };

    struct Uart {
        static Result init(UartHandle, UartConfig) noexcept { return ok(); }
        static Result enable(UartHandle) noexcept { return ok(); }
        static Result disable(UartHandle) noexcept { return ok(); }
        static Result try_write(UartHandle, util::u8) noexcept { return err(Status::busy); }
        static Result try_read(UartHandle, util::u8&) noexcept { return err(Status::busy); }
        static void enable_irq(UartHandle, util::u32) noexcept {}
        static void disable_irq(UartHandle, util::u32) noexcept {}
        static void clear_irq(UartHandle, util::u32) noexcept {}
    };

    struct Timer {
        static Result init(TimerHandle, TimerConfig) noexcept { return ok(); }
        static Result start(TimerHandle) noexcept { return ok(); }
        static Result stop(TimerHandle) noexcept { return ok(); }
        static Result set_callback(TimerHandle, TimerCallback) noexcept { return ok(); }
    };
}
