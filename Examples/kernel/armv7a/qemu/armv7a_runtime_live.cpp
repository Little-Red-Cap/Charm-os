#include "armv7a_runtime_live.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_interrupt_runtime_hook.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "targets/armv7a/common/armv7a_psr_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_bridge_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

import kernel.capabilities;
import kernel.config;
import kernel.context;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.runtime_trap;
import kernel.scheduler;
import kernel.task_state;
import kernel.thread;

namespace {
constexpr kernel::Priority kArmv7aLiveIdlePriority{0u};
constexpr kernel::Priority kArmv7aLiveWorkerPriority{1u};
constexpr std::uint32_t kArmv7aLiveWorkerBootstrapPayload = 1u;
constexpr std::uint32_t kArmv7aLiveWorkerYieldPayload = 1u;
constexpr std::uint64_t kArmv7aLiveWakeDeltaTicks = 0x00004000ull;
constexpr std::uint64_t kArmv7aLiveWaitTimeoutTicks = 0x00080000ull;
constexpr std::size_t kArmv7aLiveTraceCapacity = 32u;
constexpr std::size_t kArmv7aLiveLoopBudget = 65536u;

struct Armv7aLiveRuntimeConfig : kernel::KernelConfig {
    static constexpr bool enable_timer = true;
    static constexpr std::size_t priority_levels = 2u;
    static constexpr std::size_t evtq_capacity = 32u;
    static constexpr std::size_t timer_capacity = 8u;
    static constexpr bool enable_trace = true;
    static constexpr std::size_t trace_capacity = kArmv7aLiveTraceCapacity;
};

struct Armv7aPlatformTimeSource {
    using Tick = std::uint64_t;

    static Tick now() noexcept
    {
        return armv7a_platform_timer_counter();
    }
};

struct Armv7aPlatformIrqGuard {
    struct Token {
        bool irq_masked = true;
    };

    static Token enter() noexcept
    {
        const auto psr = armv7a_read_cpsr();
        if (!armv7a_irq_masked(psr)) {
            armv7a_disable_irq();
        }
        return Token{
            .irq_masked = armv7a_irq_masked(psr),
        };
    }

