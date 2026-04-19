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
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 32;
        static constexpr bool enable_trace = true;
        static constexpr std::size_t trace_capacity = 16;
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
    inline constexpr std::uint32_t kOldIdlePayload{11u};
    inline constexpr std::uint32_t kNewFallbackPayload{22u};
    inline constexpr std::uint32_t kNoTracePayload{33u};

    [[nodiscard]] kernel::Event make_old_idle_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(kOldIdlePayload));
    }

    [[nodiscard]] kernel::Event make_new_fallback_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1,
                                  static_cast<std::uint32_t>(
                                      kNewFallbackPayload));
    }

    [[nodiscard]] kernel::Event make_no_trace_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0,
                                  static_cast<std::uint32_t>(kNoTracePayload));
    }

    struct SharedState {
        bool initial_binding_ok{false};
        bool old_idle_bootstrapped{false};
        bool old_idle_run_ok{false};
        bool old_idle_seen{false};
        bool rebound_binding_ok{false};
        bool fallback_new_idle_run_ok{false};
        bool new_idle_seen{false};
        bool no_trace_bound_ok{false};
        bool no_trace_bootstrapped{false};
        bool no_trace_run_ok{false};
        bool no_trace_idle_seen{false};
        bool old_trace_ok{false};
        bool rerouted_trace_ok{false};
        bool trace_disabled_ok{false};
        std::uint32_t old_idle_runs{0};
        std::uint32_t new_idle_runs{0};
    };

    struct IdleContext {
        SharedState* shared{nullptr};
        const char* name{"idle"};
        bool is_new{false};
    };

    void handle_idle_step(IdleContext& context, kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[%s] init\n", context.name);
            return;
        }

        if (context.shared == nullptr) {
            return;
        }

        if (context.is_new) {
            ++context.shared->new_idle_runs;
            if (event.id == kernel::EventId::user1 &&
                kernel::payload_u32(event) == kNewFallbackPayload) {
                context.shared->new_idle_seen = true;
            }
            if (event.id == kernel::EventId::user0 &&
                kernel::payload_u32(event) == kNoTracePayload) {
                context.shared->no_trace_idle_seen = true;
            }
        } else {
            ++context.shared->old_idle_runs;
            if (event.id == kernel::EventId::user0 &&
                kernel::payload_u32(event) == kOldIdlePayload) {
                context.shared->old_idle_seen = true;
            }
        }

        std::printf("[%s] run=%u event=%u payload=%u now=%llu\n",
                    context.name,
                    context.is_new ? context.shared->new_idle_runs
                                   : context.shared->old_idle_runs,
                    static_cast<unsigned int>(event.id),
                    kernel::payload_u32(event),
                    static_cast<unsigned long long>(ManualTimeSource::now()));
    }

    void old_idle_step(IdleContext& context,
                       kernel::ThreadControl&,
                       kernel::Event event)
    {
        handle_idle_step(context, event);
    }

    void new_idle_step(IdleContext& context,
                       kernel::ThreadControl&,
                       kernel::Event event)
    {
        handle_idle_step(context, event);
    }

    using OldIdleTask =
        kernel::ThreadTask<IdleContext, &old_idle_step, kIdlePriority>;
    using NewIdleTask =
        kernel::ThreadTask<IdleContext, &new_idle_step, kIdlePriority>;

    template <typename TraceBuffer>
    [[nodiscard]] bool has_idle_trace_event(const TraceBuffer& trace,
                                            kernel::TaskId task,
                                            kernel::EventId event_id,
                                            std::uint32_t payload,
                                            bool ok) noexcept
    {
        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[runtime-trace] kind=%s task=%s%llu event=%u value=%llu ok=%d\n",
                kernel::runtime_trace_kind_name(record->kind),
                record->task_valid ? "" : "-",
                record->task_valid
                    ? static_cast<unsigned long long>(record->task.value)
                    : 0ull,
                static_cast<unsigned int>(record->event_id),
                static_cast<unsigned long long>(record->value),
                record->ok ? 1 : 0);

            if (record->kind != kernel::RuntimeTraceKind::idle_bootstrap) {
                continue;
            }

            if (!record->task_valid || record->task.value != task.value) {
                continue;
            }

            if (record->event_id != event_id) {
                continue;
            }

            if (record->value != payload) {
                continue;
            }

            if (record->ok != ok) {
                continue;
            }

            return true;
        }

        return false;
    }
}

