export module power.types;

import util.core;

export namespace power {
    enum class State : util::u8 {
        active,
        idle,
        sleep,
        deep_sleep,
        stop,
        standby
    };

    enum class ClockDomain : util::u8 {
        core,
        ahb,
        apb1,
        apb2,
        peripheral
    };

    enum class WakeSource : util::u8 {
        irq,
        wake_pin,
        rtc,
        dma,
        timer,
        usb,
        other
    };

    struct WakeRequest {
        WakeSource source{WakeSource::other};
        util::u32 id{0};
    };

    struct ClockRequest {
        ClockDomain domain{ClockDomain::core};
        util::u32 id{0};
        bool enable{true};
    };
}
