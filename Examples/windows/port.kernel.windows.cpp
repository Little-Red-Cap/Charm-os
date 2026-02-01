module;

#include <chrono>
#include <cstddef>
#include <cstdint>

module port.kernel;

namespace port {
    tick_t now_ms() noexcept {
        using namespace std::chrono;
        const auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        return static_cast<tick_t>(ms);
    }

    void irq_disable() noexcept {
    }

    void irq_enable() noexcept {
    }

    void wakeup_signal() noexcept {
    }

    void swi_trigger(std::size_t) noexcept {
    }
}
