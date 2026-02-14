#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <thread>
#include <chrono>

import util.core;
import hal_core;
import hal_time;
import hal_gpio;
import hal_uart;
import hal_timer;

struct WinTime {
    using Tick = hal::tick_t;
    static Tick now() noexcept {
        using namespace std::chrono;
        auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        return static_cast<Tick>(ms);
    }
};

struct WinDelay {
    static void delay_ms(hal::tick_t ms) noexcept {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

struct WinGpio {
    static hal::Result init(hal::GpioPin, hal::GpioConfig) noexcept { return hal::ok(); }
    static hal::Result write(hal::GpioPin, hal::GpioLevel) noexcept { return hal::ok(); }
    static hal::GpioLevel read(hal::GpioPin) noexcept { return hal::GpioLevel::low; }
};

struct WinUart {
    static hal::Result init(hal::UartHandle, hal::UartConfig) noexcept { return hal::ok(); }
    static hal::Result write(hal::UartHandle, std::span<const util::u8> tx) noexcept {
        std::fwrite(tx.data(), 1, tx.size(), stdout);
        return hal::ok();
    }
    static hal::Result read(hal::UartHandle, std::span<util::u8>) noexcept {
        return hal::err(hal::Status::unsupported);
    }
};

struct WinTimer {
    static hal::Result init(hal::TimerHandle, hal::TimerConfig) noexcept { return hal::ok(); }
    static hal::Result start(hal::TimerHandle) noexcept { return hal::ok(); }
    static hal::Result stop(hal::TimerHandle) noexcept { return hal::ok(); }
    static hal::Result set_callback(hal::TimerHandle, hal::TimerCallback) noexcept { return hal::ok(); }
};

static_assert(hal::TimeSource<WinTime>);
static_assert(hal::DelayProvider<WinDelay>);
static_assert(hal::GpioDriver<WinGpio>);
static_assert(hal::UartDriver<WinUart>);
static_assert(hal::TimerDriver<WinTimer>);

int main() {
    hal::GpioPin led{0, 13};
    hal::GpioConfig cfg{hal::GpioDirection::output, hal::GpioPull::none, hal::GpioLevel::low};
    (void)WinGpio::init(led, cfg);
    (void)WinGpio::write(led, hal::GpioLevel::high);

    hal::UartHandle uart{0, nullptr};
    hal::UartConfig uc{};
    (void)WinUart::init(uart, uc);
    const char* msg = "[hal_demo] ok\n";
    auto bytes = std::span<const util::u8>(reinterpret_cast<const util::u8*>(msg), std::strlen(msg));
    (void)WinUart::write(uart, bytes);

    WinDelay::delay_ms(1);
    return 0;
}