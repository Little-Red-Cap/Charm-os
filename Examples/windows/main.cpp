#include <cstddef>
#include <cstdint>
#include <cstdio>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;

namespace demo {
    struct Config {
        static constexpr bool enable_timer = true;
        static constexpr bool enable_dynamic_priority = false;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 16;
        static constexpr std::size_t timer_capacity = 8;
    };

    struct Heartbeat {
        static constexpr kernel::Priority priority{1};
        std::uint32_t ticks{0};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                ticks = 0;
                std::printf("[Heartbeat] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                ++ticks;
                std::printf("[Heartbeat] tick=%u\n", ticks);
            }
        }
    };

    struct Logger {
        static constexpr kernel::Priority priority{0};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Logger] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[Logger] event value=%u\n", evt.value);
            }
        }
    };
}

int main() {
    using Registry = kernel::TaskRegistry<demo::Heartbeat, demo::Logger>;
    Registry registry{};

    struct Caps {
        using TimeSource = platform::win::SteadyClock;
        using IrqGuard = platform::win::NoopIrqGuard;
        using Wakeup = platform::win::NoopWakeup;
        using SwiTrigger = kernel::NoopSwiTrigger;
    };

    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto heartbeat_id = Registry::id_of<demo::Heartbeat>();
    const auto logger_id = Registry::id_of<demo::Logger>();

    for (std::uint32_t i = 1; i <= 5; ++i) {
        const auto now = Caps::TimeSource::now();
        running.schedule_at(now, heartbeat_id, kernel::Event{kernel::EventId::tick, i});
        running.schedule_at(now, logger_id, kernel::Event{kernel::EventId::tick, i});

        while (running.run_once()) {
        }

        while (running.tick(now)) {
        }
    }

    return 0;
}
