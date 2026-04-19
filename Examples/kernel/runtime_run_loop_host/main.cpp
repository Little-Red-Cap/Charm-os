#include <cstddef>
#include <cstdint>
#include <cstdio>
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

    inline constexpr kernel::Priority kIdlePriority{0};
    inline constexpr kernel::Priority kWorkerPriority{1};
    inline constexpr std::uint32_t kFallbackIdlePayload{11u};
    inline constexpr std::uint32_t kDirectIdlePayload{22u};
    inline constexpr std::uint32_t kWorkerBootstrapPayload{33u};
    inline constexpr kernel::TaskId kInvalidIdleTask{99u};

    [[nodiscard]] kernel::Event make_fallback_idle_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(
                                      kFallbackIdlePayload));
    }

    [[nodiscard]] kernel::Event make_direct_idle_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(
                                      kDirectIdlePayload));
    }

    [[nodiscard]] kernel::Event make_worker_bootstrap_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(
                                      kWorkerBootstrapPayload));
    }

    struct SharedState {
        bool invalid_idle_run_loop_ok{false};
        bool direct_idle_bootstrapped{false};
        bool direct_idle_run_ok{false};
        bool direct_idle_seen_before_worker{false};
        bool worker_bootstrapped{false};
        bool worker_run_ok{false};
        bool worker_seen{false};
        bool worker_finished{false};
        bool fallback_idle_run_ok{false};
        bool fallback_idle_seen_after_worker{false};
        bool trace_negative_idle_bootstrap_ok{false};
        bool trace_direct_idle_bootstrap_ok{false};
        bool trace_worker_bootstrap_ok{false};
        bool trace_fallback_idle_bootstrap_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t worker_runs{0};
    };

    struct IdleContext {
        SharedState* shared{nullptr};
    };

    struct WorkerContext {
        SharedState* shared{nullptr};
    };

    void idle_step(IdleContext& context,
                   kernel::ThreadControl&,
                   kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[idle] init\n");
            return;
        }

        if (context.shared == nullptr) {
            return;
        }

        ++context.shared->idle_runs;

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == kDirectIdlePayload &&
            !context.shared->worker_seen) {
            context.shared->direct_idle_seen_before_worker = true;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kFallbackIdlePayload &&
            context.shared->worker_finished) {
            context.shared->fallback_idle_seen_after_worker = true;
        }

        std::printf("[idle] run=%u event=%u payload=%u worker_seen=%d worker_finished=%d now=%llu\n",
                    context.shared->idle_runs,
                    static_cast<unsigned int>(event.id),
                    kernel::payload_u32(event),
                    context.shared->worker_seen ? 1 : 0,
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

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == kWorkerBootstrapPayload) {
            ++context.shared->worker_runs;
            context.shared->worker_seen = true;
            context.shared->worker_finished = true;
            std::printf("[worker] bootstrap run=%u payload=%u now=%llu\n",
                        context.shared->worker_runs,
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
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.trace_negative_idle_bootstrap_ok =
                    shared.trace_negative_idle_bootstrap_ok ||
                    (record->task_valid &&
                     record->task.value == kInvalidIdleTask.value &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kFallbackIdlePayload && !record->ok);
                shared.trace_direct_idle_bootstrap_ok =
                    shared.trace_direct_idle_bootstrap_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == kDirectIdlePayload && record->ok);
                shared.trace_fallback_idle_bootstrap_ok =
                    shared.trace_fallback_idle_bootstrap_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kFallbackIdlePayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::worker_bootstrap:
                shared.trace_worker_bootstrap_ok =
                    shared.trace_worker_bootstrap_ok ||
                    (record->task_valid &&
                     record->task.value == worker_id.value &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == kWorkerBootstrapPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::tick:
            case kernel::RuntimeTraceKind::isr_defer:
            case kernel::RuntimeTraceKind::yield:
            case kernel::RuntimeTraceKind::sleep:
                break;
            }
        }
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
        demo::make_fallback_idle_event(),
        &runtime_trace,
    };
    kernel::RuntimeBridge invalid_idle_runtime{
        running,
        demo::kInvalidIdleTask,
        demo::make_fallback_idle_event(),
        &runtime_trace,
    };

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& worker = registry.get<demo::WorkerTask>();
    worker.context.shared = &shared;

    while (running.run_once()) {
    }

    shared.invalid_idle_run_loop_ok =
        !invalid_idle_runtime.run_once_or_idle(demo::ManualTimeSource::now());
    demo::ManualTimeSource::advance(1u);

    shared.direct_idle_bootstrapped =
        runtime.bootstrap_idle(demo::make_direct_idle_event());
    shared.direct_idle_run_ok =
        runtime.run_once_or_idle(demo::ManualTimeSource::now());
    demo::ManualTimeSource::advance(1u);

    shared.worker_bootstrapped =
        runtime.bootstrap_worker(worker_id, demo::make_worker_bootstrap_event());
    shared.worker_run_ok =
        runtime.run_once_or_idle(demo::ManualTimeSource::now());
    demo::ManualTimeSource::advance(1u);

    shared.fallback_idle_run_ok =
        runtime.run_once_or_idle(demo::ManualTimeSource::now());

    demo::inspect_runtime_trace(runtime_trace, shared, idle_id, worker_id);

    const bool trace_ok = shared.trace_negative_idle_bootstrap_ok &&
                          shared.trace_direct_idle_bootstrap_ok &&
                          shared.trace_worker_bootstrap_ok &&
                          shared.trace_fallback_idle_bootstrap_ok;

    const bool ok = shared.invalid_idle_run_loop_ok &&
                    shared.direct_idle_bootstrapped &&
                    shared.direct_idle_run_ok &&
                    shared.direct_idle_seen_before_worker &&
                    shared.worker_bootstrapped && shared.worker_run_ok &&
                    shared.worker_seen && shared.worker_finished &&
                    shared.fallback_idle_run_ok &&
                    shared.fallback_idle_seen_after_worker &&
                    shared.idle_runs == 2u && shared.worker_runs == 1u &&
                    trace_ok;

    std::printf(
        "[runtime-run-loop-demo] ok=%d invalid_idle=%d direct_idle_post=%d direct_idle_run=%d direct_idle_seen=%d worker_post=%d worker_run=%d worker_seen=%d fallback_idle_run=%d fallback_idle_seen=%d idle_runs=%u worker_runs=%u\n",
        ok ? 1 : 0,
        shared.invalid_idle_run_loop_ok ? 1 : 0,
        shared.direct_idle_bootstrapped ? 1 : 0,
        shared.direct_idle_run_ok ? 1 : 0,
        shared.direct_idle_seen_before_worker ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        shared.worker_run_ok ? 1 : 0,
        shared.worker_seen ? 1 : 0,
        shared.fallback_idle_run_ok ? 1 : 0,
        shared.fallback_idle_seen_after_worker ? 1 : 0,
        shared.idle_runs,
        shared.worker_runs);
    std::printf(
        "[runtime-run-loop-trace] ok=%d neg_idle=%d direct_idle=%d worker=%d fallback_idle=%d\n",
        trace_ok ? 1 : 0,
        shared.trace_negative_idle_bootstrap_ok ? 1 : 0,
        shared.trace_direct_idle_bootstrap_ok ? 1 : 0,
        shared.trace_worker_bootstrap_ok ? 1 : 0,
        shared.trace_fallback_idle_bootstrap_ok ? 1 : 0);
    return ok ? 0 : 1;
}
