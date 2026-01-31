#include <cstddef>
#include <cstdint>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr bool enable_timer = false;
        static constexpr bool enable_dynamic_priority = false;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 16;
    };

    struct TaskA {
        static constexpr kernel::Priority priority{1};
        void on_event(kernel::Event) { }
    };

    struct TaskB {
        static constexpr kernel::Priority priority{0};
        void on_event(kernel::Event) { }
    };
}

struct Caps {
    struct TimeSource {
        using Tick = std::uint32_t;
        static Tick now() noexcept { return 0; }
    };
    struct IrqGuard {
        static int enter() noexcept { return 0; }
        static void leave(int) noexcept { }
    };
    struct Wakeup {
        static void signal() noexcept { }
    };
    struct SwiTrigger {
        static void trigger(std::size_t) noexcept { }
    };
};

int main() {
    using Registry = kernel::TaskRegistry<demo::TaskA, demo::TaskB>;
    Registry registry{};
    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    (void)running;

    for (;;) {
    }
}