    static void leave(Token token) noexcept
    {
        if (!token.irq_masked) {
            armv7a_enable_irq();
        }
    }
};

struct Armv7aLiveRuntimeCaps {
    using TimeSource = Armv7aPlatformTimeSource;
    using IrqGuard = Armv7aPlatformIrqGuard;
    using Wakeup = kernel::NoopWakeup;
    using SwiTrigger = kernel::NoopSwiTrigger;
};

struct Armv7aLiveSharedState {
    kernel::TaskId worker_id{};
    bool worker_bootstrap_seen = false;
    bool yield_resume_seen = false;
    bool yield_return_ok = false;
    bool irq_enabled_after_yield = false;
    bool yield_trap_ok = false;
    bool sleep_requested = false;
    bool sleep_return_ok = false;
    bool irq_enabled_after_sleep = false;
    bool sleep_trap_ok = false;
    bool timer_arm_requested = false;
    bool timer_arm_seen = false;
    bool timer_armed = false;
    bool timer_irq_seen = false;
    bool tick_advanced = false;
    bool tick_resume_seen = false;
    bool worker_finished = false;
    bool idle_after_finish = false;
    bool wait_timeout_seen = false;
    bool trap_task_matches = false;
    bool trap_stack_matches = false;
    bool runtime_trace_worker_bootstrap = false;
    bool runtime_trace_yield = false;
    bool runtime_trace_sleep = false;
    bool runtime_trace_tick = false;
    bool runtime_trace_idle = false;
    bool trap_trace_yield = false;
    bool trap_trace_sleep = false;
    bool yield_frame_sampled = false;
    bool sleep_frame_sampled = false;
    std::uint32_t worker_resumes = 0u;
    std::uint32_t idle_runs = 0u;
    std::uint32_t cpsr_after_enable_irq = 0u;
    std::uint32_t cpsr_before_bootstrap = 0u;
    std::uint32_t cpsr_after_bootstrap = 0u;
    std::uint32_t cpsr_before_yield = 0u;
    std::uint32_t cpsr_after_yield = 0u;
    std::uint32_t cpsr_before_sleep = 0u;
    std::uint32_t cpsr_after_sleep = 0u;
    std::uint32_t cpsr_wait_timeout = 0u;
    std::uint32_t timer_arm_ticks = 0u;
    std::uint32_t timer_ctrl = 0u;
    std::uint32_t gicd_ctlr = 0u;
    std::uint32_t gicc_ctlr = 0u;
    std::uint32_t hppir = 0u;
    bool timer_line_enabled = false;
    bool timer_line_pending = false;
    bool timer_line_active = false;
    std::uint32_t yield_origin_psr = 0u;
    std::uint32_t yield_handler_psr = 0u;
    std::uint32_t sleep_origin_psr = 0u;
    std::uint32_t sleep_handler_psr = 0u;
    std::uint64_t origin_stack = 0u;
    std::uint64_t wake_due = 0u;
    std::uint64_t last_tick_now = 0u;
    std::uint64_t last_trap_value = 0u;
};

struct Armv7aLiveIdleContext {
    Armv7aLiveSharedState* shared = nullptr;
};

struct Armv7aLiveWorkerContext {
    Armv7aLiveSharedState* shared = nullptr;
};

[[nodiscard]] kernel::Event armv7a_live_idle_event() noexcept
{
    return kernel::make_event(kernel::EventId::user0);
}

[[nodiscard]] kernel::Event armv7a_live_worker_bootstrap_event() noexcept
{
    return kernel::make_event(kernel::EventId::user0,
                              kArmv7aLiveWorkerBootstrapPayload);
}

[[nodiscard]] kernel::Event armv7a_live_worker_yield_event() noexcept
{
    return kernel::make_event(kernel::EventId::user1,
                              kArmv7aLiveWorkerYieldPayload);
}

[[nodiscard]] kernel::Event armv7a_live_sleep_event(
    Armv7aPlatformTimeSource::Tick due) noexcept
{
    return kernel::make_event(kernel::EventId::tick, due);
}

void armv7a_live_idle_step(Armv7aLiveIdleContext& context,
                           kernel::ThreadControl&,
                           kernel::Event event)
{
    if (context.shared == nullptr || event.id == kernel::EventId::init) {
        return;
    }

    ++context.shared->idle_runs;
    if (context.shared->worker_finished &&
        event.id == kernel::EventId::user0) {
        context.shared->idle_after_finish = true;
    }
}

void armv7a_live_worker_step(Armv7aLiveWorkerContext& context,
                             kernel::ThreadControl& control,
                             kernel::Event event)
{
    if (context.shared == nullptr) {
        return;
    }

    if (event.id == kernel::EventId::init) {
        return;
    }

    if (event.id == kernel::EventId::terminate) {
        control.finish();
        return;
    }

    if (event.id == kernel::EventId::user0 &&
        kernel::payload_u32(event) == kArmv7aLiveWorkerBootstrapPayload) {
        ++context.shared->worker_resumes;
        context.shared->worker_bootstrap_seen = true;
        context.shared->origin_stack =
            static_cast<std::uint64_t>(armv7a_read_sp());
        context.shared->cpsr_before_yield = armv7a_read_cpsr();
        context.shared->yield_return_ok =
            armv7a_runtime_yield_call(1u, 1u) == 1u;
        context.shared->cpsr_after_yield = armv7a_read_cpsr();
        context.shared->irq_enabled_after_yield =
            !armv7a_irq_masked(context.shared->cpsr_after_yield);
        return;
    }

    if (event.id == kernel::EventId::user1 &&
        kernel::payload_u32(event) == kArmv7aLiveWorkerYieldPayload) {
        ++context.shared->worker_resumes;
        context.shared->yield_resume_seen = true;
        const auto now = Armv7aPlatformTimeSource::now();
        context.shared->wake_due = now + kArmv7aLiveWakeDeltaTicks;
        context.shared->timer_arm_requested = true;
        context.shared->sleep_requested = true;
        context.shared->origin_stack =
            static_cast<std::uint64_t>(armv7a_read_sp());
        context.shared->cpsr_before_sleep = armv7a_read_cpsr();
        context.shared->sleep_return_ok =
            armv7a_runtime_sleep_until_call(
                context.shared->wake_due,
                static_cast<std::uint32_t>(kernel::EventId::tick),
                static_cast<std::uint32_t>(
                    context.shared->wake_due & 0xFFFF'FFFFull)) ==
            static_cast<std::uint32_t>(
                context.shared->wake_due & 0xFFFF'FFFFull);
        context.shared->cpsr_after_sleep = armv7a_read_cpsr();
        context.shared->irq_enabled_after_sleep =
            !armv7a_irq_masked(context.shared->cpsr_after_sleep);
        return;
    }

    if (event.id == kernel::EventId::tick &&
        kernel::payload_u64(event) == context.shared->wake_due) {
        ++context.shared->worker_resumes;
        context.shared->tick_resume_seen = true;
        context.shared->worker_finished = true;
        control.finish();
    }
}

using Armv7aLiveIdleTask = kernel::ThreadTask<Armv7aLiveIdleContext,
                                              &armv7a_live_idle_step,
                                              kArmv7aLiveIdlePriority>;
using Armv7aLiveWorkerTask = kernel::ThreadTask<Armv7aLiveWorkerContext,
                                                &armv7a_live_worker_step,
                                                kArmv7aLiveWorkerPriority>;
using Armv7aLiveRegistry =
    kernel::TaskRegistry<Armv7aLiveIdleTask, Armv7aLiveWorkerTask>;
using Armv7aLiveScheduler =
    kernel::Scheduler<Armv7aLiveRuntimeConfig,
                      Armv7aLiveRegistry,
                      Armv7aLiveRuntimeCaps,
                      kernel::state::Running>;
using Armv7aLiveCreatedScheduler =
    decltype(kernel::make_scheduler<Armv7aLiveRuntimeConfig>(
        std::declval<Armv7aLiveRegistry&>(),
        std::declval<Armv7aLiveRuntimeCaps&>()));
using Armv7aLiveRunningScheduler =
    decltype(kernel::start(std::declval<Armv7aLiveCreatedScheduler&&>()));
using Armv7aLiveRuntimeTrace =
    kernel::RuntimeTraceBuffer<Armv7aPlatformTimeSource::Tick,
                               kArmv7aLiveTraceCapacity>;
using Armv7aLiveTrapTrace =
    kernel::RuntimeTrapTraceBuffer<Armv7aPlatformTimeSource::Tick, 16u>;
using Armv7aLiveRuntime =
    kernel::RuntimeBridge<Armv7aLiveScheduler, Armv7aLiveRuntimeTrace>;
using Armv7aLiveTrapBridge =
    kernel::RuntimeTrapBridge<Armv7aLiveRuntime, Armv7aLiveTrapTrace>;
using Armv7aLiveLoopPort =
    decltype(kernel::make_runtime_loop_port(std::declval<Armv7aLiveRuntime&>()));

struct Armv7aLiveRuntimeContext {
    Armv7aLiveLoopPort loop_port{};
    Armv7aLiveTrapBridge* trap = nullptr;
    Armv7aLiveSharedState* shared = nullptr;
};

struct Armv7aLiveSession {
    Armv7aLiveRegistry registry{};
    Armv7aLiveRuntimeCaps caps{};
    Armv7aLiveRunningScheduler running;
    Armv7aLiveRuntimeTrace runtime_trace{};
    Armv7aLiveTrapTrace trap_trace{};
    Armv7aLiveSharedState shared{};
    kernel::TaskId idle_id{};
    kernel::TaskId worker_id{};
    Armv7aLiveRuntime runtime;
    Armv7aLiveLoopPort loop_port;
    Armv7aLiveTrapBridge trap;
    Armv7aLiveRuntimeContext runtime_context{};

    Armv7aLiveSession() noexcept
        : running(kernel::start(kernel::make_scheduler<
                    Armv7aLiveRuntimeConfig>(registry, caps))),
          idle_id(Armv7aLiveRegistry::id_of<Armv7aLiveIdleTask>()),
          worker_id(Armv7aLiveRegistry::id_of<Armv7aLiveWorkerTask>()),
          runtime(running, idle_id, armv7a_live_idle_event(), &runtime_trace),
          loop_port(kernel::make_runtime_loop_port(runtime)),
          trap(runtime,
               kernel::RuntimeTrapPolicy<Armv7aPlatformTimeSource::Tick>{
                   .yield_resume_event = armv7a_live_worker_yield_event(),
                   .sleep_resume_event =
                       kernel::make_event(kernel::EventId::tick),
                   .sleep_event_factory = &armv7a_live_sleep_event,
               },
               &trap_trace),
          runtime_context{
              .loop_port = loop_port,
              .trap = &trap,
              .shared = &shared,
          }
    {
        shared.worker_id = worker_id;

        auto& idle = registry.get<Armv7aLiveIdleTask>();
        idle.context.shared = &shared;

        auto& worker = registry.get<Armv7aLiveWorkerTask>();
        worker.context.shared = &shared;

        while (running.run_once()) {
        }
    }
};

alignas(Armv7aLiveSession) std::byte
    g_armv7a_live_session_storage[sizeof(Armv7aLiveSession)];
bool g_armv7a_live_session_ready = false;
Armv7aRuntimeLiveObservation g_armv7a_last_runtime_live_observation{};
bool g_armv7a_last_runtime_live_observation_valid = false;

Armv7aLiveSession& armv7a_prepare_live_session() noexcept
{
    auto* session = std::launder(reinterpret_cast<Armv7aLiveSession*>(
        g_armv7a_live_session_storage));
    if (g_armv7a_live_session_ready) {
        session->~Armv7aLiveSession();
    }

    new (session) Armv7aLiveSession{};
    g_armv7a_live_session_ready = true;
    return *session;
}

[[nodiscard]] bool armv7a_runtime_trap_origin_to_kernel_origin(
    Armv7aRuntimeTrapOrigin origin,
    kernel::TrapOrigin& out) noexcept
{
    switch (origin) {
    case Armv7aRuntimeTrapOrigin::kernel_thread:
        out = kernel::TrapOrigin::kernel_thread;
        return true;
    case Armv7aRuntimeTrapOrigin::user_task:
        out = kernel::TrapOrigin::user_task;
        return true;
    case Armv7aRuntimeTrapOrigin::supervisor:
        out = kernel::TrapOrigin::supervisor;
        return true;
    case Armv7aRuntimeTrapOrigin::isr:
        out = kernel::TrapOrigin::isr;
        return true;
    case Armv7aRuntimeTrapOrigin::unknown:
    default:
        return false;
    }
}

[[nodiscard]] Armv7aRuntimeTrapIngressDisposition
armv7a_runtime_trap_disposition_from_kernel(
    kernel::TrapDisposition disposition) noexcept
{
    switch (disposition) {
    case kernel::TrapDisposition::handled:
        return Armv7aRuntimeTrapIngressDisposition::handled;
    case kernel::TrapDisposition::unsupported:
        return Armv7aRuntimeTrapIngressDisposition::unsupported;
    case kernel::TrapDisposition::rejected:
    default:
        return Armv7aRuntimeTrapIngressDisposition::rejected;
    }
}

[[nodiscard]] Armv7aRuntimeTrapIngressError
armv7a_runtime_trap_error_from_kernel(kernel::TrapError error) noexcept
{
    switch (error) {
    case kernel::TrapError::none:
        return Armv7aRuntimeTrapIngressError::none;
    case kernel::TrapError::decode_failed:
        return Armv7aRuntimeTrapIngressError::decode_failed;
    case kernel::TrapError::writeback_failed:
        return Armv7aRuntimeTrapIngressError::writeback_failed;
    case kernel::TrapError::unsupported_service:
        return Armv7aRuntimeTrapIngressError::unsupported_service;
    case kernel::TrapError::unbound_adapter:
    case kernel::TrapError::unbound_bridge:
        return Armv7aRuntimeTrapIngressError::unbound_adapter;
    case kernel::TrapError::no_current_task:
    case kernel::TrapError::invalid_origin:
    case kernel::TrapError::invalid_argument:
    default:
        return Armv7aRuntimeTrapIngressError::unsupported_service;
    }
}

bool armv7a_capture_live_runtime_current(
    void* ctx,
    Armv7aRuntimeCurrentContext& out) noexcept
{
    auto* context = static_cast<Armv7aLiveRuntimeContext*>(ctx);
    if (context == nullptr || context->shared == nullptr ||
        !kernel::has_current()) {
        out = {};
        return false;
    }

    const auto stack_pointer = context->shared->origin_stack != 0u
        ? context->shared->origin_stack
        : static_cast<std::uint64_t>(armv7a_read_sp());
    out = Armv7aRuntimeCurrentContext{
        .stack_pointer = stack_pointer,
        .task = kernel::current_task().value,
        .task_valid = true,
    };
    return true;
}

Armv7aRuntimeTrapIngressResult armv7a_dispatch_live_runtime_trap(
    void* ctx,
    const Armv7aRuntimeTrapSeamFrameView& frame) noexcept
{
    auto* context = static_cast<Armv7aLiveRuntimeContext*>(ctx);
    if (context == nullptr || context->trap == nullptr || context->shared == nullptr) {
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
            .error = Armv7aRuntimeTrapIngressError::unbound_adapter,
            .value = 0u,
        };
    }

    kernel::TrapOrigin origin{};
    if (!armv7a_runtime_trap_origin_to_kernel_origin(frame.origin, origin)) {
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
            .error = Armv7aRuntimeTrapIngressError::unsupported_service,
            .value = 0u,
        };
    }

    auto result = context->trap->dispatch(kernel::TrapRequest{
        .service = static_cast<kernel::TrapService>(frame.service_id),
        .arg0 = frame.arg0,
        .arg1 = frame.arg1,
        .arg2 = frame.arg2,
        .arg3 = frame.arg3,
        .return_pc = frame.return_pc,
        .stack_pointer = frame.stack_pointer,
        .status = frame.status,
        .origin = origin,
        .task = kernel::TaskId{
            .value = static_cast<std::size_t>(frame.task),
        },
        .task_valid = frame.task_valid,
    });

    context->shared->last_trap_value = result.value;
    context->shared->trap_task_matches =
        frame.task_valid &&
        frame.task == context->shared->worker_id.value;
    context->shared->trap_stack_matches =
        context->shared->origin_stack != 0u &&
        frame.stack_pointer == context->shared->origin_stack;

    if (frame.service_id == static_cast<std::uint16_t>(
                               kernel::TrapService::yield_current)) {
        context->shared->yield_trap_ok =
            result.ok() && result.value == 1u;
    } else if (frame.service_id == static_cast<std::uint16_t>(
                                      kernel::TrapService::sleep_until)) {
        context->shared->sleep_trap_ok =
            result.ok() && result.value == context->shared->wake_due;
    }

    return Armv7aRuntimeTrapIngressResult{
        .disposition = armv7a_runtime_trap_disposition_from_kernel(
            result.disposition),
        .error = armv7a_runtime_trap_error_from_kernel(result.error),
        .value = result.value,
    };
}

bool armv7a_handle_live_timer_interrupt(
    void* ctx,
    unsigned int intid,
    const Armv7aExceptionFrame&,
    bool fiq_route) noexcept
{
    auto* context = static_cast<Armv7aLiveRuntimeContext*>(ctx);
    if (context == nullptr || context->shared == nullptr || fiq_route ||
        !armv7a_platform_is_timer_interrupt(intid)) {
        return false;
    }

    const auto now = Armv7aPlatformTimeSource::now();
    context->shared->timer_irq_seen = true;
    context->shared->timer_armed = false;
    context->shared->last_tick_now = now;
    context->shared->tick_advanced =
        context->loop_port.advance_tick(now) != 0u;
    return true;
}

void armv7a_restore_irq_mask(std::uint32_t saved_psr) noexcept
{
    if (armv7a_irq_masked(saved_psr)) {
        armv7a_disable_irq();
    } else {
        armv7a_enable_irq();
    }
}

[[nodiscard]] std::uint32_t armv7a_live_timer_ticks_until_due(
    std::uint64_t due,
    std::uint64_t now) noexcept
{
    if (due <= now) {
        return 1u;
    }

    const auto delta = due - now;
    return delta > 0xFFFF'FFFFull
        ? 0xFFFF'FFFFu
        : static_cast<std::uint32_t>(delta);
}

void armv7a_arm_live_runtime_timer(Armv7aLiveSharedState& shared) noexcept
{
    if (!shared.timer_arm_requested || shared.timer_armed) {
        return;
    }

    const auto now = Armv7aPlatformTimeSource::now();
    shared.timer_arm_ticks = armv7a_live_timer_ticks_until_due(
        shared.wake_due, now);
    armv7a_platform_prepare_timer_interrupt();
    armv7a_platform_enable_interrupt_controller(
        Armv7aPlatformInterruptRoute::kIrq);
    armv7a_platform_timer_start_oneshot(shared.timer_arm_ticks);
    shared.timer_ctrl = armv7a_platform_timer_control();
    const auto controller = armv7a_platform_interrupt_controller_state();
    shared.gicd_ctlr = controller.distributor_control;
    shared.gicc_ctlr = controller.cpu_control;
    shared.hppir = controller.highest_pending;
    const auto line = armv7a_platform_nonsecure_timer_interrupt_line_state();
    shared.timer_line_enabled = line.line_enabled;
    shared.timer_line_pending = line.line_pending;
    shared.timer_line_active = line.line_active;
    shared.timer_arm_seen = true;
    shared.timer_arm_requested = false;
    shared.timer_armed = true;
}

void armv7a_wait_for_live_runtime_timer(Armv7aLiveSharedState& shared) noexcept
{
    if (!shared.timer_armed || shared.timer_irq_seen || shared.wait_timeout_seen) {
        return;
    }

    const auto timeout = shared.wake_due + kArmv7aLiveWaitTimeoutTicks;
    while (!shared.timer_irq_seen) {
        const auto now = Armv7aPlatformTimeSource::now();
        if (now >= timeout) {
            shared.wait_timeout_seen = true;
            shared.cpsr_wait_timeout = armv7a_read_cpsr();
            shared.timer_ctrl = armv7a_platform_timer_control();
            const auto controller =
                armv7a_platform_interrupt_controller_state();
            shared.gicd_ctlr = controller.distributor_control;
            shared.gicc_ctlr = controller.cpu_control;
            shared.hppir = controller.highest_pending;
            const auto line =
                armv7a_platform_nonsecure_timer_interrupt_line_state();
            shared.timer_line_enabled = line.line_enabled;
            shared.timer_line_pending = line.line_pending;
            shared.timer_line_active = line.line_active;
            break;
        }

        armv7a_compiler_barrier();
    }
}

void armv7a_capture_live_runtime_svc_samples(
    Armv7aLiveSharedState& shared) noexcept
{
    const auto yield_sample = armv7a_svc_frame_sample_for_immediate(
        kArmv7aRuntimeBridgeYieldServiceId);
    shared.yield_frame_sampled = yield_sample.frame_sampled;
    if (yield_sample.frame_sampled) {
        shared.yield_origin_psr = yield_sample.frame.spsr;
        shared.yield_handler_psr = yield_sample.handler_psr;
    }

    const auto sleep_sample = armv7a_svc_frame_sample_for_immediate(
        kArmv7aRuntimeBridgeSleepServiceId);
    shared.sleep_frame_sampled = sleep_sample.frame_sampled;
    if (sleep_sample.frame_sampled) {
        shared.sleep_origin_psr = sleep_sample.frame.spsr;
        shared.sleep_handler_psr = sleep_sample.handler_psr;
    }
}

void armv7a_print_runtime_live_psr_sample(const char* label,
                                          std::uint32_t psr) noexcept
{
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts("=0x");
    armv7a_diag_put_hex(psr);
    armv7a_platform_early_console_puts("(");
    armv7a_platform_early_console_puts(
        armv7a_irq_masked(psr) ? "irq-masked" : "irq-enabled");
    armv7a_platform_early_console_puts(")");
}

template <typename TraceBuffer>
void armv7a_inspect_live_runtime_trace(const TraceBuffer& trace,
                                       Armv7aLiveSharedState& shared,
                                       kernel::TaskId idle_id,
                                       kernel::TaskId worker_id) noexcept
{
    for (std::size_t i = 0; i < trace.size(); ++i) {
        const auto* record = trace.at(i);
        if (record == nullptr) {
            continue;
        }

        switch (record->kind) {
        case kernel::RuntimeTraceKind::worker_bootstrap:
            shared.runtime_trace_worker_bootstrap =
                shared.runtime_trace_worker_bootstrap ||
                (record->task_valid &&
                 record->task.value == worker_id.value &&
                 record->ok);
            break;
        case kernel::RuntimeTraceKind::yield:
            shared.runtime_trace_yield =
                shared.runtime_trace_yield ||
                (record->task_valid &&
                 record->task.value == worker_id.value &&
                 record->ok);
            break;
        case kernel::RuntimeTraceKind::sleep:
            shared.runtime_trace_sleep =
                shared.runtime_trace_sleep ||
                (record->task_valid &&
                 record->task.value == worker_id.value &&
                 record->ok &&
                 record->value == shared.wake_due);
            break;
        case kernel::RuntimeTraceKind::tick:
            shared.runtime_trace_tick =
                shared.runtime_trace_tick ||
                (!record->task_valid && record->ok && record->value != 0u);
            break;
        case kernel::RuntimeTraceKind::idle_bootstrap:
            shared.runtime_trace_idle =
                shared.runtime_trace_idle ||
                (record->task_valid &&
                 record->task.value == idle_id.value &&
                 record->ok);
            break;
        case kernel::RuntimeTraceKind::isr_defer:
            break;
        }
    }
}

template <typename TraceBuffer>
void armv7a_inspect_live_trap_trace(const TraceBuffer& trace,
                                    Armv7aLiveSharedState& shared,
                                    kernel::TaskId worker_id) noexcept
{
    for (std::size_t i = 0; i < trace.size(); ++i) {
        const auto* record = trace.at(i);
        if (record == nullptr || !record->task_valid ||
            record->task.value != worker_id.value ||
            record->disposition != kernel::TrapDisposition::handled ||
            record->error != kernel::TrapError::none) {
            continue;
        }

        if (record->service == kernel::TrapService::yield_current &&
            record->value == 1u) {
            shared.trap_trace_yield = true;
        } else if (
            record->service == kernel::TrapService::sleep_until &&
            record->value == shared.wake_due) {
            shared.trap_trace_sleep = true;
        }
    }
}
} // namespace

Armv7aRuntimeLiveObservation armv7a_run_runtime_live_observation() noexcept
{
    auto& session = armv7a_prepare_live_session();

    const auto saved_psr = armv7a_read_cpsr();
    armv7a_bind_runtime_current_context_port(Armv7aRuntimeCurrentContextPort{
        .ctx = &session.runtime_context,
        .capture = &armv7a_capture_live_runtime_current,
    });
    armv7a_bind_runtime_trap_dispatch_port(Armv7aRuntimeTrapDispatchPort{
        .ctx = &session.runtime_context,
        .dispatch_frame = &armv7a_dispatch_live_runtime_trap,
    });
    armv7a_bind_interrupt_runtime_hook(Armv7aInterruptRuntimeHook{
        .ctx = &session.runtime_context,
        .on_delivery = &armv7a_handle_live_timer_interrupt,
    });

    armv7a_platform_disable_interrupt_controller();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_enable_interrupt_controller(
        Armv7aPlatformInterruptRoute::kIrq);
    armv7a_enable_irq();
    session.shared.cpsr_after_enable_irq = armv7a_read_cpsr();
    session.shared.cpsr_before_bootstrap = armv7a_read_cpsr();

    const auto worker_bootstrapped = session.loop_port.bootstrap_worker(
        session.worker_id, armv7a_live_worker_bootstrap_event());
    session.shared.cpsr_after_bootstrap = armv7a_read_cpsr();

    std::size_t loops = 0u;
    while ((!session.shared.worker_finished ||
            !session.shared.idle_after_finish) &&
           !session.shared.wait_timeout_seen &&
           loops < kArmv7aLiveLoopBudget) {
        armv7a_arm_live_runtime_timer(session.shared);
        armv7a_wait_for_live_runtime_timer(session.shared);
        (void)session.loop_port.run_once_or_idle(
            Armv7aPlatformTimeSource::now());
        ++loops;
    }

    armv7a_platform_timer_stop();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_disable_interrupt_controller();
    armv7a_restore_irq_mask(saved_psr);
    armv7a_unbind_interrupt_runtime_hook();
    armv7a_unbind_runtime_trap_dispatch_port();
    armv7a_unbind_runtime_current_context_port();
    armv7a_capture_live_runtime_svc_samples(session.shared);

    armv7a_inspect_live_runtime_trace(session.runtime_trace,
                                      session.shared,
                                      session.idle_id,
                                      session.worker_id);
    armv7a_inspect_live_trap_trace(session.trap_trace,
                                   session.shared,
                                   session.worker_id);

    const auto observation = Armv7aRuntimeLiveObservation{
        .task_ready = session.shared.trap_task_matches &&
                      session.shared.trap_stack_matches,
        .trap_ready = session.shared.yield_return_ok &&
                      session.shared.sleep_return_ok &&
                      session.shared.yield_trap_ok &&
                      session.shared.sleep_trap_ok &&
                      session.shared.trap_trace_yield &&
                      session.shared.trap_trace_sleep,
        .timer_ready = worker_bootstrapped && session.shared.timer_arm_seen &&
                       session.shared.timer_irq_seen,
        .tick_ready = session.shared.tick_advanced &&
                      session.shared.tick_resume_seen &&
                      session.shared.runtime_trace_tick,
        .idle_ready = session.shared.idle_after_finish &&
                      session.shared.runtime_trace_idle,
        .worker_ready = worker_bootstrapped &&
                        session.shared.worker_bootstrap_seen &&
                        session.shared.yield_resume_seen &&
                        session.shared.sleep_requested &&
                        session.shared.worker_finished &&
                        session.shared.worker_resumes == 3u &&
                        session.shared.runtime_trace_worker_bootstrap &&
                        session.shared.runtime_trace_yield &&
                        session.shared.runtime_trace_sleep,
        .irq_enabled_after_yield = session.shared.irq_enabled_after_yield,
        .irq_enabled_after_sleep = session.shared.irq_enabled_after_sleep,
        .timer_arm_seen = session.shared.timer_arm_seen,
        .timer_armed = session.shared.timer_armed,
        .timer_irq_seen = session.shared.timer_irq_seen,
        .timer_line_enabled = session.shared.timer_line_enabled,
        .timer_line_pending = session.shared.timer_line_pending,
        .timer_line_active = session.shared.timer_line_active,
        .wait_timeout_seen = session.shared.wait_timeout_seen,
        .yield_trap_ok = session.shared.yield_trap_ok,
        .sleep_trap_ok = session.shared.sleep_trap_ok,
        .runtime_trace_worker_bootstrap =
            session.shared.runtime_trace_worker_bootstrap,
        .runtime_trace_yield = session.shared.runtime_trace_yield,
        .runtime_trace_sleep = session.shared.runtime_trace_sleep,
        .runtime_trace_tick = session.shared.runtime_trace_tick,
        .runtime_trace_idle = session.shared.runtime_trace_idle,
        .trap_trace_yield = session.shared.trap_trace_yield,
        .trap_trace_sleep = session.shared.trap_trace_sleep,
        .yield_frame_sampled = session.shared.yield_frame_sampled,
        .sleep_frame_sampled = session.shared.sleep_frame_sampled,
        .worker_resumes = session.shared.worker_resumes,
        .idle_runs = session.shared.idle_runs,
        .cpsr_after_enable_irq = session.shared.cpsr_after_enable_irq,
        .cpsr_before_bootstrap = session.shared.cpsr_before_bootstrap,
        .cpsr_after_bootstrap = session.shared.cpsr_after_bootstrap,
        .cpsr_before_yield = session.shared.cpsr_before_yield,
        .cpsr_after_yield = session.shared.cpsr_after_yield,
        .cpsr_before_sleep = session.shared.cpsr_before_sleep,
        .cpsr_after_sleep = session.shared.cpsr_after_sleep,
        .cpsr_wait_timeout = session.shared.cpsr_wait_timeout,
        .timer_arm_ticks = session.shared.timer_arm_ticks,
        .timer_ctrl = session.shared.timer_ctrl,
        .gicd_ctlr = session.shared.gicd_ctlr,
        .gicc_ctlr = session.shared.gicc_ctlr,
        .hppir = session.shared.hppir,
        .yield_origin_psr = session.shared.yield_origin_psr,
        .yield_handler_psr = session.shared.yield_handler_psr,
        .sleep_origin_psr = session.shared.sleep_origin_psr,
        .sleep_handler_psr = session.shared.sleep_handler_psr,
        .wake_due = session.shared.wake_due,
        .last_tick_now = session.shared.last_tick_now,
        .last_trap_value = session.shared.last_trap_value,
    };

    g_armv7a_last_runtime_live_observation = observation;
    g_armv7a_last_runtime_live_observation_valid = true;
    return observation;
}

Armv7aRuntimeLiveObservation armv7a_last_runtime_live_observation() noexcept
{
    return g_armv7a_last_runtime_live_observation_valid
        ? g_armv7a_last_runtime_live_observation
        : Armv7aRuntimeLiveObservation{};
}

void armv7a_print_runtime_live_observation()
{
    const auto observation = armv7a_run_runtime_live_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime live, task=");
    armv7a_platform_early_console_puts(observation.task_ready ? "yes" : "no");
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(observation.trap_ready ? "yes" : "no");
    armv7a_platform_early_console_puts(", timer=");
    armv7a_platform_early_console_puts(observation.timer_ready ? "yes" : "no");
    armv7a_platform_early_console_puts(", tick=");
    armv7a_platform_early_console_puts(observation.tick_ready ? "yes" : "no");
    armv7a_platform_early_console_puts(", idle=");
    armv7a_platform_early_console_puts(observation.idle_ready ? "yes" : "no");
    armv7a_platform_early_console_puts(", worker=");
    armv7a_platform_early_console_puts(observation.worker_ready ? "yes" : "no");
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_live_ready(observation) ? "yes" : "no");
    armv7a_platform_early_console_puts(", resumes=");
    armv7a_diag_put_dec(observation.worker_resumes);
    armv7a_platform_early_console_puts(", idle-runs=");
    armv7a_diag_put_dec(observation.idle_runs);
    armv7a_platform_early_console_puts(", wake-due=0x");
    armv7a_diag_put_hex64(observation.wake_due, 16);
    armv7a_platform_early_console_puts(", tick-now=0x");
    armv7a_diag_put_hex64(observation.last_tick_now, 16);
    armv7a_platform_early_console_puts("\r\n");

