#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import charm.core;
import kernel.scheduler;
import kernel.scheduler_export;
import kernel.ssu;
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
        static constexpr bool enable_trace = true;
        static constexpr std::size_t trace_capacity = 4;
        // Demo override: use stricter SSU demand thresholds to surface warning in sample output.
        static constexpr std::size_t ssu_demand_warn_permille = 300;
        static constexpr std::size_t ssu_demand_err_permille = 100;
    };

    struct TaskA {
        static constexpr kernel::Priority priority{1};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return {
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::event,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "demo.task_a",
            };
        }

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[A] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[A] tick=%u\n", kernel::payload_u32(evt));
            }
        }
    };

    struct TaskB {
        static constexpr kernel::Priority priority{0};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return {
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::event,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "demo.task_b",
            };
        }

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[B] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[B] tick=%u\n", kernel::payload_u32(evt));
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
    using Registry = kernel::TaskRegistry<demo::TaskA, demo::TaskB>;
    Registry registry{};
    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto a_id = Registry::id_of<demo::TaskA>();
    const auto b_id = Registry::id_of<demo::TaskB>();

    const auto base = Caps::TimeSource::now();
    (void)running.schedule_at(base + 1000, a_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(1)));
    (void)running.schedule_at(base + 1000, b_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(2)));
    (void)running.post_demand(a_id, kernel::make_event(kernel::EventId::message, static_cast<std::uint32_t>(3)));

    Caps::TimeSource::advance(1000);
    const auto t1 = Caps::TimeSource::now();
    while (running.tick(t1)) {
    }
    while (running.run_once()) {
    }

    char stats_buf[256]{};
    char trace_json[256]{};
    char event_src_json[160]{};
    char ssu_overview_json[256]{};
    char ssu_hotspots_json[320]{};
    (void)kernel::format_snapshot(running, stats_buf, sizeof(stats_buf));
    (void)kernel::format_trace_json(running, trace_json, sizeof(trace_json));
    (void)kernel::format_event_source_json(running, event_src_json, sizeof(event_src_json));
    (void)kernel::format_ssu_overview_json(running, ssu_overview_json, sizeof(ssu_overview_json));
    (void)kernel::format_ssu_hotspots_json(running, ssu_hotspots_json, sizeof(ssu_hotspots_json));
    std::printf("[Stats] %s\n", stats_buf);
    std::printf("[EventSource.json] %s\n", event_src_json);
    std::printf("[SsuOverview.json] %s\n", ssu_overview_json);
    std::printf("[SsuHotspots.json] %s\n", ssu_hotspots_json);
    std::printf("[Trace.json] %s\n", trace_json);

    return 0;
}
