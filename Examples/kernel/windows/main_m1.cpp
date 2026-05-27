#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.event_token;
import kernel.evt;
import kernel.ipc;
import kernel.scheduler;
import kernel.scheduler_export;
import kernel.ssu;
import kernel.sync_base;
import kernel.sync_unified;
import kernel.wait_token;
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

    struct Producer {
        static constexpr kernel::Priority priority{1};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return {
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::event,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "demo.producer",
            };
        }

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Producer] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[Producer] tick=%u\n", kernel::payload_u32(evt));
            }
        }
    };

    struct Consumer {
        static constexpr kernel::Priority priority{0};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return {
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::event,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "demo.consumer",
            };
        }

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Consumer] init\n");
                return;
            }
            if (evt.id == kernel::EventId::sync) {
                std::printf("[Consumer] sync=%u\n", kernel::payload_u32(evt));
                return;
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
    using Registry = kernel::TaskRegistry<demo::Producer, demo::Consumer>;
    Registry registry{};
    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto producer_id = Registry::id_of<demo::Producer>();
    const auto consumer_id = Registry::id_of<demo::Consumer>();

    kernel::SyncUnified<decltype(running), 2> sync(running);
    kernel::SemaphoreIpc<Caps, 2, decltype(running)> sem_ipc(running);

    // Scenario 1: wait + notify_one => ok
    (void)sync.wait(consumer_id, kernel::WaitToken{1});
    (void)sync.notify_one(kernel::WaitResult::ok);

    // Scenario 2: wait + cancel => canceled
    (void)sync.wait(consumer_id, kernel::WaitToken{2});
    (void)sync.cancel(kernel::WaitToken{2});

    // Scenario 3: wait_timeout + timeout => timeout
    (void)sync.wait_timeout(consumer_id, kernel::WaitToken{3}, kernel::EventToken{running.schedule_at_token(Caps::TimeSource::now() + 1000, consumer_id, kernel::make_event(kernel::EventId::sync, static_cast<std::uint32_t>(kernel::WaitResult::timeout))).value});

    // Scenario 4: wait_timeout + notify before due => ok
    (void)sync.wait_timeout(consumer_id, kernel::WaitToken{4}, kernel::EventToken{running.schedule_at_token(Caps::TimeSource::now() + 2000, consumer_id, kernel::make_event(kernel::EventId::sync, static_cast<std::uint32_t>(kernel::WaitResult::timeout))).value});
    (void)sync.notify_one(kernel::WaitResult::ok);

    // Scenario 5: notify_all wakes two waiters
    (void)sync.wait(consumer_id, kernel::WaitToken{5});
    (void)sync.wait(consumer_id, kernel::WaitToken{6});
    (void)sync.notify_all(kernel::WaitResult::ok);

    // Producer tick
    (void)running.schedule_at(Caps::TimeSource::now() + 1000, producer_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(1)));

    // IPC demo
    (void)sem_ipc.wait(consumer_id);
    (void)sem_ipc.post();

    Caps::TimeSource::advance(1000);
    const auto t1 = Caps::TimeSource::now();
    for (std::size_t i = 0; i < 128 && running.tick(t1); ++i) {
    }
    for (std::size_t i = 0; i < 512 && running.run_once(); ++i) {
    }

    char snapshot[256]{};
    char event_src_json[192]{};
    char ssu_overview_json[256]{};
    char ssu_hotspots_json[320]{};
    (void)kernel::format_snapshot(running, snapshot, sizeof(snapshot));
    (void)kernel::format_event_source_json(running, event_src_json, sizeof(event_src_json));
    (void)kernel::format_ssu_overview_json(running, ssu_overview_json, sizeof(ssu_overview_json));
    (void)kernel::format_ssu_hotspots_json(running, ssu_hotspots_json, sizeof(ssu_hotspots_json));
    std::printf("[M1.Stats] %s\n", snapshot);
    std::printf("[M1.EventSource.json] %s\n", event_src_json);
    std::printf("[M1.SsuOverview.json] %s\n", ssu_overview_json);
    std::printf("[M1.SsuHotspots.json] %s\n", ssu_hotspots_json);

    return 0;
}
