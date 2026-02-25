#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import charm.foundation;
import charm.runtime;
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

    struct BlinkContext {
        std::uint32_t count{0};
    };

    inline void blink_step(BlinkContext& ctx, kernel::ThreadControl& ctrl, kernel::Event evt) {
        if (evt.id == kernel::EventId::init) {
            std::printf("[Thread] init\n");
            return;
        }
        if (evt.id == kernel::EventId::tick) {
            ++ctx.count;
            std::printf("[Thread] step=%u\n", ctx.count);
            if (ctx.count >= 2) {
                ctrl.finish();
            }
        }
    }

    using ThreadTask = kernel::ThreadTask<BlinkContext, blink_step, kernel::Priority{1}>;

    struct BlockingContext {
        bool waiting{false};
    };

    inline void blocking_handler(kernel::ThreadState<BlockingContext>& state, kernel::Event evt) {
        if (evt.id == kernel::EventId::init) {
            std::printf("[Blocking] init\n");
            return;
        }
        if (evt.id == kernel::EventId::tick) {
            std::printf("[Blocking] tick\n");
            if (state.control) {
                state.control->block();
            }
        }
        if (evt.id == kernel::EventId::sync) {
            std::printf("[Blocking] sync\n");
            if (state.control) {
                state.control->resume();
            }
        }
    }

    using BlockingTask = kernel::ThreadBlockingTask<BlockingContext, blocking_handler, kernel::Priority{0}>;
}

struct Caps {
    using TimeSource = platform::win::ManualTimeSource;
    using IrqGuard = platform::win::SpinIrqGuard;
    using Wakeup = platform::win::NoopWakeup;
    using SwiTrigger = kernel::NoopSwiTrigger;
};

int main() {
    using Registry = kernel::TaskRegistry<demo::ThreadTask, demo::BlockingTask>;
    Registry registry{};
    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto thread_id = Registry::id_of<demo::ThreadTask>();
    const auto blocking_id = Registry::id_of<demo::BlockingTask>();

    const auto base = Caps::TimeSource::now();
    (void)running.schedule_at(base + 1000, thread_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(1)));
    (void)running.schedule_at(base + 1000, blocking_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(1)));
    (void)running.schedule_at(base + 2000, blocking_id, kernel::make_event(kernel::EventId::sync, static_cast<std::uint32_t>(0)));

    for (int i = 0; i < 2; ++i) {
        Caps::TimeSource::advance(1000);
        const auto t = Caps::TimeSource::now();
        while (running.tick(t)) {
        }
        while (running.run_once()) {
        }
    }

    return 0;
}
