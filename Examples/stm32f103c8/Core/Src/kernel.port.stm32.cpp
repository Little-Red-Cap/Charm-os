module;

#include <cstddef>

#include "main.h"

module port.kernel;

namespace port {
    tick_t now_ms() noexcept {
        return static_cast<tick_t>(HAL_GetTick());
    }

    void irq_disable() noexcept {
        __disable_irq();
    }

    void irq_enable() noexcept {
        __enable_irq();
    }

    void wakeup_signal() noexcept {
        // TODO: optional low-power wakeup hook.
    }

    void swi_trigger(std::size_t) noexcept {
        // TODO: optional SW interrupt hook.
    }
}
