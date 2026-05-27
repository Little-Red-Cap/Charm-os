#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

#include "targets/armv7a/common/armv7a_exception_contract.hpp"
#include "targets/armv7a/common/armv7a_psr_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_bridge_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_contract.hpp"

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.task_syscall_api;
import kernel.runtime_trap_ingress;
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
        bool task_syscall_valid{false};
        bool worker_bootstrapped{false};
        bool worker_waiting_isr{false};
        bool worker_deferred{false};
        bool worker_finished{false};
        bool idle_seen_after_finish{false};
        bool trap_ingress_writeback_seen{false};
        bool armv7a_yield_capture_ok{false};
        bool armv7a_sleep_capture_ok{false};
        bool armv7a_user_origin_sample_ok{false};
        bool armv7a_kernel_origin_sample_ok{false};
        bool armv7a_supervisor_origin_sample_ok{false};
        bool armv7a_invalid_mode_rejected{false};
        bool armv7a_invalid_mode_decode_failed{false};
        std::uint32_t idle_runs{0};
        std::uint32_t trap_ingress_writebacks{0};
        std::uint32_t worker_resumes{0};
        std::uint64_t wake_due{0};
        std::uint64_t last_trap_value{0};
        std::uint16_t last_generic_service_id{0};
        std::uint32_t last_armv7a_service_id{0};
        std::uint32_t last_origin_psr{0};
        std::uint64_t last_return_pc{0};
        kernel::TrapOrigin last_origin{kernel::TrapOrigin::kernel_thread};
        kernel::TrapError last_trap_error{kernel::TrapError::none};
    };

    struct Armv7aHostTrapFrame {
        Armv7aRuntimeTrapObservation observation{};
        std::uint64_t return_value{0};
        kernel::TrapDisposition disposition{kernel::TrapDisposition::rejected};
        kernel::TrapError error{kernel::TrapError::none};
        bool writeback_seen{false};
    };

    struct Armv7aTrapAdapterContext {
        SharedState* shared{nullptr};
    };

    using RuntimeCaller =
        kernel::RuntimeTrapIngressCaller<Armv7aHostTrapFrame,
                                         ManualTimeSource::Tick>;
    using RuntimeServices = kernel::RuntimeTrapServiceFacade<RuntimeCaller>;
    using TaskRuntime = kernel::TaskRuntimeApi<RuntimeServices>;
    using TaskSyscalls = kernel::TaskSyscallApi<TaskRuntime>;

    struct IdleContext {
        SharedState* shared{nullptr};
    };

    struct WorkerContext {
        SharedState* shared{nullptr};
        TaskSyscalls runtime{};
    };

    inline constexpr kernel::Priority kIdlePriority{0};
    inline constexpr kernel::Priority kWorkerPriority{1};
    inline constexpr std::uint32_t kUsrMode = 0x10u;
    inline constexpr std::uint32_t kIrqMode = 0x12u;
    inline constexpr std::uint32_t kSvcMode = 0x13u;
    inline constexpr std::uint32_t kSysMode = 0x1fu;

    [[nodiscard]] constexpr std::uint32_t event_id_value(
        kernel::EventId id) noexcept
    {
        return static_cast<std::uint32_t>(id);
    }

    kernel::Event make_trap_sleep_event(ManualTimeSource::Tick due) noexcept
    {
        return kernel::make_event(kernel::EventId::tick,
                                  static_cast<std::uint32_t>(due));
    }

    [[nodiscard]] bool map_armv7a_origin(
        std::uint32_t origin_psr,
        kernel::TrapOrigin& out) noexcept
    {
        switch (armv7a_psr_mode(origin_psr)) {
        case kUsrMode:
            out = kernel::TrapOrigin::user_task;
            return true;
        case kSvcMode:
            out = kernel::TrapOrigin::supervisor;
            return true;
        case kSysMode:
            out = kernel::TrapOrigin::kernel_thread;
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] Armv7aSvcObservation make_svc_observation(
        std::uint32_t immediate,
        std::uint32_t arg0,
        std::uint32_t arg1,
        std::uint32_t arg2,
        std::uint32_t arg3,
        std::uint32_t origin_psr,
        std::uint32_t return_pc) noexcept
    {
        return Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(
                origin_psr, kSvcMode, return_pc),
            .immediate = immediate,
            .arg0 = arg0,
            .arg1 = arg1,
            .arg2 = arg2,
            .arg3 = arg3,
            .arguments_sampled = true,
        };
    }

    [[nodiscard]] Armv7aRuntimeTrapObservation make_runtime_trap_observation(
        const Armv7aSvcObservation& svc) noexcept
    {
        return Armv7aRuntimeTrapObservation{
            .path = Armv7aRuntimeTrapPath::svc_immediate,
            .service_id = svc.immediate,
            .service_id_sampled = true,
            .arguments_sampled = svc.arguments_sampled,
            .svc = svc,
        };
    }

    [[nodiscard]] bool yield_request_matches_policy(
        const Armv7aRuntimeBridgeTrapRequest& request) noexcept
    {
        return request.event_id == event_id_value(kernel::EventId::user1) &&
               request.event_payload == 1u;
    }

    [[nodiscard]] bool sleep_request_matches_policy(
        const Armv7aRuntimeBridgeTrapRequest& request) noexcept
    {
        return request.event_id == event_id_value(kernel::EventId::tick) &&
               request.due == static_cast<std::uint64_t>(request.event_payload);
    }

    bool capture_armv7a_host_trap_frame(void* ctx,
                                        const Armv7aHostTrapFrame& frame,
                                        kernel::TrapFrameView& out) noexcept
    {
        auto* adapter = static_cast<Armv7aTrapAdapterContext*>(ctx);
        auto* shared = adapter != nullptr ? adapter->shared : nullptr;
        const auto& observation = frame.observation;

        if (!armv7a_runtime_trap_ready(observation)) {
            return false;
        }

        kernel::TrapOrigin origin{};
        if (!map_armv7a_origin(observation.svc.entry.origin_psr, origin)) {
            return false;
        }

        const auto request = armv7a_decode_runtime_bridge_trap(observation.svc);
        switch (request.kind) {
        case Armv7aRuntimeBridgeTrapKind::yield_current:
            if (!armv7a_runtime_bridge_yield_request_ready(request) ||
                !yield_request_matches_policy(request)) {
                return false;
            }
            out = kernel::TrapFrameView{
                .service_id = static_cast<std::uint16_t>(
                    kernel::TrapService::yield_current),
                .arg0 = request.event_id,
                .arg1 = request.event_payload,
                .return_pc = observation.svc.entry.return_pc,
                .status = observation.svc.entry.origin_psr,
                .origin = origin,
            };
            if (shared != nullptr) {
                shared->armv7a_yield_capture_ok = true;
                shared->last_generic_service_id = out.service_id;
                shared->last_armv7a_service_id = observation.service_id;
                shared->last_origin_psr = observation.svc.entry.origin_psr;
                shared->last_return_pc = out.return_pc;
                shared->last_origin = origin;
            }
            return true;
        case Armv7aRuntimeBridgeTrapKind::sleep_current_until:
            if (!armv7a_runtime_bridge_sleep_request_ready(request) ||
                !sleep_request_matches_policy(request)) {
                return false;
            }
            out = kernel::TrapFrameView{
                .service_id = static_cast<std::uint16_t>(
                    kernel::TrapService::sleep_until),
                .arg0 = request.due,
                .arg1 = request.event_id,
                .arg2 = request.event_payload,
                .return_pc = observation.svc.entry.return_pc,
                .status = observation.svc.entry.origin_psr,
                .origin = origin,
            };
            if (shared != nullptr) {
                shared->armv7a_sleep_capture_ok = true;
                shared->last_generic_service_id = out.service_id;
                shared->last_armv7a_service_id = observation.service_id;
                shared->last_origin_psr = observation.svc.entry.origin_psr;
                shared->last_return_pc = out.return_pc;
                shared->last_origin = origin;
            }
            return true;
        case Armv7aRuntimeBridgeTrapKind::none:
        default:
            return false;
        }
    }

    bool apply_armv7a_host_result(void* ctx,
                                  Armv7aHostTrapFrame& frame,
                                  const kernel::TrapResult& result) noexcept
    {
        frame.return_value = result.value;
        frame.disposition = result.disposition;
        frame.error = result.error;
        frame.writeback_seen = true;

        auto* adapter = static_cast<Armv7aTrapAdapterContext*>(ctx);
        if (adapter != nullptr && adapter->shared != nullptr) {
            adapter->shared->trap_ingress_writeback_seen = true;
            ++adapter->shared->trap_ingress_writebacks;
            adapter->shared->last_trap_value = result.value;
            adapter->shared->last_trap_error = result.error;
        }

        return true;
    }

    bool make_armv7a_yield_trap_frame(void*,
                                      kernel::TrapYieldCurrentView,
                                      Armv7aHostTrapFrame& out) noexcept
    {
        out = Armv7aHostTrapFrame{
            .observation = make_runtime_trap_observation(make_svc_observation(
                kArmv7aRuntimeBridgeYieldServiceId,
                event_id_value(kernel::EventId::user1),
                1u,
                0u,
                0u,
                kSysMode,
                static_cast<std::uint32_t>(0x8100u + ManualTimeSource::now()))),
        };
        return true;
    }

    bool make_armv7a_sleep_trap_frame(
        void*,
        kernel::TrapSleepUntilView<ManualTimeSource::Tick> sleep,
        Armv7aHostTrapFrame& out) noexcept
    {
        const auto due = sleep.due;
        const auto due_lo = static_cast<std::uint32_t>(due & 0xFFFF'FFFFull);
        const auto due_hi =
            static_cast<std::uint32_t>((due >> 32u) & 0xFFFF'FFFFull);
        out = Armv7aHostTrapFrame{
            .observation = make_runtime_trap_observation(make_svc_observation(
                kArmv7aRuntimeBridgeSleepServiceId,
                due_lo,
                due_hi,
                event_id_value(kernel::EventId::tick),
                static_cast<std::uint32_t>(due),
                kUsrMode,
                static_cast<std::uint32_t>(0x8200u + ManualTimeSource::now()))),
        };
        return true;
    }

    bool armv7a_trap_result_ready(void*,
                                  const Armv7aHostTrapFrame& frame,
                                  const kernel::TrapResult& result) noexcept
    {
        return result.error != kernel::TrapError::none || frame.writeback_seen;
    }

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    void print_trap_projection_fields(
        const kernel::TrapSemanticProjection& projection,
        std::uint64_t result_value,
        bool include_ok,
        bool ok)
    {
        for (std::size_t i = 0; i < projection.field_count; ++i) {
            std::printf("%s=%llu ",
                        projection.fields[i].name,
                        static_cast<unsigned long long>(
                            projection.fields[i].value));
        }
        std::printf("%s=%llu",
                    projection.result_name,
                    static_cast<unsigned long long>(result_value));
        if (include_ok) {
            std::printf(" ok=%d", ok ? 1 : 0);
        }
        std::printf("\n");
    }

    template <typename Tick>
    void print_trap_trace_record(
        const kernel::RuntimeTrapTraceEvent<Tick>& record)
    {
        const auto projection = kernel::trap_semantic_projection(record);
        const auto& descriptor = projection.descriptor;
        std::printf("[trap-trace] t=%llu service=%s origin=%s ",
                    static_cast<unsigned long long>(record.stamp),
                    descriptor.service_name,
                    kernel::trap_origin_name(record.origin));
        if (record.task_valid) {
            std::printf("task=%llu ",
                        static_cast<unsigned long long>(record.task.value));
        } else {
            std::printf("task=- ");
        }
        std::printf("disp=%s err=%s view=%s args=%u supported=%d ",
                    kernel::trap_disposition_name(record.disposition),
                    kernel::trap_error_name(record.error),
                    kernel::trap_service_view_kind_name(
                        descriptor.view_kind),
                    static_cast<unsigned int>(descriptor.wire_argument_count),
                    descriptor.supported ? 1 : 0);
        print_trap_projection_fields(
            projection, record.value, false, false);
    }

    template <typename Tick>
    void print_trap_ingress_trace_record(
        const kernel::RuntimeTrapIngressTraceEvent<Tick>& record)
    {
        const auto projection = kernel::trap_semantic_projection(record);
        const auto& descriptor = projection.descriptor;
        std::printf("[trap-ingress-trace] t=%llu seq=%llu stage=%s service=%s origin=%s ",
                    static_cast<unsigned long long>(record.stamp),
                    static_cast<unsigned long long>(record.sequence),
                    kernel::trap_ingress_stage_name(record.stage),
                    descriptor.service_name,
                    kernel::trap_origin_name(record.origin));
        if (record.task_valid) {
            std::printf("task=%llu ",
                        static_cast<unsigned long long>(record.task.value));
        } else {
            std::printf("task=- ");
        }
        std::printf("disp=%s err=%s view=%s args=%u supported=%d ",
                    kernel::trap_disposition_name(record.disposition),
                    kernel::trap_error_name(record.error),
                    kernel::trap_service_view_kind_name(
                        descriptor.view_kind),
                    static_cast<unsigned int>(descriptor.wire_argument_count),
                    descriptor.supported ? 1 : 0);
        print_trap_projection_fields(projection, record.value, true, record.ok);
    }

    [[nodiscard]] bool probe_armv7a_origin_sample(
        std::uint32_t origin_psr,
        kernel::TrapOrigin expected_origin) noexcept
    {
        Armv7aHostTrapFrame frame{
            .observation = make_runtime_trap_observation(make_svc_observation(
                kArmv7aRuntimeBridgeYieldServiceId,
                event_id_value(kernel::EventId::user1),
                1u,
                0u,
                0u,
                origin_psr,
                static_cast<std::uint32_t>(0x9000u + origin_psr))),
        };
        kernel::TrapFrameView view{};
        return capture_armv7a_host_trap_frame(nullptr, frame, view) &&
               static_cast<kernel::TrapService>(view.service_id) ==
                   kernel::TrapService::yield_current &&
               view.origin == expected_origin &&
               view.return_pc == frame.observation.svc.entry.return_pc &&
               view.status == origin_psr;
    }

    [[nodiscard]] bool probe_armv7a_invalid_mode_rejected(
        std::uint32_t origin_psr) noexcept
    {
        Armv7aHostTrapFrame frame{
            .observation = make_runtime_trap_observation(make_svc_observation(
                kArmv7aRuntimeBridgeYieldServiceId,
                event_id_value(kernel::EventId::user1),
                1u,
                0u,
                0u,
                origin_psr,
                static_cast<std::uint32_t>(0x9100u + origin_psr))),
        };
        kernel::TrapFrameView view{};
        return !capture_armv7a_host_trap_frame(nullptr, frame, view);
    }

    template <typename IngressPort>
    [[nodiscard]] bool probe_armv7a_invalid_mode_decode_failed(
        const IngressPort& ingress,
        std::uint32_t origin_psr) noexcept
    {
        Armv7aHostTrapFrame frame{
            .observation = make_runtime_trap_observation(make_svc_observation(
                kArmv7aRuntimeBridgeYieldServiceId,
                event_id_value(kernel::EventId::user1),
                1u,
                0u,
                0u,
                origin_psr,
                static_cast<std::uint32_t>(0x9200u + origin_psr))),
        };
        const auto result = ingress.dispatch_frame(frame);
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::decode_failed) &&
               !frame.writeback_seen;
    }

    void idle_step(IdleContext& context,
                   kernel::ThreadControl&,
                   kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[idle] init\n");
            return;
        }

        if (event.id != kernel::EventId::user0 || context.shared == nullptr) {
            return;
        }

        ++context.shared->idle_runs;
        if (context.shared->worker_finished) {
            context.shared->idle_seen_after_finish = true;
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
            std::printf("[worker] bootstrap resume=%u\n",
                        context.shared->worker_resumes);
            const auto yielded = context.runtime.sys_yield();
            std::printf("[worker] yield=%d\n", yielded.ok() ? 1 : 0);
            return;
        }

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == 1u) {
            ++context.shared->worker_resumes;
            context.shared->worker_waiting_isr = true;
            std::printf("[worker] yielded resume=%u\n",
                        context.shared->worker_resumes);
            return;
        }

        if (event.id == kernel::EventId::user1 &&
            kernel::payload_u32(event) == 2u) {
            ++context.shared->worker_resumes;
            context.shared->worker_waiting_isr = false;
            context.shared->wake_due = ManualTimeSource::now() + 3u;
            std::printf("[worker] deferred resume=%u wake_due=%llu\n",
                        context.shared->worker_resumes,
                        static_cast<unsigned long long>(
                            context.shared->wake_due));
            const auto slept = context.runtime.sys_sleep_until(
                context.shared->wake_due);
            std::printf("[worker] sleep=%d\n", slept.ok() ? 1 : 0);
            return;
        }

        if (event.id == kernel::EventId::tick) {
            context.shared->worker_finished = true;
            std::printf("[worker] wake now=%llu payload=%u\n",
                        static_cast<unsigned long long>(ManualTimeSource::now()),
                        kernel::payload_u32(event));
            control.finish();
        }
    }

    using IdleTask = kernel::ThreadTask<IdleContext, &idle_step, kIdlePriority>;
    using WorkerTask =
        kernel::ThreadTask<WorkerContext, &worker_step, kWorkerPriority>;
}

