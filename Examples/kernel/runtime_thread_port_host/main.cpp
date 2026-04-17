#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.context;
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

    struct SharedState {
        bool default_port_invalid_ok{false};
        bool no_current_yield_ok{false};
        bool no_current_sleep_ok{false};
        bool port_valid{false};
        bool worker_bootstrapped{false};
        bool worker_yield_ok{false};
        bool worker_sleep_ok{false};
        bool worker_finished{false};
        bool idle_seen_while_waiting{false};
        bool idle_seen_after_finish{false};
        bool trace_negative_yield_ok{false};
        bool trace_negative_sleep_ok{false};
        bool trace_worker_bootstrap_ok{false};
        bool trace_positive_yield_ok{false};
        bool trace_positive_sleep_ok{false};
        bool trace_tick_ok{false};
        bool trace_idle_bootstrap_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t worker_resumes{0};
        std::uint64_t wake_due{0};
    };

    struct IdleContext {
        SharedState* shared{nullptr};
    };

    struct WorkerContext {
        SharedState* shared{nullptr};
        kernel::RuntimeThreadPort<ManualTimeSource::Tick> port{};
    };

    inline constexpr kernel::Priority kIdlePriority{0};
    inline constexpr kernel::Priority kWorkerPriority{1};

    [[nodiscard]] kernel::Event make_worker_yield_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(1u));
    }

    [[nodiscard]] kernel::Event make_worker_sleep_event(
        ManualTimeSource::Tick due) noexcept
    {
        return kernel::make_event(kernel::EventId::tick,
                                  static_cast<std::uint64_t>(due));
    }

    [[nodiscard]] bool probe_default_invalid_port() noexcept
    {
        kernel::RuntimeThreadPort<ManualTimeSource::Tick> port{};
        return !port.valid() && !port.yield_current(make_worker_yield_event()) &&
               !port.sleep_current_until(7u, make_worker_sleep_event(7u));
    }

    template <typename Tick>
    [[nodiscard]] bool probe_bound_port_no_current_yield(
        const kernel::RuntimeThreadPort<Tick>& port) noexcept
    {
        kernel::clear_current();
        return port.valid() && !port.yield_current(make_worker_yield_event());
    }

    template <typename Tick>
    [[nodiscard]] bool probe_bound_port_no_current_sleep(
        const kernel::RuntimeThreadPort<Tick>& port,
        Tick due) noexcept
    {
        kernel::clear_current();
        return port.valid() &&
               !port.sleep_current_until(due, make_worker_sleep_event(due));
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
        if (context.shared->worker_finished) {
            context.shared->idle_seen_after_finish = true;
        } else {
            context.shared->idle_seen_while_waiting = true;
        }

        std::printf("[idle] run=%u worker_finished=%d now=%llu\n",
                    context.shared->idle_runs,
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
            kernel::payload_u32(event) == 1u) {
            ++context.shared->worker_resumes;
            context.shared->worker_yield_ok =
                context.port.yield_current(make_worker_yield_event());
            std::printf("[worker] bootstrap resume=%u yield=%d now=%llu\n",
                        context.shared->worker_resumes,
                        context.shared->worker_yield_ok ? 1 : 0,
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == 1u) {
            ++context.shared->worker_resumes;
            context.shared->wake_due = ManualTimeSource::now() + 3u;
            context.shared->worker_sleep_ok = context.port.sleep_current_until(
                context.shared->wake_due,
                make_worker_sleep_event(context.shared->wake_due));
            std::printf("[worker] yielded resume=%u sleep=%d due=%llu\n",
                        context.shared->worker_resumes,
                        context.shared->worker_sleep_ok ? 1 : 0,
                        static_cast<unsigned long long>(
                            context.shared->wake_due));
            return;
        }

        if (event.id == kernel::EventId::tick) {
            ++context.shared->worker_resumes;
            context.shared->worker_finished =
                kernel::payload_u64(event) == context.shared->wake_due;
            std::printf("[worker] wake resume=%u payload=%llu now=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(
                            kernel::payload_u64(event)),
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
                               kernel::TaskId worker_id)
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
                     record->ok);
                break;
            case kernel::RuntimeTraceKind::yield:
                shared.trace_negative_yield_ok =
                    shared.trace_negative_yield_ok ||
                    (!record->task_valid && !record->ok);
                shared.trace_positive_yield_ok =
                    shared.trace_positive_yield_ok ||
                    (record->task_valid && record->task.value == worker_id.value &&
                     record->ok &&
                     record->event_id == kernel::EventId::user1 &&
                     record->value == 1u);
                break;
            case kernel::RuntimeTraceKind::sleep:
                shared.trace_negative_sleep_ok =
                    shared.trace_negative_sleep_ok ||
                    (!record->task_valid && !record->ok);
                shared.trace_positive_sleep_ok =
                    shared.trace_positive_sleep_ok ||
                    (record->task_valid && record->task.value == worker_id.value &&
                     record->ok && record->value == shared.wake_due);
                break;
            case kernel::RuntimeTraceKind::tick:
                shared.trace_tick_ok =
                    shared.trace_tick_ok || (record->ok && record->value != 0u);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.trace_idle_bootstrap_ok =
                    shared.trace_idle_bootstrap_ok ||
                    (record->task_valid && record->task.value == idle_id.value &&
                     record->ok);
                break;
            case kernel::RuntimeTraceKind::isr_defer:
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
        kernel::make_event(kernel::EventId::user0),
        &runtime_trace,
    };

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& worker = registry.get<demo::WorkerTask>();
    worker.context.shared = &shared;
    worker.context.port = kernel::make_runtime_thread_port(runtime);
    shared.port_valid = worker.context.port.valid();

    while (running.run_once()) {
    }

    shared.default_port_invalid_ok = demo::probe_default_invalid_port();
    shared.no_current_yield_ok =
        demo::probe_bound_port_no_current_yield(worker.context.port);
    shared.no_current_sleep_ok =
        demo::probe_bound_port_no_current_sleep(worker.context.port,
                                               demo::ManualTimeSource::Tick{
                                                   11u});
    kernel::clear_current();

    shared.worker_bootstrapped = runtime.bootstrap_worker(
        worker_id,
        kernel::make_event(kernel::EventId::user0,
                           static_cast<std::uint32_t>(1u)));

    std::size_t loops = 0;
    while ((!shared.worker_finished || !shared.idle_seen_after_finish) &&
           loops < 64u) {
        (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());
        demo::ManualTimeSource::advance(1u);
        ++loops;
    }

    demo::inspect_runtime_trace(runtime_trace, shared, idle_id, worker_id);

    const bool trace_ok = shared.trace_negative_yield_ok &&
                          shared.trace_negative_sleep_ok &&
                          shared.trace_worker_bootstrap_ok &&
                          shared.trace_positive_yield_ok &&
                          shared.trace_positive_sleep_ok &&
                          shared.trace_tick_ok &&
                          shared.trace_idle_bootstrap_ok;

    const bool ok = shared.default_port_invalid_ok && shared.no_current_yield_ok &&
                    shared.no_current_sleep_ok && shared.port_valid &&
                    shared.worker_bootstrapped && shared.worker_yield_ok &&
                    shared.worker_sleep_ok && shared.worker_finished &&
                    shared.idle_seen_while_waiting &&
                    shared.idle_seen_after_finish && shared.idle_runs >= 2u &&
                    shared.worker_resumes == 3u && trace_ok && loops < 64u;

    std::printf(
        "[runtime-thread-port-demo] ok=%d invalid=%d no_current_yield=%d no_current_sleep=%d port=%d bootstrap=%d yield=%d sleep=%d idle_wait=%d idle_finish=%d idle_runs=%u resumes=%u loops=%llu\n",
        ok ? 1 : 0,
        shared.default_port_invalid_ok ? 1 : 0,
        shared.no_current_yield_ok ? 1 : 0,
        shared.no_current_sleep_ok ? 1 : 0,
        shared.port_valid ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        shared.worker_yield_ok ? 1 : 0,
        shared.worker_sleep_ok ? 1 : 0,
        shared.idle_seen_while_waiting ? 1 : 0,
        shared.idle_seen_after_finish ? 1 : 0,
        shared.idle_runs,
        shared.worker_resumes,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-thread-port-trace] ok=%d neg_yield=%d neg_sleep=%d worker_bootstrap=%d yield=%d sleep=%d tick=%d idle_bootstrap=%d wake_due=%llu\n",
        trace_ok ? 1 : 0,
        shared.trace_negative_yield_ok ? 1 : 0,
        shared.trace_negative_sleep_ok ? 1 : 0,
        shared.trace_worker_bootstrap_ok ? 1 : 0,
        shared.trace_positive_yield_ok ? 1 : 0,
        shared.trace_positive_sleep_ok ? 1 : 0,
        shared.trace_tick_ok ? 1 : 0,
        shared.trace_idle_bootstrap_ok ? 1 : 0,
        static_cast<unsigned long long>(shared.wake_due));
    return ok ? 0 : 1;
}
