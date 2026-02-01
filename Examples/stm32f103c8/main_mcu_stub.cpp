#include <cstddef>
#include <cstdint>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import port.kernel;

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

extern "C" void application() {
    using Registry = kernel::TaskRegistry<demo::TaskA, demo::TaskB>;
    static auto running = []() {
        static Registry registry{};
        static port::KernelCaps caps{};
        auto created = kernel::make_scheduler<demo::Config>(registry, caps);
        return kernel::start(std::move(created));
    }();

    (void)running.run_auto();
}
