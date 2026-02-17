module port.kernel;

namespace port {
    tick_t now_ms() noexcept {
        // TODO: return monotonic millisecond tick
        return 0;
    }

    void irq_disable() noexcept {
        // TODO: disable interrupts
    }

    void irq_enable() noexcept {
        // TODO: enable interrupts
    }

    void wakeup_signal() noexcept {
        // TODO: optional low-power wakeup hook
    }

    void swi_trigger(std::size_t) noexcept {
        // TODO: optional SW interrupt hook
    }
}
