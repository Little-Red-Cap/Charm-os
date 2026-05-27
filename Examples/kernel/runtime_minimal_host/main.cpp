#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.context;
import kernel.eda;
import kernel.evt;
import kernel.task_syscall_api;
import kernel.runtime_trap_ingress;
import kernel.scheduler;
import kernel.scheduler_export;
import kernel.thread;
import out.format;
import out.sink;

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
        bool capability_call_ok{false};
        bool capability_output_seen{false};
        bool debug_write_ok{false};
        bool debug_output_seen{false};
        bool negative_no_current_ok{false};
        bool negative_invalid_origin_ok{false};
        bool negative_capability_invalid_ok{false};
        bool negative_unsupported_ok{false};
        bool negative_decode_failed_ok{false};
        bool negative_writeback_failed_ok{false};
        bool negative_unbound_bridge_ok{false};
        bool negative_unbound_adapter_ok{false};
        bool trap_catalog_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t trap_ingress_writebacks{0};
        std::uint32_t worker_resumes{0};
        std::uint64_t wake_due{0};
        std::uint64_t last_capability_id{0};
        std::uint64_t last_capability_operation{0};
        std::uint64_t last_capability_payload{0};
        std::uint64_t last_capability_result{0};
        std::size_t capability_output_bytes{0};
        std::uint64_t last_debug_value{0};
        std::size_t debug_output_bytes{0};
        std::uint64_t last_trap_value{0};
        kernel::TrapError last_trap_error{kernel::TrapError::none};
    };

    struct SyntheticTrapFrame {
        std::uint16_t service_id{0};
        std::uint64_t arg0{0};
        std::uint64_t arg1{0};
        std::uint64_t arg2{0};
        std::uint64_t arg3{0};
        std::uint64_t return_pc{0};
        std::uint64_t stack_pointer{0};
        std::uint64_t status{0};
        kernel::TrapOrigin origin{kernel::TrapOrigin::kernel_thread};
        kernel::TaskId task{};
        bool task_valid{false};
        std::uint64_t return_value{0};
        kernel::TrapDisposition disposition{kernel::TrapDisposition::rejected};
        kernel::TrapError error{kernel::TrapError::none};
        bool writeback_seen{false};
    };

    struct SyntheticTrapAdapterContext {
        SharedState* shared{nullptr};
    };

    struct DebugWriteContext {
        SharedState* shared{nullptr};
        out::buffer_sink<256>* sink{nullptr};
    };

    struct CapabilityCallContext {
        SharedState* shared{nullptr};
        out::buffer_sink<256>* sink{nullptr};
    };

    using RuntimeCaller = kernel::RuntimeTrapIngressCaller<
        SyntheticTrapFrame,
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

    kernel::Event make_trap_sleep_event(ManualTimeSource::Tick due) noexcept
    {
        return kernel::make_event(kernel::EventId::tick,
                                  static_cast<std::uint32_t>(due));
    }

    bool capture_synthetic_trap_frame(void*,
                                      const SyntheticTrapFrame& frame,
                                      kernel::TrapFrameView& out) noexcept
    {
        out = kernel::TrapFrameView{
            .service_id = frame.service_id,
            .arg0 = frame.arg0,
            .arg1 = frame.arg1,
            .arg2 = frame.arg2,
            .arg3 = frame.arg3,
            .return_pc = frame.return_pc,
            .stack_pointer = frame.stack_pointer,
            .status = frame.status,
            .origin = frame.origin,
            .task = frame.task,
            .task_valid = frame.task_valid,
        };
        return true;
    }

    bool apply_synthetic_trap_result(void* ctx,
                                     SyntheticTrapFrame& frame,
                                     const kernel::TrapResult& result) noexcept
    {
        frame.return_value = result.value;
        frame.disposition = result.disposition;
        frame.error = result.error;
        frame.writeback_seen = true;

        auto* adapter = static_cast<SyntheticTrapAdapterContext*>(ctx);
        if (adapter != nullptr && adapter->shared != nullptr) {
            adapter->shared->trap_ingress_writeback_seen = true;
            ++adapter->shared->trap_ingress_writebacks;
            adapter->shared->last_trap_value = result.value;
            adapter->shared->last_trap_error = result.error;
        }

        return true;
    }

    bool make_yield_synthetic_trap_frame(void*,
                                         kernel::TrapYieldCurrentView,
                                         SyntheticTrapFrame& out) noexcept
    {
        out = SyntheticTrapFrame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::yield_current),
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        return true;
    }

    bool make_sleep_synthetic_trap_frame(
        void*,
        kernel::TrapSleepUntilView<ManualTimeSource::Tick> sleep,
        SyntheticTrapFrame& out) noexcept
    {
        out = SyntheticTrapFrame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::sleep_until),
            .arg0 = sleep.due,
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        return true;
    }

    bool make_debug_write_synthetic_trap_frame(
        void*,
        kernel::TrapDebugWriteView write,
        SyntheticTrapFrame& out) noexcept
    {
        out = SyntheticTrapFrame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::debug_write),
            .arg0 = write.value,
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        return true;
    }

    bool make_capability_call_synthetic_trap_frame(
        void*,
        kernel::TrapCapabilityCallView capability,
        SyntheticTrapFrame& out) noexcept
    {
        out = SyntheticTrapFrame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::capability_call),
            .arg0 = capability.capability_id,
            .arg1 = capability.operation,
            .arg2 = capability.payload,
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        return true;
    }

    bool synthetic_trap_result_ready(void*,
                                     const SyntheticTrapFrame& frame,
                                     const kernel::TrapResult& result) noexcept
    {
        return result.error != kernel::TrapError::none || frame.writeback_seen;
    }

    kernel::TrapResult handle_debug_write(void* ctx,
                                          const kernel::TrapRequest& request) noexcept
    {
        auto* debug = static_cast<DebugWriteContext*>(ctx);
        if (debug == nullptr || debug->sink == nullptr) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::unsupported,
                .error = kernel::TrapError::unsupported_service,
                .value = 0,
            };
        }

        const auto write = kernel::trap_debug_write_view(request);
        auto written = out::vprint<"[debug-write] origin={} value={}\n">(
            *debug->sink,
            kernel::trap_origin_name(request.origin),
            write.value);
        if (!written) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::invalid_argument,
                .value = 0,
            };
        }

        if (debug->shared != nullptr) {
            debug->shared->debug_write_ok = true;
            debug->shared->last_debug_value = write.value;
            debug->shared->debug_output_bytes = *written;
        }

        return kernel::TrapResult{
            .disposition = kernel::TrapDisposition::handled,
            .error = kernel::TrapError::none,
            .value = static_cast<std::uint64_t>(*written),
        };
    }

    kernel::TrapResult handle_capability_call(
        void* ctx,
        const kernel::TrapRequest& request) noexcept
    {
        auto* capability = static_cast<CapabilityCallContext*>(ctx);
        if (capability == nullptr || capability->sink == nullptr) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::unsupported,
                .error = kernel::TrapError::unsupported_service,
                .value = 0,
            };
        }

        const auto call = kernel::trap_capability_call_view(request);
        constexpr std::uint64_t kExpectedCapabilityId = 7u;
        constexpr std::uint64_t kExpectedOperation = 2u;
        if (call.capability_id != kExpectedCapabilityId ||
            call.operation != kExpectedOperation) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::invalid_argument,
                .value = 0,
            };
        }

        const auto result_value =
            call.capability_id + call.operation + call.payload;
        auto written =
            out::vprint<"[capability-call] cap={} op={} payload={} result={}\n">(
                *capability->sink,
                call.capability_id,
                call.operation,
                call.payload,
                result_value);
        if (!written) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::invalid_argument,
                .value = 0,
            };
        }

        if (capability->shared != nullptr) {
            capability->shared->capability_call_ok = true;
            capability->shared->last_capability_id = call.capability_id;
            capability->shared->last_capability_operation = call.operation;
            capability->shared->last_capability_payload = call.payload;
            capability->shared->last_capability_result = result_value;
            capability->shared->capability_output_bytes = *written;
        }

        return kernel::TrapResult{
            .disposition = kernel::TrapDisposition::handled,
            .error = kernel::TrapError::none,
            .value = result_value,
        };
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

    [[nodiscard]] bool probe_trap_service_catalog() noexcept
    {
        const auto invalid = kernel::trap_service_catalog_entry(
            kernel::TrapService::invalid);
        const auto yield = kernel::trap_service_catalog_entry(
            kernel::TrapService::yield_current);
        const auto sleep = kernel::trap_service_catalog_entry(
            kernel::TrapService::sleep_until);
        const auto debug = kernel::trap_service_catalog_entry(
            kernel::TrapService::debug_write);
        const auto capability = kernel::trap_service_catalog_entry(
            kernel::TrapService::capability_call);
        const auto unknown = kernel::trap_service_catalog_entry(
            static_cast<kernel::TrapService>(0x99u));

        return !invalid.supported &&
               invalid.view_kind == kernel::TrapServiceViewKind::invalid &&
               invalid.wire_argument_count == 0u &&
               invalid.result_name == std::string_view{"value"} &&
               std::string_view{invalid.service_name} == "invalid" &&
               yield.supported &&
               yield.view_kind ==
                   kernel::TrapServiceViewKind::yield_current &&
               yield.wire_argument_count == 0u &&
               yield.result_name == std::string_view{"accepted"} &&
               sleep.supported &&
               sleep.view_kind == kernel::TrapServiceViewKind::sleep_until &&
               sleep.wire_argument_count == 1u &&
               sleep.wire_argument_names[0] == std::string_view{"due"} &&
               sleep.result_name == std::string_view{"due"} &&
               debug.supported &&
               debug.view_kind == kernel::TrapServiceViewKind::debug_write &&
               debug.wire_argument_count == 1u &&
               debug.wire_argument_names[0] == std::string_view{"value"} &&
               debug.result_name == std::string_view{"bytes-written"} &&
               capability.supported &&
               capability.view_kind ==
                   kernel::TrapServiceViewKind::capability_call &&
               capability.wire_argument_count == 3u &&
               capability.wire_argument_names[0] ==
                   std::string_view{"capability-id"} &&
               capability.wire_argument_names[1] ==
                   std::string_view{"operation"} &&
               capability.wire_argument_names[2] ==
                   std::string_view{"payload"} &&
               capability.result_name == std::string_view{"result"} &&
               !unknown.supported &&
               unknown.view_kind == kernel::TrapServiceViewKind::opaque &&
               unknown.result_name == std::string_view{"value"} &&
               std::string_view{unknown.service_name} == "unknown";
    }

    template <typename TrapPort>
    [[nodiscard]] bool probe_no_current_task(TrapPort trap_port) noexcept
    {
        kernel::clear_current();
        const auto result = trap_port.yield_current(
            kernel::TrapYieldCurrentView{});
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::no_current_task);
    }

    template <typename TrapPort>
    [[nodiscard]] bool probe_invalid_origin(TrapPort trap_port) noexcept
    {
        kernel::clear_current();
        const auto result = trap_port.yield_current(
            kernel::TrapYieldCurrentView{},
            kernel::TrapOrigin::isr);
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_origin);
    }

    template <typename TrapPort>
    [[nodiscard]] bool probe_unsupported_service(TrapPort trap_port,
                                                 kernel::TaskId task) noexcept
    {
        kernel::set_current(
            task,
            kernel::make_event(kernel::EventId::user0,
                               static_cast<std::uint32_t>(0x55u)));
        const auto result = trap_port.call(kernel::TrapRequest{
            .service = static_cast<kernel::TrapService>(0x99u),
            .arg0 = static_cast<std::uint64_t>(0x1234u),
            .origin = kernel::TrapOrigin::kernel_thread,
        });
        kernel::clear_current();
        return trap_result_matches(result,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service);
    }

    template <typename TrapPort>
    [[nodiscard]] bool probe_capability_invalid(TrapPort trap_port,
                                                kernel::TaskId task) noexcept
    {
        kernel::set_current(
            task,
            kernel::make_event(kernel::EventId::user0,
                               static_cast<std::uint32_t>(0x56u)));
        const auto result = trap_port.capability_call(
            kernel::TrapCapabilityCallView{
                .capability_id = 0x55u,
                .operation = 9u,
                .payload = 0x33u,
            });
        kernel::clear_current();
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument);
    }

    bool reject_synthetic_trap_frame(void*,
                                     const SyntheticTrapFrame&,
                                     kernel::TrapFrameView&) noexcept
    {
        return false;
    }

    bool ignore_synthetic_trap_result(void*,
                                      SyntheticTrapFrame&,
                                      const kernel::TrapResult&) noexcept
    {
        return true;
    }

    bool reject_synthetic_trap_result(void*,
                                      SyntheticTrapFrame&,
                                      const kernel::TrapResult&) noexcept
    {
        return false;
    }

    [[nodiscard]] bool probe_unbound_bridge() noexcept
    {
        const auto result =
            kernel::RuntimeTrapPort<ManualTimeSource::Tick>{}.yield_current(
                kernel::TrapYieldCurrentView{});
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge);
    }

    [[nodiscard]] bool probe_unbound_adapter() noexcept
    {
        SyntheticTrapFrame frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::yield_current),
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        const auto result =
            kernel::RuntimeTrapIngressPort<SyntheticTrapFrame>{}.dispatch_frame(
                frame);
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               !frame.writeback_seen;
    }

    template <typename TrapBridge, typename TraceBuffer>
    [[nodiscard]] bool probe_decode_failed(TrapBridge& trap,
                                           TraceBuffer* trace) noexcept
    {
        kernel::RuntimeTrapIngress<TrapBridge, SyntheticTrapFrame, TraceBuffer>
            rejecting_ingress{
                trap,
                kernel::RuntimeTrapFrameAdapter<SyntheticTrapFrame>{
                    .capture = &reject_synthetic_trap_frame,
                    .apply_result = &ignore_synthetic_trap_result,
                },
                trace,
            };
        SyntheticTrapFrame frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::yield_current),
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        const auto result = rejecting_ingress.dispatch(frame);
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::decode_failed) &&
               !frame.writeback_seen;
    }

    template <typename TrapBridge, typename TraceBuffer>
    [[nodiscard]] bool probe_writeback_failed(TrapBridge& trap,
                                              TraceBuffer* trace,
                                              kernel::TaskId task) noexcept
    {
        kernel::RuntimeTrapIngress<TrapBridge, SyntheticTrapFrame, TraceBuffer>
            writeback_failing_ingress{
                trap,
                kernel::RuntimeTrapFrameAdapter<SyntheticTrapFrame>{
                    .capture = &capture_synthetic_trap_frame,
                    .apply_result = &reject_synthetic_trap_result,
                },
                trace,
            };
        SyntheticTrapFrame frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::yield_current),
            .return_pc = ManualTimeSource::now(),
            .origin = kernel::TrapOrigin::kernel_thread,
        };
        kernel::set_current(
            task,
            kernel::make_event(kernel::EventId::user0,
                               static_cast<std::uint32_t>(0x57u)));
        const auto result = writeback_failing_ingress.dispatch(frame);
        kernel::clear_current();
        return trap_result_matches(result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::writeback_failed,
                                   1u) &&
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
            const auto called =
                context.runtime.sys_capability_call(7u, 2u, 33u);
            std::printf("[worker] capability=%d value=%llu\n",
                        called.ok() ? 1 : 0,
                        static_cast<unsigned long long>(called.value));
            const auto debugged = context.runtime.sys_debug_write(0xC0DEu);
            std::printf("[worker] debug=%d\n", debugged.ok() ? 1 : 0);
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
    out::buffer_sink<256> capability_sink{};
    out::buffer_sink<256> debug_sink{};
    demo::SharedState shared{};
    demo::CapabilityCallContext capability_call_ctx{
        .shared = &shared,
        .sink = &capability_sink,
    };
    demo::DebugWriteContext debug_write_ctx{
        .shared = &shared,
        .sink = &debug_sink,
    };
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
            .debug_write_ctx = &debug_write_ctx,
            .debug_write_fn = &demo::handle_debug_write,
            .capability_call_ctx = &capability_call_ctx,
            .capability_call_fn = &demo::handle_capability_call,
        },
        &trap_trace,
    };
    demo::SyntheticTrapAdapterContext trap_adapter_ctx{
        .shared = &shared,
    };
    kernel::RuntimeTrapIngress<decltype(trap),
                               demo::SyntheticTrapFrame,
                               TrapIngressTrace>
        trap_ingress{
            trap,
            kernel::RuntimeTrapFrameAdapter<demo::SyntheticTrapFrame>{
                .ctx = &trap_adapter_ctx,
                .capture = &demo::capture_synthetic_trap_frame,
                .apply_result = &demo::apply_synthetic_trap_result,
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
                        demo::SyntheticTrapFrame,
                        demo::ManualTimeSource::Tick>{
                        .make_yield_frame =
                            &demo::make_yield_synthetic_trap_frame,
                        .make_sleep_frame =
                            &demo::make_sleep_synthetic_trap_frame,
                        .make_debug_write_frame =
                            &demo::make_debug_write_synthetic_trap_frame,
                        .make_capability_call_frame =
                            &demo::make_capability_call_synthetic_trap_frame,
                        .result_ready = &demo::synthetic_trap_result_ready,
                    }))));
    shared.task_syscall_valid = worker.context.runtime.valid();

    while (running.run_once()) {
    }

    const auto trap_port = kernel::make_runtime_trap_port(trap);
    shared.trap_catalog_ok = demo::probe_trap_service_catalog();
    shared.negative_no_current_ok = demo::probe_no_current_task(trap_port);
    shared.negative_invalid_origin_ok = demo::probe_invalid_origin(trap_port);
    shared.negative_capability_invalid_ok =
        demo::probe_capability_invalid(trap_port, worker_id);
    shared.negative_unsupported_ok =
        demo::probe_unsupported_service(trap_port, worker_id);
    shared.negative_decode_failed_ok =
        demo::probe_decode_failed(trap, &trap_ingress_trace);
    shared.negative_writeback_failed_ok =
        demo::probe_writeback_failed(trap, &trap_ingress_trace, worker_id);
    shared.negative_unbound_bridge_ok = demo::probe_unbound_bridge();
    shared.negative_unbound_adapter_ok = demo::probe_unbound_adapter();

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
    const auto capability_view = capability_sink.view();
    const auto debug_view = debug_sink.view();
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
            kernel::TrapOrigin::kernel_thread &&
        ingress_forensics.has_last_failure &&
        ingress_forensics.last_failure.stage ==
            kernel::TrapIngressStage::writeback &&
        ingress_forensics.last_failure.error ==
            kernel::TrapError::writeback_failed &&
        ingress_forensics.last_failure.sequence !=
            ingress_forensics.sequence();
    const bool ingress_witness_ok =
        kernel::trap_ingress_forensic_witness_ready(ingress_witness) &&
        ingress_witness.ok() &&
        ingress_witness.sequence == ingress_forensics.sequence() &&
        ingress_witness.terminal_service == kernel::TrapService::sleep_until &&
        ingress_witness.terminal_origin == kernel::TrapOrigin::kernel_thread &&
        ingress_witness.last_failure_error ==
            kernel::TrapError::writeback_failed &&
        ingress_witness.last_failure_is_prior_attempt() &&
        ingress_witness_json[0] != '\0';
    shared.capability_output_seen =
        !capability_view.empty() && shared.last_capability_id == 7u &&
        shared.last_capability_operation == 2u &&
        shared.last_capability_payload == 33u &&
        shared.last_capability_result == 42u &&
        capability_view.find("cap=7 op=2 payload=33 result=42") !=
            std::string_view::npos;
    shared.debug_output_seen =
        !debug_view.empty() && shared.last_debug_value == 0xC0DEu &&
        debug_view.find("value=49374") != std::string_view::npos;

    const bool ok = shared.task_syscall_valid &&
                    shared.worker_bootstrapped && shared.worker_deferred &&
                    shared.worker_finished && shared.idle_runs != 0u &&
                    shared.idle_seen_after_finish &&
                    shared.trap_ingress_writeback_seen &&
                    shared.trap_ingress_writebacks >= 2u &&
                    shared.capability_call_ok && shared.capability_output_seen &&
                    shared.debug_write_ok && shared.debug_output_seen &&
                    shared.negative_no_current_ok &&
                    shared.negative_invalid_origin_ok &&
                    shared.negative_capability_invalid_ok &&
                    shared.negative_unsupported_ok &&
                    shared.negative_decode_failed_ok &&
                    shared.negative_writeback_failed_ok &&
                    shared.negative_unbound_bridge_ok &&
                    shared.negative_unbound_adapter_ok &&
                    ingress_forensics_ok &&
                    ingress_witness_ok &&
                    shared.trap_catalog_ok &&
                    shared.last_trap_error == kernel::TrapError::none;

    std::printf(
        "[runtime-demo] ok=%d syscall=%d bootstrapped=%d deferred=%d worker_finished=%d idle_runs=%u loops=%llu\n",
        ok ? 1 : 0,
        shared.task_syscall_valid ? 1 : 0,
        shared.worker_bootstrapped ? 1 : 0,
        shared.worker_deferred ? 1 : 0,
        shared.worker_finished ? 1 : 0,
        shared.idle_runs,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-negative] no_current=%d invalid_origin=%d capability_invalid=%d unsupported=%d decode_failed=%d writeback_failed=%d unbound_bridge=%d unbound_adapter=%d\n",
        shared.negative_no_current_ok ? 1 : 0,
        shared.negative_invalid_origin_ok ? 1 : 0,
        shared.negative_capability_invalid_ok ? 1 : 0,
        shared.negative_unsupported_ok ? 1 : 0,
        shared.negative_decode_failed_ok ? 1 : 0,
        shared.negative_writeback_failed_ok ? 1 : 0,
        shared.negative_unbound_bridge_ok ? 1 : 0,
        shared.negative_unbound_adapter_ok ? 1 : 0);
    const auto yield_catalog = kernel::trap_service_catalog_entry(
        kernel::TrapService::yield_current);
    const auto sleep_catalog = kernel::trap_service_catalog_entry(
        kernel::TrapService::sleep_until);
    const auto debug_catalog = kernel::trap_service_catalog_entry(
        kernel::TrapService::debug_write);
    const auto capability_catalog = kernel::trap_service_catalog_entry(
        kernel::TrapService::capability_call);
    std::printf(
        "[trap-catalog] ok=%d yield=%s/%u->%s sleep=%s/%u->%s debug=%s/%u->%s capability=%s/%u->%s\n",
        shared.trap_catalog_ok ? 1 : 0,
        kernel::trap_service_view_kind_name(yield_catalog.view_kind),
        static_cast<unsigned int>(yield_catalog.wire_argument_count),
        yield_catalog.result_name,
        kernel::trap_service_view_kind_name(sleep_catalog.view_kind),
        static_cast<unsigned int>(sleep_catalog.wire_argument_count),
        sleep_catalog.result_name,
        kernel::trap_service_view_kind_name(debug_catalog.view_kind),
        static_cast<unsigned int>(debug_catalog.wire_argument_count),
        debug_catalog.result_name,
        kernel::trap_service_view_kind_name(capability_catalog.view_kind),
        static_cast<unsigned int>(capability_catalog.wire_argument_count),
        capability_catalog.result_name);
    std::printf("[runtime-capability] handled=%d seen=%d bytes=%zu text=%.*s\n",
                shared.capability_call_ok ? 1 : 0,
                shared.capability_output_seen ? 1 : 0,
                shared.capability_output_bytes,
                static_cast<int>(capability_view.size()),
                capability_view.data());
    std::printf("[runtime-debug] handled=%d seen=%d bytes=%zu text=%.*s\n",
                shared.debug_write_ok ? 1 : 0,
                shared.debug_output_seen ? 1 : 0,
                shared.debug_output_bytes,
                static_cast<int>(debug_view.size()),
                debug_view.data());
    std::printf("[runtime-demo.snapshot] %s\n", snapshot);
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