int main()
{
    using Registry = kernel::TaskRegistry<demo::OldIdleTask, demo::NewIdleTask>;
    using RuntimeTrace =
        kernel::RuntimeTraceBuffer<demo::ManualTimeSource::Tick, 16>;

    demo::ManualTimeSource::reset();

    Registry registry{};
    demo::Caps caps{};
    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    RuntimeTrace trace_a{};
    RuntimeTrace trace_b{};
    demo::SharedState shared{};

    const auto old_idle_id = Registry::id_of<demo::OldIdleTask>();
    const auto new_idle_id = Registry::id_of<demo::NewIdleTask>();

    kernel::RuntimeBridge runtime{
        running,
        old_idle_id,
        demo::make_old_idle_event(),
        &trace_a,
    };

    auto& old_idle = registry.get<demo::OldIdleTask>();
    old_idle.context.shared = &shared;
    old_idle.context.name = "old-idle";
    old_idle.context.is_new = false;

    auto& new_idle = registry.get<demo::NewIdleTask>();
    new_idle.context.shared = &shared;
    new_idle.context.name = "new-idle";
    new_idle.context.is_new = true;

    while (running.run_once()) {
    }

    shared.initial_binding_ok =
        runtime.idle_task().value == old_idle_id.value &&
        runtime.idle_event().id == kernel::EventId::user0 &&
        kernel::payload_u32(runtime.idle_event()) == demo::kOldIdlePayload &&
        runtime.trace() == &trace_a;

    shared.old_idle_bootstrapped = runtime.bootstrap_idle();
    shared.old_idle_run_ok =
        runtime.run_once_or_idle(demo::ManualTimeSource::now());
    demo::ManualTimeSource::advance(1u);

    const auto trace_a_size_before_rebind = trace_a.size();

    runtime.bind_trace(&trace_b);
    runtime.bind_idle(new_idle_id, demo::make_new_fallback_event());
    shared.rebound_binding_ok =
        runtime.idle_task().value == new_idle_id.value &&
        runtime.idle_event().id == kernel::EventId::user1 &&
        kernel::payload_u32(runtime.idle_event()) == demo::kNewFallbackPayload &&
        runtime.trace() == &trace_b;

    shared.fallback_new_idle_run_ok =
        runtime.run_once_or_idle(demo::ManualTimeSource::now());
    demo::ManualTimeSource::advance(1u);

    const auto trace_b_size_before_disable = trace_b.size();

    runtime.bind_trace(nullptr);
    shared.no_trace_bound_ok = runtime.trace() == nullptr;
    shared.no_trace_bootstrapped =
        runtime.bootstrap_idle(demo::make_no_trace_event());
    shared.no_trace_run_ok =
        runtime.run_once_or_idle(demo::ManualTimeSource::now());

    shared.old_trace_ok = trace_a.size() == trace_a_size_before_rebind &&
                          demo::has_idle_trace_event(trace_a,
                                                     old_idle_id,
                                                     kernel::EventId::user0,
                                                     demo::kOldIdlePayload,
                                                     true);

    shared.rerouted_trace_ok =
        trace_a.size() == trace_a_size_before_rebind &&
        trace_b.size() == trace_b_size_before_disable &&
        demo::has_idle_trace_event(trace_b,
                                   new_idle_id,
                                   kernel::EventId::user1,
                                   demo::kNewFallbackPayload,
                                   true);

    shared.trace_disabled_ok =
        trace_b.size() == trace_b_size_before_disable &&
        shared.no_trace_bound_ok && shared.no_trace_bootstrapped &&
        shared.no_trace_run_ok && shared.no_trace_idle_seen;

    const bool ok = shared.initial_binding_ok && shared.old_idle_bootstrapped &&
                    shared.old_idle_run_ok && shared.old_idle_seen &&
                    shared.rebound_binding_ok &&
                    shared.fallback_new_idle_run_ok && shared.new_idle_seen &&
                    shared.old_trace_ok && shared.rerouted_trace_ok &&
                    shared.trace_disabled_ok && shared.old_idle_runs == 1u &&
                    shared.new_idle_runs == 2u;

    std::printf(
        "[runtime-bridge-binding-demo] ok=%d initial=%d old_post=%d old_run=%d old_seen=%d rebind=%d new_run=%d new_seen=%d no_trace=%d old_runs=%u new_runs=%u\n",
        ok ? 1 : 0,
        shared.initial_binding_ok ? 1 : 0,
        shared.old_idle_bootstrapped ? 1 : 0,
        shared.old_idle_run_ok ? 1 : 0,
        shared.old_idle_seen ? 1 : 0,
        shared.rebound_binding_ok ? 1 : 0,
        shared.fallback_new_idle_run_ok ? 1 : 0,
        shared.new_idle_seen ? 1 : 0,
        shared.trace_disabled_ok ? 1 : 0,
        shared.old_idle_runs,
        shared.new_idle_runs);
    std::printf(
        "[runtime-bridge-binding-trace] ok=%d old=%d rerouted=%d disabled=%d trace_a=%llu trace_b=%llu\n",
        (shared.old_trace_ok && shared.rerouted_trace_ok &&
         shared.trace_disabled_ok)
            ? 1
            : 0,
        shared.old_trace_ok ? 1 : 0,
        shared.rerouted_trace_ok ? 1 : 0,
        shared.trace_disabled_ok ? 1 : 0,
        static_cast<unsigned long long>(trace_a.size()),
        static_cast<unsigned long long>(trace_b.size()));

    return ok ? 0 : 1;
}