    if (!armv7a_runtime_live_ready(observation)) {
        armv7a_platform_early_console_puts(
            "ARMv7-A runtime live debug, irq-yield=");
        armv7a_platform_early_console_puts(
            observation.irq_enabled_after_yield ? "enabled" : "masked");
        armv7a_platform_early_console_puts(", irq-sleep=");
        armv7a_platform_early_console_puts(
            observation.irq_enabled_after_sleep ? "enabled" : "masked");
        armv7a_platform_early_console_puts(", arm=");
        armv7a_platform_early_console_puts(
            observation.timer_arm_seen ? "yes" : "no");
        armv7a_platform_early_console_puts(", armed=");
        armv7a_platform_early_console_puts(
            observation.timer_armed ? "yes" : "no");
        armv7a_platform_early_console_puts(", irq=");
        armv7a_platform_early_console_puts(
            observation.timer_irq_seen ? "yes" : "no");
        armv7a_platform_early_console_puts(", line=");
        armv7a_platform_early_console_puts(
            observation.timer_line_enabled ? "enabled" : "disabled");
        armv7a_platform_early_console_puts("/");
        armv7a_platform_early_console_puts(
            observation.timer_line_pending ? "pending" : "clear");
        armv7a_platform_early_console_puts("/");
        armv7a_platform_early_console_puts(
            observation.timer_line_active ? "active" : "inactive");
        armv7a_platform_early_console_puts(", ticks=0x");
        armv7a_diag_put_hex(observation.timer_arm_ticks);
        armv7a_platform_early_console_puts(", ctrl=0x");
        armv7a_diag_put_hex(observation.timer_ctrl);
        armv7a_platform_early_console_puts(", gicd=0x");
        armv7a_diag_put_hex(observation.gicd_ctlr);
        armv7a_platform_early_console_puts(", gicc=0x");
        armv7a_diag_put_hex(observation.gicc_ctlr);
        armv7a_platform_early_console_puts(", hppir=0x");
        armv7a_diag_put_hex(observation.hppir);
        armv7a_platform_early_console_puts(", trap=0x");
        armv7a_diag_put_hex64(observation.last_trap_value, 16);
        armv7a_platform_early_console_puts("\r\n");

        armv7a_platform_early_console_puts("ARMv7-A runtime live psr, ");
        armv7a_print_runtime_live_psr_sample(
            "enable", observation.cpsr_after_enable_irq);
        armv7a_platform_early_console_puts(", ");
        armv7a_print_runtime_live_psr_sample(
            "bootstrap-before", observation.cpsr_before_bootstrap);
        armv7a_platform_early_console_puts(", ");
        armv7a_print_runtime_live_psr_sample(
            "bootstrap-after", observation.cpsr_after_bootstrap);
        armv7a_platform_early_console_puts(", ");
        armv7a_print_runtime_live_psr_sample(
            "yield-before", observation.cpsr_before_yield);
        armv7a_platform_early_console_puts(", ");
        armv7a_print_runtime_live_psr_sample(
            "yield-after", observation.cpsr_after_yield);
        armv7a_platform_early_console_puts(", ");
        armv7a_print_runtime_live_psr_sample(
            "sleep-before", observation.cpsr_before_sleep);
        armv7a_platform_early_console_puts(", ");
        armv7a_print_runtime_live_psr_sample(
            "sleep-after", observation.cpsr_after_sleep);
        if (observation.wait_timeout_seen) {
            armv7a_platform_early_console_puts(", ");
            armv7a_print_runtime_live_psr_sample(
                "wait-timeout", observation.cpsr_wait_timeout);
        }
        armv7a_platform_early_console_puts("\r\n");

        armv7a_platform_early_console_puts(
            "ARMv7-A runtime live flow, yield-trap=");
        armv7a_platform_early_console_puts(
            observation.yield_trap_ok ? "yes" : "no");
        armv7a_platform_early_console_puts(", sleep-trap=");
        armv7a_platform_early_console_puts(
            observation.sleep_trap_ok ? "yes" : "no");
        armv7a_platform_early_console_puts(", rt-worker=");
        armv7a_platform_early_console_puts(
            observation.runtime_trace_worker_bootstrap ? "yes" : "no");
        armv7a_platform_early_console_puts(", rt-yield=");
        armv7a_platform_early_console_puts(
            observation.runtime_trace_yield ? "yes" : "no");
        armv7a_platform_early_console_puts(", rt-sleep=");
        armv7a_platform_early_console_puts(
            observation.runtime_trace_sleep ? "yes" : "no");
        armv7a_platform_early_console_puts(", rt-tick=");
        armv7a_platform_early_console_puts(
            observation.runtime_trace_tick ? "yes" : "no");
        armv7a_platform_early_console_puts(", rt-idle=");
        armv7a_platform_early_console_puts(
            observation.runtime_trace_idle ? "yes" : "no");
        armv7a_platform_early_console_puts(", trap-yield=");
        armv7a_platform_early_console_puts(
            observation.trap_trace_yield ? "yes" : "no");
        armv7a_platform_early_console_puts(", trap-sleep=");
        armv7a_platform_early_console_puts(
            observation.trap_trace_sleep ? "yes" : "no");
        armv7a_platform_early_console_puts("\r\n");

        armv7a_platform_early_console_puts(
            "ARMv7-A runtime live svc, yield-origin=");
        if (observation.yield_frame_sampled) {
            armv7a_platform_early_console_puts("0x");
            armv7a_diag_put_hex(observation.yield_origin_psr);
            armv7a_platform_early_console_puts(", yield-handler=0x");
            armv7a_diag_put_hex(observation.yield_handler_psr);
        } else {
            armv7a_platform_early_console_puts("n/a, yield-handler=n/a");
        }
        armv7a_platform_early_console_puts(", sleep-origin=");
        if (observation.sleep_frame_sampled) {
            armv7a_platform_early_console_puts("0x");
            armv7a_diag_put_hex(observation.sleep_origin_psr);
            armv7a_platform_early_console_puts(", sleep-handler=0x");
            armv7a_diag_put_hex(observation.sleep_handler_psr);
        } else {
            armv7a_platform_early_console_puts("n/a, sleep-handler=n/a");
        }
        armv7a_platform_early_console_puts("\r\n");
    }
}
