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

    inline constexpr kernel::Priority kIdlePriority{0};
    inline constexpr kernel::Priority kWorkerPriority{1};
    inline constexpr std::uint32_t kDefaultIdlePayload{77u};
    inline constexpr std::uint32_t kCustomIdlePayload{88u};
    inline constexpr std::uint32_t kWorkerBootstrapPayload{1u};
    inline constexpr std::uint32_t kDeferredPayload{2u};
    inline constexpr ManualTimeSource::Tick kWakeDue{4u};

    [[nodiscard]] kernel::Event make_default_idle_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(
                                      kDefaultIdlePayload));
    }

    [[nodiscard]] kernel::Event make_custom_idle_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(
                                      kCustomIdlePayload));
    }

    [[nodiscard]] kernel::Event make_worker_bootstrap_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(
                                      kWorkerBootstrapPayload));
    }

    [[nodiscard]] kernel::Event make_deferred_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(kDeferredPayload));
    }

    [[nodiscard]] kernel::Event make_tick_wake_event() noexcept
    {
        return kernel::make_event(kernel::EventId::tick,
                                  static_cast<std::uint64_t>(kWakeDue));
    }

    struct SharedState {
        bool default_port_invalid_ok{false};
        bool loop_port_valid{false};
        bool worker_bootstrapped{false};
        bool pre_due_zero{false};
        bool pre_due_trace_stable{false};
        bool deferred_posted{false};
        bool due_count_ok{false};
        bool worker_bootstrap_seen{false};
        bool worker_deferred_seen{false};
        bool worker_tick_seen{false};
        bool worker_finished{false};
        bool idle_seen_while_waiting{false};
        bool default_idle_bootstrapped{false};
        bool default_idle_seen_after_finish{false};
        bool custom_idle_bootstrapped{false};
        bool custom_idle_seen_after_finish{false};
        bool trace_worker_bootstrap_ok{false};
        bool trace_isr_defer_ok{false};
        bool trace_tick_ok{false};
        bool trace_default_idle_ok{false};
        bool trace_custom_idle_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t worker_resumes{0};
    };

    struct IdleContext {
        SharedState* shared{nullptr};
    };

    struct WorkerContext {
        SharedState* shared{nullptr};
    };

    [[nodiscard]] bool probe_default_invalid_port() noexcept
    {
        kernel::RuntimeLoopPort<ManualTimeSource::Tick> port{};
        return !port.valid() && port.advance_tick(3u) == 0u &&
               !port.defer_from_isr(kernel::TaskId{9u}, make_deferred_event()) &&
               !port.bootstrap_idle() &&
               !port.bootstrap_idle(make_custom_idle_event()) &&
               !port.bootstrap_worker(kernel::TaskId{9u},
                                      make_worker_bootstrap_event()) &&
               !port.run_once_or_idle(3u);
    }

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
        if (!context.shared->worker_finished &&
            event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kDefaultIdlePayload) {
            context.shared->idle_seen_while_waiting = true;
        }

        if (context.shared->worker_finished &&
            event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kDefaultIdlePayload) {
            context.shared->default_idle_seen_after_finish = true;
        }

        if (context.shared->worker_finished &&
            event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == kCustomIdlePayload) {
            context.shared->custom_idle_seen_after_finish = true;
        }

        std::printf("[idle] run=%u event=%u payload=%u finished=%d now=%llu\n",
                    context.shared->idle_runs,
                    static_cast<unsigned int>(event.id),
                    kernel::payload_u32(event),
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
            kernel::payload_u32(event) == kWorkerBootstrapPayload) {
            ++context.shared->worker_resumes;
            context.shared->worker_bootstrap_seen = true;
            std::printf("[worker] bootstrap resume=%u now=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == kDeferredPayload) {
            ++context.shared->worker_resumes;
            context.shared->worker_deferred_seen = true;
            std::printf("[worker] deferred resume=%u now=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        if (event.id == kernel::EventId::tick &&
            kernel::payload_u64(event) == kWakeDue) {
            ++context.shared->worker_resumes;
            context.shared->worker_tick_seen = true;
            context.shared->worker_finished = true;
            std::printf("[worker] tick resume=%u payload=%llu now=%llu\n",
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
            case kernel::RuntimeTraceKind::worker_bootstrap:
                shared.trace_worker_bootstrap_ok =
                    shared.trace_worker_bootstrap_ok ||
                    (record->task_valid && record->task.value == worker_id.value &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kWorkerBootstrapPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::isr_defer:
                shared.trace_isr_defer_ok =
                    shared.trace_isr_defer_ok ||
                    (record->task_valid && record->task.value == worker_id.value &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == kDeferredPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::tick:
                shared.trace_tick_ok =
                    shared.trace_tick_ok ||
                    (!record->task_valid &&
                     record->event_id == kernel::EventId::tick &&
                     record->value == 1u && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.trace_default_idle_ok =
                    shared.trace_default_idle_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kDefaultIdlePayload && record->ok);
                shared.trace_custom_idle_ok =
                    shared.trace_custom_idle_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == kCustomIdlePayload && record->ok);
                break;
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
        demo::make_default_idle_event(),
        &runtime_trace,
    };
    const auto loop_port = kernel::make_runtime_loop_port(runtime);

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& worker = registry.get<demo::WorkerTask>();
    worker.context.shared = &shared;

    while (running.run_once()) {
    }

    shared.default_port_invalid_ok = demo::probe_default_invalid_port();
    shared.loop_port_valid = loop_port.valid();

    shared.worker_bootstrapped =
        loop_port.bootstrap_worker(worker_id, demo::make_worker_bootstrap_event());
    (void)loop_port.run_once_or_idle(demo::ManualTimeSource::now());

    const bool timer_scheduled =
        running.schedule_at(demo::kWakeDue, worker_id, demo::make_tick_wake_event());

    demo::ManualTimeSource::advance(1u);
    const auto trace_size_before_pre_due = runtime_trace.size();
    shared.pre_due_zero =
        loop_port.advance_tick(demo::ManualTimeSource::now()) == 0u;
    shared.pre_due_trace_stable =
        runtime_trace.size() == trace_size_before_pre_due;

    (void)loop_port.run_once_or_idle(demo::ManualTimeSource::now());

    demo::ManualTimeSource::advance(1u);
    shared.deferred_posted =
        loop_port.defer_from_isr(worker_id, demo::make_deferred_event());
    (void)loop_port.run_once_or_idle(demo::ManualTimeSource::now());

    demo::ManualTimeSource::advance(2u);
    shared.due_count_ok =
        loop_port.advance_tick(demo::ManualTimeSource::now()) == 1u;

    while (running.run_once()) {
    }

    shared.default_idle_bootstrapped = loop_port.bootstrap_idle();
    (void)loop_port.run_once_or_idle(demo::ManualTimeSource::now());

    demo::ManualTimeSource::advance(1u);
    shared.custom_idle_bootstrapped =
        loop_port.bootstrap_idle(demo::make_custom_idle_event());
    (void)loop_port.run_once_or_idle(demo::ManualTimeSource::now());

    demo::inspect_runtime_trace(runtime_trace, shared, idle_id, worker_id);

    const bool trace_ok = shared.trace_worker_bootstrap_ok &&
                          shared.trace_isr_defer_ok && shared.trace_tick_ok &&
                          shared.trace_default_idle_ok &&
                          shared.trace_custom_idle_ok;
    const bool ok = shared.default_port_invalid_ok && shared.loop_port_valid &&
                    shared.worker_bootstrapped && timer_scheduled &&
                    shared.pre_due_zero && shared.pre_due_trace_stable &&
                    shared.deferred_posted && shared.due_count_ok &&
                    shared.worker_bootstrap_seen && shared.worker_deferred_seen &&
                    shared.worker_tick_seen && shared.worker_finished &&
                    shared.idle_seen_while_waiting &&
                    shared.default_idle_bootstrapped &&
                    shared.default_idle_seen_after_finish &&
                    shared.custom_idle_bootstrapped &&
                    shared.custom_idle_seen_after_finish &&
                    shared.idle_runs >= 3u && shared.worker_resumes == 3u &&
                    trace_ok;

    std::printf(
        "[runtime-loop-port-demo] ok=%d invalid=%d port=%d bootstrap=%d timer=%d pre_due=%d trace_stable=%d defer=%d due=%d worker_boot=%d worker_defer=%d worker_tick=%d default_idle=%d custom_idle=%d idle_runs=%u resumes=%u\n",
        ok ? 1 : 0,
        shared.default_port_invalid_ok ? 1 : 0,
        shared.loop_port_valid ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        timer_scheduled ? 1 : 0,
        shared.pre_due_zero ? 1 : 0,
        shared.pre_due_trace_stable ? 1 : 0,
        shared.deferred_posted ? 1 : 0,
        shared.due_count_ok ? 1 : 0,
        shared.worker_bootstrap_seen ? 1 : 0,
        shared.worker_deferred_seen ? 1 : 0,
        shared.worker_tick_seen ? 1 : 0,
        shared.default_idle_seen_after_finish ? 1 : 0,
        shared.custom_idle_seen_after_finish ? 1 : 0,
        shared.idle_runs,
        shared.worker_resumes);
    std::printf(
        "[runtime-loop-port-trace] ok=%d worker=%d isr=%d tick=%d idle_default=%d idle_custom=%d\n",
        trace_ok ? 1 : 0,
        shared.trace_worker_bootstrap_ok ? 1 : 0,
        shared.trace_isr_defer_ok ? 1 : 0,
        shared.trace_tick_ok ? 1 : 0,
        shared.trace_default_idle_ok ? 1 : 0,
        shared.trace_custom_idle_ok ? 1 : 0);
    return ok ? 0 : 1;
}
