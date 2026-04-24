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
import kernel.thread;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr bool enable_timer = false;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 32;
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
        bool worker_waiting_isr{false};
        bool invalid_defer_ok{false};
        bool worker_deferred{false};
        bool worker_resumed_from_isr{false};
        bool worker_finished{false};
        bool idle_seen_before_defer{false};
        bool idle_seen_after_finish{false};
        bool trace_negative_defer_ok{false};
        bool trace_positive_defer_ok{false};
        bool trace_idle_bootstrap_ok{false};
        bool demand_json_ok{false};
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
    inline constexpr std::uint32_t kDeferredPayload{2u};
    inline constexpr std::uint32_t kInvalidDeferredPayload{99u};

    [[nodiscard]] kernel::Event make_bootstrap_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(kBootstrapPayload));
    }

    [[nodiscard]] kernel::Event make_deferred_event(std::uint32_t payload) noexcept
    {
        return kernel::make_event(kernel::EventId::user1, payload);
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
        if (context.shared->worker_waiting_isr &&
            !context.shared->worker_deferred) {
            context.shared->idle_seen_before_defer = true;
        }
        if (context.shared->worker_finished) {
            context.shared->idle_seen_after_finish = true;
        }

        std::printf("[idle] run=%u waiting=%d deferred=%d finished=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->worker_waiting_isr ? 1 : 0,
                    context.shared->worker_deferred ? 1 : 0,
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
            context.shared->worker_waiting_isr = true;
            std::printf("[worker] bootstrap resume=%u waiting=1 now=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == kDeferredPayload) {
            ++context.shared->worker_resumes;
            context.shared->worker_waiting_isr = false;
            context.shared->worker_resumed_from_isr = true;
            context.shared->worker_finished = true;
            std::printf("[worker] deferred resume=%u payload=%u now=%llu\n",
                        context.shared->worker_resumes,
                        kernel::payload_u32(event),
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
                               kernel::TaskId idle_id,
                               kernel::TaskId worker_id) noexcept
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
            case kernel::RuntimeTraceKind::isr_defer:
                shared.trace_negative_defer_ok =
                    shared.trace_negative_defer_ok ||
                    (record->task_valid && record->task.value == 99u &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == kInvalidDeferredPayload && !record->ok);
                shared.trace_positive_defer_ok =
                    shared.trace_positive_defer_ok ||
                    (record->task_valid && record->task.value == worker_id.value &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == kDeferredPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.trace_idle_bootstrap_ok =
                    shared.trace_idle_bootstrap_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->ok);
                break;
            case kernel::RuntimeTraceKind::tick:
            case kernel::RuntimeTraceKind::worker_bootstrap:
            case kernel::RuntimeTraceKind::yield:
            case kernel::RuntimeTraceKind::sleep:
                break;
            }
        }
    }

    [[nodiscard]] bool inspect_event_sources(std::string_view sources) noexcept
    {
        return sources.find("\"demand\":2") != std::string_view::npos;
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

    std::size_t loops = 0;
    while ((!shared.worker_finished || !shared.idle_seen_after_finish) &&
           loops < 32u) {
        if (shared.worker_waiting_isr && !shared.idle_seen_before_defer) {
            (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());
            demo::ManualTimeSource::advance(1u);
            ++loops;
            continue;
        }

        if (shared.worker_waiting_isr && !shared.invalid_defer_ok) {
            shared.invalid_defer_ok = !runtime.defer_from_isr(
                kernel::TaskId{99u},
                demo::make_deferred_event(demo::kInvalidDeferredPayload));
        }

        if (shared.worker_waiting_isr && !shared.worker_deferred) {
            shared.worker_deferred = runtime.defer_from_isr(
                worker_id, demo::make_deferred_event(demo::kDeferredPayload));
        }

        (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());
        demo::ManualTimeSource::advance(1u);
        ++loops;
    }

    char event_sources[128]{};
    char snapshot[256]{};
    (void)running.format_event_source_json(event_sources, sizeof(event_sources));
    (void)running.format_snapshot(snapshot, sizeof(snapshot));
    shared.demand_json_ok = demo::inspect_event_sources(event_sources);

    demo::inspect_runtime_trace(runtime_trace, shared, idle_id, worker_id);

    const bool trace_ok = shared.trace_negative_defer_ok &&
                          shared.trace_positive_defer_ok &&
                          shared.trace_idle_bootstrap_ok;

    const bool ok = shared.worker_bootstrapped &&
                    shared.idle_seen_before_defer &&
                    shared.invalid_defer_ok && shared.worker_deferred &&
                    shared.worker_resumed_from_isr && shared.worker_finished &&
                    shared.idle_seen_after_finish && shared.idle_runs >= 2u &&
                    shared.worker_resumes == 2u && shared.demand_json_ok &&
                    trace_ok && loops < 32u;

    std::printf(
        "[runtime-isr-defer-demo] ok=%d bootstrapped=%d idle_wait=%d invalid=%d deferred=%d resumed=%d finished=%d idle_finish=%d idle_runs=%u resumes=%u demand=%d loops=%llu\n",
        ok ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        shared.idle_seen_before_defer ? 1 : 0,
        shared.invalid_defer_ok ? 1 : 0,
        shared.worker_deferred ? 1 : 0,
        shared.worker_resumed_from_isr ? 1 : 0,
        shared.worker_finished ? 1 : 0,
        shared.idle_seen_after_finish ? 1 : 0,
        shared.idle_runs,
        shared.worker_resumes,
        shared.demand_json_ok ? 1 : 0,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-isr-defer-trace] ok=%d neg=%d pos=%d idle_bootstrap=%d sources=%s\n",
        trace_ok ? 1 : 0,
        shared.trace_negative_defer_ok ? 1 : 0,
        shared.trace_positive_defer_ok ? 1 : 0,
        shared.trace_idle_bootstrap_ok ? 1 : 0,
        event_sources);
    std::printf("[runtime-isr-defer.snapshot] %s\n", snapshot);
    return ok ? 0 : 1;
}