int main()
{
    using Registry = kernel::TaskRegistry<demo::IdleTask, demo::WorkerTask>;
    using RuntimeTrace =
        kernel::RuntimeTraceBuffer<demo::ManualTimeSource::Tick, 32>;
    using TrapTrace =
        kernel::RuntimeTrapTraceBuffer<demo::ManualTimeSource::Tick, 16>;
    using TrapIngressTrace =
        kernel::RuntimeTrapIngressTraceBuffer<demo::ManualTimeSource::Tick, 16>;

    demo::ManualTimeSource::reset();

    Registry registry{};
    demo::Caps caps{};
    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    using RunningScheduler = decltype(running);
    RuntimeTrace runtime_trace{};
    TrapTrace trap_trace{};
    TrapIngressTrace trap_ingress_trace{};
    demo::SharedState shared{};
    shared.armv7a_user_origin_sample_ok =
        demo::probe_armv7a_origin_sample(demo::kUsrMode,
                                         kernel::TrapOrigin::user_task);
    shared.armv7a_kernel_origin_sample_ok =
        demo::probe_armv7a_origin_sample(demo::kSysMode,
                                         kernel::TrapOrigin::kernel_thread);
    shared.armv7a_supervisor_origin_sample_ok =
        demo::probe_armv7a_origin_sample(demo::kSvcMode,
                                         kernel::TrapOrigin::supervisor);
    shared.armv7a_invalid_mode_rejected =
        demo::probe_armv7a_invalid_mode_rejected(demo::kIrqMode);

    const auto idle_id = Registry::id_of<demo::IdleTask>();
    const auto worker_id = Registry::id_of<demo::WorkerTask>();
    kernel::RuntimeBridge<RunningScheduler, RuntimeTrace> runtime{
        running,
        idle_id,
        kernel::make_event(kernel::EventId::user0),
        &runtime_trace,
    };
    kernel::RuntimeTrapBridge<decltype(runtime), TrapTrace> trap{
        runtime,
        kernel::RuntimeTrapPolicy<demo::ManualTimeSource::Tick>{
            .yield_resume_event = kernel::make_event(
                kernel::EventId::user1,
                static_cast<std::uint32_t>(1u)),
            .sleep_resume_event = kernel::make_event(kernel::EventId::tick),
            .sleep_event_factory = &demo::make_trap_sleep_event,
        },
        &trap_trace,
    };
    demo::Armv7aTrapAdapterContext trap_adapter_ctx{
        .shared = &shared,
    };
    kernel::RuntimeTrapIngress<decltype(trap),
                               demo::Armv7aHostTrapFrame,
                               TrapIngressTrace>
        trap_ingress{
            trap,
            kernel::RuntimeTrapFrameAdapter<demo::Armv7aHostTrapFrame>{
                .ctx = &trap_adapter_ctx,
                .capture = &demo::capture_armv7a_host_trap_frame,
                .apply_result = &demo::apply_armv7a_host_result,
            },
            &trap_ingress_trace,
        };
    auto trap_ingress_port = kernel::make_runtime_trap_ingress_port(trap_ingress);

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& worker = registry.get<demo::WorkerTask>();
    worker.context.shared = &shared;
    worker.context.runtime = kernel::make_task_syscall_api(
        kernel::make_task_runtime_api(
            kernel::make_runtime_trap_service_facade(
                kernel::make_runtime_trap_ingress_caller(
                    trap_ingress_port,
                    kernel::RuntimeTrapCallFrameAdapter<
                        demo::Armv7aHostTrapFrame,
                        demo::ManualTimeSource::Tick>{
                        .make_yield_frame =
                            &demo::make_armv7a_yield_trap_frame,
                        .make_sleep_frame =
                            &demo::make_armv7a_sleep_trap_frame,
                        .result_ready = &demo::armv7a_trap_result_ready,
                    }))));
    shared.task_syscall_valid = worker.context.runtime.valid();
    shared.armv7a_invalid_mode_decode_failed =
        demo::probe_armv7a_invalid_mode_decode_failed(trap_ingress_port,
                                                      demo::kIrqMode);

    while (running.run_once()) {
    }

    shared.worker_bootstrapped = runtime.bootstrap_worker(
        worker_id,
        kernel::make_event(kernel::EventId::user0,
                           static_cast<std::uint32_t>(1u)));

    std::size_t loops = 0;
    while ((!shared.worker_finished || !shared.idle_seen_after_finish) &&
           loops < 64u) {
        const auto now = demo::ManualTimeSource::now();
        if (shared.worker_waiting_isr && !shared.worker_deferred) {
            shared.worker_deferred = runtime.defer_from_isr(
                worker_id,
                kernel::make_event(kernel::EventId::user1,
                                   static_cast<std::uint32_t>(2u)));
        }

        (void)runtime.run_once_or_idle(now);

        demo::ManualTimeSource::advance(1u);
        ++loops;
    }

    char snapshot[512]{};
    char scheduler_trace[4096]{};
    char ingress_witness_json[512]{};
    (void)kernel::format_snapshot(running, snapshot, sizeof(snapshot));
    (void)kernel::format_trace_csv(running, scheduler_trace, sizeof(scheduler_trace));
    const auto ingress_forensics =
        kernel::trap_ingress_forensic_snapshot(trap_ingress_trace);
    const auto ingress_witness =
        kernel::trap_ingress_forensic_witness(trap_ingress_trace);
    (void)kernel::format_trap_ingress_forensic_witness_json(
        ingress_witness_json, sizeof(ingress_witness_json), ingress_witness);
    const bool ingress_forensics_ok =
        ingress_forensics.has_terminal && ingress_forensics.has_decode &&
        ingress_forensics.has_dispatch && ingress_forensics.has_writeback &&
        ingress_forensics.ok() &&
        ingress_forensics.terminal_stage() ==
            kernel::TrapIngressStage::writeback &&
        ingress_forensics.terminal.service ==
            kernel::TrapService::sleep_until &&
        ingress_forensics.terminal.origin ==
            kernel::TrapOrigin::user_task &&
        ingress_forensics.has_last_failure &&
        ingress_forensics.last_failure.stage ==
            kernel::TrapIngressStage::decode &&
        ingress_forensics.last_failure.error ==
            kernel::TrapError::decode_failed &&
        ingress_forensics.last_failure.sequence !=
            ingress_forensics.sequence();
    const bool ingress_witness_ok =
        kernel::trap_ingress_forensic_witness_ready(ingress_witness) &&
        ingress_witness.ok() &&
        ingress_witness.sequence == ingress_forensics.sequence() &&
        ingress_witness.terminal_service == kernel::TrapService::sleep_until &&
        ingress_witness.terminal_origin == kernel::TrapOrigin::user_task &&
        ingress_witness.last_failure_error ==
            kernel::TrapError::decode_failed &&
        ingress_witness.last_failure_is_prior_attempt() &&
        ingress_witness_json[0] != '\0';

    const bool ok = shared.task_syscall_valid &&
                    shared.worker_bootstrapped && shared.worker_deferred &&
                    shared.worker_finished && shared.idle_runs != 0u &&
                    shared.idle_seen_after_finish &&
                    shared.trap_ingress_writeback_seen &&
                    shared.trap_ingress_writebacks >= 2u &&
                    shared.armv7a_yield_capture_ok &&
                    shared.armv7a_sleep_capture_ok &&
                    shared.armv7a_user_origin_sample_ok &&
                    shared.armv7a_kernel_origin_sample_ok &&
                    shared.armv7a_supervisor_origin_sample_ok &&
                    shared.armv7a_invalid_mode_rejected &&
                    shared.armv7a_invalid_mode_decode_failed &&
                    ingress_forensics_ok &&
                    ingress_witness_ok &&
                    shared.last_trap_error == kernel::TrapError::none &&
                    shared.last_armv7a_service_id ==
                        kArmv7aRuntimeBridgeSleepServiceId &&
                    static_cast<kernel::TrapService>(
                        shared.last_generic_service_id) ==
                        kernel::TrapService::sleep_until &&
                    shared.last_origin == kernel::TrapOrigin::user_task &&
                    shared.last_return_pc != 0u;

    std::printf(
        "[armv7a-runtime-demo] ok=%d syscall=%d bootstrapped=%d deferred=%d worker_finished=%d idle_runs=%u writebacks=%u loops=%llu\n",
        ok ? 1 : 0,
        shared.task_syscall_valid ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        shared.worker_deferred ? 1 : 0,
        shared.worker_finished ? 1 : 0,
        shared.idle_runs,
        shared.trap_ingress_writebacks,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[armv7a-origin-samples] usr=%d sys=%d svc=%d invalid_irq=%d invalid_irq_decode=%d last_mode=%s last_service=0x%02x last_generic=%s return_pc=0x%llx err=%s\n",
        shared.armv7a_user_origin_sample_ok ? 1 : 0,
        shared.armv7a_kernel_origin_sample_ok ? 1 : 0,
        shared.armv7a_supervisor_origin_sample_ok ? 1 : 0,
        shared.armv7a_invalid_mode_rejected ? 1 : 0,
        shared.armv7a_invalid_mode_decode_failed ? 1 : 0,
        armv7a_mode_name(shared.last_origin_psr),
        static_cast<unsigned int>(shared.last_armv7a_service_id),
        kernel::trap_service_name(static_cast<kernel::TrapService>(
            shared.last_generic_service_id)),
        static_cast<unsigned long long>(shared.last_return_pc),
        kernel::trap_error_name(shared.last_trap_error));
    std::printf("[armv7a-runtime.snapshot] %s\n", snapshot);
    std::printf(
        "[trap-ingress.forensics] ok=%d seq=%llu terminal=%s/%s/%s last_failure=%s/%s/%s\n",
        ingress_forensics_ok ? 1 : 0,
        static_cast<unsigned long long>(ingress_forensics.sequence()),
        kernel::trap_ingress_stage_name(ingress_forensics.terminal_stage()),
        kernel::trap_service_name(ingress_forensics.terminal.service),
        kernel::trap_origin_name(ingress_forensics.terminal.origin),
        ingress_forensics.has_last_failure
            ? kernel::trap_ingress_stage_name(
                  ingress_forensics.last_failure.stage)
            : "none",
        ingress_forensics.has_last_failure
            ? kernel::trap_error_name(ingress_forensics.last_failure.error)
            : "none",
        ingress_forensics.has_last_failure
            ? kernel::trap_service_name(
                  ingress_forensics.last_failure.service)
            : "invalid");
    std::printf("[trap-ingress.witness] %s\n", ingress_witness_json);

    for (std::size_t i = 0; i < runtime_trace.size(); ++i) {
        const auto* record = runtime_trace.at(i);
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
    }

    for (std::size_t i = 0; i < trap_trace.size(); ++i) {
        const auto* record = trap_trace.at(i);
        if (record == nullptr) {
            continue;
        }

        demo::print_trap_trace_record(*record);
    }

    for (std::size_t i = 0; i < trap_ingress_trace.size(); ++i) {
        const auto* record = trap_ingress_trace.at(i);
        if (record == nullptr) {
            continue;
        }

        demo::print_trap_ingress_trace_record(*record);
    }

    std::printf("[scheduler-trace]\n%s", scheduler_trace);
    return ok ? 0 : 1;
}
