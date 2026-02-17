module;

#include <cstddef>
#include <cstdint>

export module port.kernel;

export namespace port {
    using tick_t = std::uint32_t;

    tick_t now_ms() noexcept;
    void irq_disable() noexcept;
    void irq_enable() noexcept;
    void wakeup_signal() noexcept;
    void swi_trigger(std::size_t prio) noexcept;

    struct KernelCaps {
        struct TimeSource {
            using Tick = tick_t;
            static Tick now() noexcept { return now_ms(); }
        };
        struct IrqGuard {
            using Token = int;
            static int enter() noexcept {
                irq_disable();
                return 1;
            }
            static void leave(Token) noexcept {
                irq_enable();
            }
        };
        struct Wakeup {
            static void signal() noexcept { wakeup_signal(); }
        };
        struct SwiTrigger {
            static void trigger(std::size_t prio) noexcept { swi_trigger(prio); }
        };
    };
}
