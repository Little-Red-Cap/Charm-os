#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import kernel.sync;
import kernel.timer;
import platform.win.irq_guard;
import platform.win.manual_time_source;
import platform.win.wakeup;

namespace demo {
    struct Config {
        static constexpr bool enable_timer = true;
        static constexpr bool enable_dynamic_priority = false;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 16;
        static constexpr std::size_t timer_capacity = 8;
        using timer_policy = kernel::HeapTimerPolicy;
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
        using TimeSource = platform::win::ManualTimeSource;
        using IrqGuard = platform::win::NoopIrqGuard;
        using Wakeup = platform::win::NoopWakeup;
        using SwiTrigger = kernel::NoopSwiTrigger;
    };

    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto heartbeat_id = Registry::id_of<demo::Heartbeat>();
    const auto logger_id = Registry::id_of<demo::Logger>();

    kernel::Semaphore<Caps, 2> sem{};
    kernel::Mutex<Caps> lock{};

    for (std::uint32_t i = 1; i <= 3; ++i) {
        const auto base = Caps::TimeSource::now();
        (void)running.schedule_at(base + 2000, heartbeat_id, kernel::Event{kernel::EventId::tick, i});
        (void)running.schedule_at(base + 1000, logger_id, kernel::Event{kernel::EventId::tick, i});

        Caps::TimeSource::advance(1000);
        const auto t1 = Caps::TimeSource::now();
        while (running.tick(t1)) {
        }
        while (running.run_once()) {
        }

        Caps::TimeSource::advance(1000);
        const auto t2 = Caps::TimeSource::now();
        while (running.tick(t2)) {
        }
        while (running.run_once()) {
        }

        const bool released = sem.release();
        const bool acquired = sem.try_acquire();
        if (released && acquired && lock.try_lock()) {
            std::printf("[Sync] released=%u acquired=1 locked=1\n", i);
            lock.unlock();
        } else {
            std::printf("[Sync] released=%u acquired=%u locked=0\n", i, acquired ? 1u : 0u);
        }
    }

    return 0;
}
