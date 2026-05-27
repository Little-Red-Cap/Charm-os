#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.scheduler;
import kernel.scheduler_export;
import kernel.thread;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr bool enable_timer = true;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 32;
        static constexpr std::size_t timer_capacity = 8;
        static constexpr bool enable_trace = true;
        static constexpr std::size_t trace_capacity = 32;
    };

    struct ManualTimeSource {
        using Tick = std::uint64_t;

        static Tick now() noexcept
        {
            return ticks_;
        }

        static void reset() noexcept
        {
            ticks_ = 0;
        }

        static void advance(Tick delta) noexcept
        {
            ticks_ += delta;
        }

    private:
        inline static Tick ticks_{0};
    };

    struct Caps {
        using TimeSource = ManualTimeSource;
        using IrqGuard = kernel::NoopIrqGuard;
        using Wakeup = kernel::NoopWakeup;
        using SwiTrigger = kernel::NoopSwiTrigger;
    };

    struct SharedState {
        bool worker_bootstrapped{false};
        bool timers_scheduled{false};
        bool pre_due_count_zero{false};
        bool pre_due_trace_stable{false};
        bool due_count_ok{false};
        bool worker_waiting_tick{false};
        bool worker_user_wake_seen{false};
        bool worker_tick_wake_seen{false};
        bool worker_finished{false};
        bool idle_seen_while_waiting{false};
        bool idle_seen_after_finish{false};
        bool trace_tick_ok{false};
        bool trace_idle_bootstrap_ok{false};
        bool timer_json_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t worker_resumes{0};
    };

    struct IdleContext {
        SharedState* shared{nullptr};
    };

    struct WorkerContext {
        SharedState* shared{nullptr};
    };

    inline constexpr kernel::Priority kIdlePriority{0};
    inline constexpr kernel::Priority kWorkerPriority{1};
    inline constexpr std::uint32_t kBootstrapPayload{1u};
    inline constexpr std::uint32_t kUserWakePayload{7u};
    inline constexpr ManualTimeSource::Tick kWakeDue{4u};

    [[nodiscard]] kernel::Event make_bootstrap_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(kBootstrapPayload));
    }

    [[nodiscard]] kernel::Event make_timer_user_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(kUserWakePayload));
    }

    [[nodiscard]] kernel::Event make_timer_tick_event() noexcept
    {
        return kernel::make_event(kernel::EventId::tick,
                                  static_cast<std::uint64_t>(kWakeDue));
    }

    void idle_step(IdleContext& context,
                   kernel::ThreadControl&,
                   kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[idle] init\n");
            return;
        }

        if (context.shared == nullptr || event.id != kernel::EventId::user0) {
            return;
        }

        ++context.shared->idle_runs;
        if (context.shared->worker_waiting_tick &&
            !context.shared->worker_finished) {
            context.shared->idle_seen_while_waiting = true;
        }
        if (context.shared->worker_finished) {
            context.shared->idle_seen_after_finish = true;
        }

        std::printf("[idle] run=%u waiting=%d finished=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->worker_waiting_tick ? 1 : 0,
                    context.shared->worker_finished ? 1 : 0,
                    static_cast<unsigned long long>(ManualTimeSource::now()));
    }

    void worker_step(WorkerContext& context,
                     kernel::ThreadControl& control,
                     kernel::Event event)
    {
        if (context.shared == nullptr) {
            return;
        }

        if (event.id == kernel::EventId::init) {
            std::printf("[worker] init\n");
            return;
        }

        if (event.id == kernel::EventId::terminate) {
            control.finish();
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kBootstrapPayload) {
            ++context.shared->worker_resumes;
            context.shared->worker_waiting_tick = true;
            std::printf("[worker] bootstrap resume=%u waiting=1 now=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == kUserWakePayload) {
            ++context.shared->worker_resumes;
            context.shared->worker_user_wake_seen = true;
            std::printf("[worker] timer-user resume=%u payload=%u now=%llu\n",
                        context.shared->worker_resumes,
                        kernel::payload_u32(event),
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        if (event.id == kernel::EventId::tick &&
            kernel::payload_u64(event) == kWakeDue) {
            ++context.shared->worker_resumes;
            context.shared->worker_waiting_tick = false;
            context.shared->worker_tick_wake_seen = true;
            context.shared->worker_finished = true;
            std::printf("[worker] timer-tick resume=%u payload=%llu now=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(kernel::payload_u64(event)),
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            control.finish();
        }
    }

    using IdleTask = kernel::ThreadTask<IdleContext, &idle_step, kIdlePriority>;
    using WorkerTask =
        kernel::ThreadTask<WorkerContext, &worker_step, kWorkerPriority>;

    template <typename TraceBuffer>
    void inspect_runtime_trace(TraceBuffer& trace,
                               SharedState& shared,
                               kernel::TaskId idle_id) noexcept
    {
        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            if (record->task_valid) {
                std::printf(
                    "[runtime-trace] t=%llu kind=%s task=%llu event=%u value=%llu ok=%d\n",
                    static_cast<unsigned long long>(record->stamp),
                    kernel::runtime_trace_kind_name(record->kind),
                    static_cast<unsigned long long>(record->task.value),
                    static_cast<unsigned int>(record->event_id),
                    static_cast<unsigned long long>(record->value),
                    record->ok ? 1 : 0);
            } else {
                std::printf(
                    "[runtime-trace] t=%llu kind=%s task=- event=%u value=%llu ok=%d\n",
                    static_cast<unsigned long long>(record->stamp),
                    kernel::runtime_trace_kind_name(record->kind),
                    static_cast<unsigned int>(record->event_id),
                    static_cast<unsigned long long>(record->value),
                    record->ok ? 1 : 0);
            }

            switch (record->kind) {
            case kernel::RuntimeTraceKind::tick:
                shared.trace_tick_ok =
                    shared.trace_tick_ok ||
                    (!record->task_valid && record->event_id == kernel::EventId::tick &&
                     record->value == 2u && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.trace_idle_bootstrap_ok =
                    shared.trace_idle_bootstrap_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->ok);
                break;
            case kernel::RuntimeTraceKind::isr_defer:
            case kernel::RuntimeTraceKind::worker_bootstrap:
            case kernel::RuntimeTraceKind::yield:
            case kernel::RuntimeTraceKind::sleep:
                break;
            }
        }
    }

    [[nodiscard]] bool inspect_event_sources(std::string_view sources) noexcept
    {
        return sources.find("\"timer\":2") != std::string_view::npos;
    }
}

int main()
{
    using Registry = kernel::TaskRegistry<demo::IdleTask, demo::WorkerTask>;
    using RuntimeTrace =
        kernel::RuntimeTraceBuffer<demo::ManualTimeSource::Tick, 32>;

    demo::ManualTimeSource::reset();

    Registry registry{};
    demo::Caps caps{};
    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    RuntimeTrace runtime_trace{};
    demo::SharedState shared{};
    const auto idle_id = Registry::id_of<demo::IdleTask>();
    const auto worker_id = Registry::id_of<demo::WorkerTask>();

    kernel::RuntimeBridge runtime{
        running,
        idle_id,
        kernel::make_event(kernel::EventId::user0),
        &runtime_trace,
    };

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& worker = registry.get<demo::WorkerTask>();
    worker.context.shared = &shared;

    while (running.run_once()) {
    }

    shared.worker_bootstrapped =
        runtime.bootstrap_worker(worker_id, demo::make_bootstrap_event());
    (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());

    shared.timers_scheduled =
        running.schedule_at(demo::kWakeDue, worker_id, demo::make_timer_user_event()) &&
        running.schedule_at(demo::kWakeDue, worker_id, demo::make_timer_tick_event());

    demo::ManualTimeSource::advance(3u);
    const auto trace_size_before = runtime_trace.size();
    shared.pre_due_count_zero =
        runtime.advance_tick(demo::ManualTimeSource::now()) == 0u;
    shared.pre_due_trace_stable = runtime_trace.size() == trace_size_before;

    (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());

    demo::ManualTimeSource::advance(1u);
    shared.due_count_ok =
        runtime.advance_tick(demo::ManualTimeSource::now()) == 2u;

    while (running.run_once()) {
    }

    demo::ManualTimeSource::advance(1u);
    (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());

    char event_sources[128]{};
    char snapshot[256]{};
    (void)kernel::format_event_source_json(running, event_sources, sizeof(event_sources));
    (void)kernel::format_snapshot(running, snapshot, sizeof(snapshot));
    shared.timer_json_ok = demo::inspect_event_sources(event_sources);

    demo::inspect_runtime_trace(runtime_trace, shared, idle_id);

    const bool trace_ok = shared.trace_tick_ok && shared.trace_idle_bootstrap_ok;
    const bool ok = shared.worker_bootstrapped && shared.timers_scheduled &&
                    shared.pre_due_count_zero && shared.pre_due_trace_stable &&
                    shared.due_count_ok && shared.worker_user_wake_seen &&
                    shared.worker_tick_wake_seen && shared.worker_finished &&
                    shared.idle_seen_while_waiting &&
                    shared.idle_seen_after_finish && shared.idle_runs >= 2u &&
                    shared.worker_resumes == 3u && shared.timer_json_ok &&
                    trace_ok;

    std::printf(
        "[runtime-tick-demo] ok=%d bootstrapped=%d timers=%d pre_due=%d trace_stable=%d due_count=%d user_wake=%d tick_wake=%d finished=%d idle_wait=%d idle_finish=%d idle_runs=%u resumes=%u timer_json=%d\n",
        ok ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        shared.timers_scheduled ? 1 : 0,
        shared.pre_due_count_zero ? 1 : 0,
        shared.pre_due_trace_stable ? 1 : 0,
        shared.due_count_ok ? 1 : 0,
        shared.worker_user_wake_seen ? 1 : 0,
        shared.worker_tick_wake_seen ? 1 : 0,
        shared.worker_finished ? 1 : 0,
        shared.idle_seen_while_waiting ? 1 : 0,
        shared.idle_seen_after_finish ? 1 : 0,
        shared.idle_runs,
        shared.worker_resumes,
        shared.timer_json_ok ? 1 : 0);
    std::printf(
        "[runtime-tick-trace] ok=%d tick=%d idle_bootstrap=%d sources=%s\n",
        trace_ok ? 1 : 0,
        shared.trace_tick_ok ? 1 : 0,
        shared.trace_idle_bootstrap_ok ? 1 : 0,
        event_sources);
    std::printf("[runtime-tick.snapshot] %s\n", snapshot);
    return ok ? 0 : 1;
}
