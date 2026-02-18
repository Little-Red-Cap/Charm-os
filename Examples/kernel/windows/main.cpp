#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import kernel.timer;
import platform.win.irq_guard;
import platform.win.manual_time_source;
import platform.win.wakeup;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr bool enable_timer = true;
        static constexpr bool enable_dynamic_priority = false;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 16;
        static constexpr std::size_t timer_capacity = 8;
    };

    struct Urgent {
        static constexpr kernel::Priority priority{1};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Urgent] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[Urgent] tick=%u\n", kernel::payload_u32(evt));
            }
        }
    };

    struct Heartbeat {
        static constexpr kernel::Priority priority{0};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Heartbeat] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[Heartbeat] tick=%u\n", kernel::payload_u32(evt));
            }
        }
    };
}

struct Caps {
    using TimeSource = platform::win::ManualTimeSource;
    using IrqGuard = platform::win::SpinIrqGuard;
    using Wakeup = platform::win::NoopWakeup;
    using SwiTrigger = kernel::NoopSwiTrigger;
};

int main() {
    using Registry = kernel::TaskRegistry<demo::Urgent, demo::Heartbeat>;
    Registry registry{};
    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto urgent_id = Registry::id_of<demo::Urgent>();
    const auto heartbeat_id = Registry::id_of<demo::Heartbeat>();

    for (std::uint32_t i = 1; i <= 3; ++i) {
        const auto base = Caps::TimeSource::now();
        (void)running.schedule_at(base + 1000, urgent_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 1000, heartbeat_id, kernel::make_event(kernel::EventId::tick, i));

        Caps::TimeSource::advance(1000);
        const auto t1 = Caps::TimeSource::now();
        while (running.tick(t1)) {
        }
        while (running.run_once()) {
        }
    }

    return 0;
}
