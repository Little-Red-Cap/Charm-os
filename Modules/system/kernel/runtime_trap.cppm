module;

#include <array>
#include <cstddef>
#include <cstdint>

export module kernel.runtime_trap;

import kernel.context;
import kernel.eda;
import kernel.evt;
export import kernel.runtime_bridge;
import util.core;

export namespace kernel {
    enum class TrapOrigin : util::u8 {
        kernel_thread = 0,
        user_task,
        supervisor,
        isr,
    };

    [[nodiscard]] constexpr const char* trap_origin_name(TrapOrigin origin) noexcept
    {
        switch (origin) {
        case TrapOrigin::kernel_thread:
            return "kernel-thread";
        case TrapOrigin::user_task:
            return "user-task";
        case TrapOrigin::supervisor:
            return "supervisor";
        case TrapOrigin::isr:
            return "isr";
        }
        return "unknown";
    }

    enum class TrapService : util::u16 {
        invalid = 0,
        yield_current = 1,
        sleep_until = 2,
        debug_write = 3,
        capability_call = 4,
    };

    enum class TrapServiceViewKind : util::u8 {
        invalid = 0,
        yield_current,
        sleep_until,
        debug_write,
        capability_call,
        opaque,
    };

    [[nodiscard]] constexpr const char* trap_service_view_kind_name(
        TrapServiceViewKind kind) noexcept
    {
        switch (kind) {
        case TrapServiceViewKind::invalid:
            return "invalid";
        case TrapServiceViewKind::yield_current:
            return "yield-current";
        case TrapServiceViewKind::sleep_until:
            return "sleep-until";
        case TrapServiceViewKind::debug_write:
            return "debug-write";
        case TrapServiceViewKind::capability_call:
            return "capability-call";
        case TrapServiceViewKind::opaque:
            return "opaque";
        }
        return "unknown";
    }

    struct TrapServiceCatalogEntry {
        TrapService service{TrapService::invalid};
        const char* service_name{"invalid"};
        TrapServiceViewKind view_kind{TrapServiceViewKind::invalid};
        util::u8 wire_argument_count{0};
        std::array<const char*, 4> wire_argument_names{};
        const char* result_name{"value"};
        bool supported{false};
    };

    [[nodiscard]] constexpr TrapServiceCatalogEntry trap_service_catalog_entry(
        TrapService service) noexcept
    {
        switch (service) {
        case TrapService::invalid:
            return TrapServiceCatalogEntry{
                .service = service,
                .service_name = "invalid",
                .view_kind = TrapServiceViewKind::invalid,
                .wire_argument_count = 0,
                .wire_argument_names = {},
                .result_name = "value",
                .supported = false,
            };
        case TrapService::yield_current:
            return TrapServiceCatalogEntry{
                .service = service,
                .service_name = "yield-current",
                .view_kind = TrapServiceViewKind::yield_current,
                .wire_argument_count = 0,
                .wire_argument_names = {},
                .result_name = "accepted",
                .supported = true,
            };
        case TrapService::sleep_until:
            return TrapServiceCatalogEntry{
                .service = service,
                .service_name = "sleep-until",
                .view_kind = TrapServiceViewKind::sleep_until,
                .wire_argument_count = 1,
                .wire_argument_names = {"due", nullptr, nullptr, nullptr},
                .result_name = "due",
                .supported = true,
            };
        case TrapService::debug_write:
            return TrapServiceCatalogEntry{
                .service = service,
                .service_name = "debug-write",
                .view_kind = TrapServiceViewKind::debug_write,
                .wire_argument_count = 1,
                .wire_argument_names = {"value", nullptr, nullptr, nullptr},
                .result_name = "bytes-written",
                .supported = true,
            };
        case TrapService::capability_call:
            return TrapServiceCatalogEntry{
                .service = service,
                .service_name = "capability-call",
                .view_kind = TrapServiceViewKind::capability_call,
                .wire_argument_count = 3,
                .wire_argument_names = {
                    "capability-id",
                    "operation",
                    "payload",
                    nullptr,
                },
                .result_name = "result",
                .supported = true,
            };
        }

        return TrapServiceCatalogEntry{
            .service = service,
            .service_name = "unknown",
            .view_kind = TrapServiceViewKind::opaque,
            .wire_argument_count = 0,
            .wire_argument_names = {},
            .result_name = "value",
            .supported = false,
        };
    }

    [[nodiscard]] constexpr const char* trap_service_name(
        TrapService service) noexcept
    {
        return trap_service_catalog_entry(service).service_name;
    }

    enum class TrapDisposition : util::u8 {
        handled = 0,
        rejected,
        unsupported,
    };

    [[nodiscard]] constexpr const char* trap_disposition_name(
        TrapDisposition disposition) noexcept
    {
        switch (disposition) {
        case TrapDisposition::handled:
            return "handled";
        case TrapDisposition::rejected:
            return "rejected";
        case TrapDisposition::unsupported:
            return "unsupported";
        }
        return "unknown";
    }

    enum class TrapError : util::u8 {
        none = 0,
        no_current_task,
        invalid_origin,
        invalid_argument,
        decode_failed,
        writeback_failed,
        unsupported_service,
        unbound_bridge,
        unbound_adapter,
    };

    [[nodiscard]] constexpr const char* trap_error_name(TrapError error) noexcept
    {
        switch (error) {
        case TrapError::none:
            return "none";
        case TrapError::no_current_task:
            return "no-current-task";
        case TrapError::invalid_origin:
            return "invalid-origin";
        case TrapError::invalid_argument:
            return "invalid-argument";
        case TrapError::decode_failed:
            return "decode-failed";
        case TrapError::writeback_failed:
            return "writeback-failed";
        case TrapError::unsupported_service:
            return "unsupported-service";
        case TrapError::unbound_bridge:
            return "unbound-bridge";
        case TrapError::unbound_adapter:
            return "unbound-adapter";
        }
        return "unknown";
    }

    struct TrapRequest {
        TrapService service{TrapService::invalid};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 return_pc{0};
        util::u64 stack_pointer{0};
        util::u64 status{0};
        TrapOrigin origin{TrapOrigin::kernel_thread};
        TaskId task{};
        bool task_valid{false};
    };

    struct TrapFrameView {
        util::u16 service_id{0};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 return_pc{0};
        util::u64 stack_pointer{0};
        util::u64 status{0};
        TrapOrigin origin{TrapOrigin::user_task};
        TaskId task{};
        bool task_valid{false};
    };

    struct TrapYieldCurrentView {
    };

    [[nodiscard]] constexpr TrapYieldCurrentView trap_yield_current_view(
        const TrapRequest&) noexcept
    {
        return TrapYieldCurrentView{};
    }

    template <typename Tick>
    struct TrapSleepUntilView {
        Tick due{};
    };

    template <typename Tick>
    [[nodiscard]] constexpr TrapSleepUntilView<Tick> trap_sleep_until_view(
        const TrapRequest& request) noexcept
    {
        return TrapSleepUntilView<Tick>{
            .due = static_cast<Tick>(request.arg0),
        };
    }

    struct TrapDebugWriteView {
        util::u64 value{0};
    };

    [[nodiscard]] constexpr TrapDebugWriteView trap_debug_write_view(
        const TrapRequest& request) noexcept
    {
        return TrapDebugWriteView{
            .value = request.arg0,
        };
    }

    struct TrapCapabilityCallView {
        util::u64 capability_id{0};
        util::u64 operation{0};
        util::u64 payload{0};
    };

    [[nodiscard]] constexpr TrapCapabilityCallView trap_capability_call_view(
        const TrapRequest& request) noexcept
    {
        return TrapCapabilityCallView{
            .capability_id = request.arg0,
            .operation = request.arg1,
            .payload = request.arg2,
        };
    }

    [[nodiscard]] inline TrapRequest trap_request_from_frame(
        const TrapFrameView& frame) noexcept
    {
        return TrapRequest{
            .service = static_cast<TrapService>(frame.service_id),
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
    }

    [[nodiscard]] constexpr TrapRequest make_debug_write_trap_request(
        TrapDebugWriteView write,
        TrapOrigin origin = TrapOrigin::kernel_thread) noexcept
    {
        return TrapRequest{
            .service = TrapService::debug_write,
            .arg0 = write.value,
            .origin = origin,
        };
    }

    [[nodiscard]] constexpr TrapRequest make_yield_current_trap_request(
        TrapYieldCurrentView,
        TrapOrigin origin = TrapOrigin::kernel_thread) noexcept
    {
        return TrapRequest{
            .service = TrapService::yield_current,
            .origin = origin,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapRequest make_sleep_until_trap_request(
        TrapSleepUntilView<Tick> sleep,
        TrapOrigin origin = TrapOrigin::kernel_thread) noexcept
    {
        return TrapRequest{
            .service = TrapService::sleep_until,
            .arg0 = static_cast<util::u64>(sleep.due),
            .origin = origin,
        };
    }

    [[nodiscard]] constexpr TrapRequest make_capability_call_trap_request(
        TrapCapabilityCallView capability,
        TrapOrigin origin = TrapOrigin::kernel_thread) noexcept
    {
        return TrapRequest{
            .service = TrapService::capability_call,
            .arg0 = capability.capability_id,
            .arg1 = capability.operation,
            .arg2 = capability.payload,
            .origin = origin,
        };
    }

    struct TrapResult {
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unsupported_service};
        util::u64 value{0};

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return disposition == TrapDisposition::handled &&
                   error == TrapError::none;
        }
    };

    struct TrapSemanticField {
        const char* name{"arg0"};
        util::u64 value{0};
    };

    struct TrapSemanticProjection {
        TrapServiceCatalogEntry descriptor{};
        std::array<TrapSemanticField, 4> fields{};
        util::u8 field_count{0};
        const char* result_name{"value"};
    };

    [[nodiscard]] constexpr TrapSemanticField trap_semantic_field(
        const char* name,
        util::u64 value) noexcept
    {
        return TrapSemanticField{
            .name = name,
            .value = value,
        };
    }

    [[nodiscard]] constexpr TrapSemanticProjection trap_semantic_projection(
        const TrapRequest& request) noexcept
    {
        const auto descriptor = trap_service_catalog_entry(request.service);
        switch (descriptor.view_kind) {
        case TrapServiceViewKind::yield_current:
            return TrapSemanticProjection{
                .descriptor = descriptor,
                .fields = {},
                .field_count = 0,
                .result_name = descriptor.result_name,
            };
        case TrapServiceViewKind::sleep_until:
            return TrapSemanticProjection{
                .descriptor = descriptor,
                .fields = {
                    trap_semantic_field(descriptor.wire_argument_names[0],
                                        request.arg0),
                },
                .field_count = 1,
                .result_name = descriptor.result_name,
            };
        case TrapServiceViewKind::debug_write:
            return TrapSemanticProjection{
                .descriptor = descriptor,
                .fields = {
                    trap_semantic_field(descriptor.wire_argument_names[0],
                                        request.arg0),
                },
                .field_count = 1,
                .result_name = descriptor.result_name,
            };
        case TrapServiceViewKind::capability_call:
            return TrapSemanticProjection{
                .descriptor = descriptor,
                .fields = {
                    trap_semantic_field(descriptor.wire_argument_names[0],
                                        request.arg0),
                    trap_semantic_field(descriptor.wire_argument_names[1],
                                        request.arg1),
                    trap_semantic_field(descriptor.wire_argument_names[2],
                                        request.arg2),
                },
                .field_count = 3,
                .result_name = descriptor.result_name,
            };
        case TrapServiceViewKind::invalid:
        case TrapServiceViewKind::opaque:
        default:
            return TrapSemanticProjection{
                .descriptor = descriptor,
                .fields = {
                    trap_semantic_field("arg0", request.arg0),
                    trap_semantic_field("arg1", request.arg1),
                    trap_semantic_field("arg2", request.arg2),
                    trap_semantic_field("arg3", request.arg3),
                },
                .field_count = 4,
                .result_name = descriptor.result_name,
            };
        }
    }

    template <typename Tick>
    struct RuntimeTrapTraceEvent {
        Tick stamp{};
        TrapService service{TrapService::invalid};
        TrapOrigin origin{TrapOrigin::kernel_thread};
        TaskId task{};
        bool task_valid{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unsupported_service};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 value{0};
    };

    template <typename Tick>
    [[nodiscard]] constexpr TrapRequest trap_request_from_trace_event(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return TrapRequest{
            .service = event.service,
            .arg0 = event.arg0,
            .arg1 = event.arg1,
            .arg2 = event.arg2,
            .arg3 = event.arg3,
            .origin = event.origin,
            .task = event.task,
            .task_valid = event.task_valid,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapYieldCurrentView trap_yield_current_view(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return trap_yield_current_view(trap_request_from_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapSleepUntilView<Tick> trap_sleep_until_view(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return trap_sleep_until_view<Tick>(trap_request_from_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapDebugWriteView trap_debug_write_view(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return trap_debug_write_view(trap_request_from_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapCapabilityCallView trap_capability_call_view(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return trap_capability_call_view(trap_request_from_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapSemanticProjection trap_semantic_projection(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return trap_semantic_projection(trap_request_from_trace_event(event));
    }

    template <typename Tick, std::size_t Capacity>
    class RuntimeTrapTraceBuffer {
    public:
        using tick_type = Tick;
        using value_type = RuntimeTrapTraceEvent<Tick>;

        static_assert(Capacity > 0);

        [[nodiscard]] bool push(const value_type& event) noexcept
        {
            events_[head_] = event;
            head_ = (head_ + 1u) % Capacity;
            if (size_ < Capacity) {
                ++size_;
            }
            return true;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] const value_type* at(std::size_t index) const noexcept
        {
            if (index >= size_) {
                return nullptr;
            }

            const auto first = (head_ + Capacity - size_) % Capacity;
            return &events_[(first + index) % Capacity];
        }

    private:
        std::array<value_type, Capacity> events_{};
        std::size_t head_{0};
        std::size_t size_{0};
    };

    template <typename Tick>
    struct RuntimeTrapPolicy {
        Event yield_resume_event{make_event(EventId::user0)};
        Event sleep_resume_event{make_event(EventId::tick)};
        Event (*sleep_event_factory)(Tick due) noexcept {nullptr};
        void* debug_write_ctx{nullptr};
        TrapResult (*debug_write_fn)(void* ctx,
                                     const TrapRequest& request) noexcept {
            nullptr
        };
        void* capability_call_ctx{nullptr};
        TrapResult (*capability_call_fn)(void* ctx,
                                         const TrapRequest& request) noexcept {
            nullptr
        };
    };

    template <typename RuntimeBridge,
              typename TraceBuffer =
                  RuntimeTrapTraceBuffer<typename RuntimeBridge::tick_type, 1>>
    class RuntimeTrapBridge {
    public:
        using runtime_type = RuntimeBridge;
        using tick_type = typename RuntimeBridge::tick_type;
        using trace_type = TraceBuffer;
        using policy_type = RuntimeTrapPolicy<tick_type>;

        RuntimeTrapBridge(RuntimeBridge& runtime,
                          policy_type policy = {},
                          TraceBuffer* trace = nullptr) noexcept
            : runtime_(&runtime), policy_(policy), trace_(trace)
        {
        }

        [[nodiscard]] RuntimeBridge& runtime() noexcept
        {
            return *runtime_;
        }

        [[nodiscard]] const RuntimeBridge& runtime() const noexcept
        {
            return *runtime_;
        }

        [[nodiscard]] const policy_type& policy() const noexcept
        {
            return policy_;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void bind_policy(policy_type policy) noexcept
        {
            policy_ = policy;
        }

        [[nodiscard]] TrapResult dispatch(const TrapRequest& request) noexcept
        {
            using time_source =
                typename RuntimeBridge::scheduler_type::TimeSource;
            const auto now = time_source::now();

            TaskId trace_task{};
            bool trace_task_valid = false;
            if (request.task_valid) {
                trace_task = request.task;
                trace_task_valid = true;
            } else if (has_current()) {
                trace_task = current_task();
                trace_task_valid = true;
            }

            TrapResult result{};
            switch (request.origin) {
            case TrapOrigin::kernel_thread:
            case TrapOrigin::user_task:
            case TrapOrigin::supervisor:
                break;
            case TrapOrigin::isr:
                result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::invalid_origin,
                    .value = 0,
                };
                trap_trace_push(now, request, trace_task, trace_task_valid, result);
                return result;
            }

            if (!has_current()) {
                result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::no_current_task,
                    .value = 0,
                };
                trap_trace_push(now, request, trace_task, trace_task_valid, result);
                return result;
            }

            switch (request.service) {
            case TrapService::yield_current: {
                (void)trap_yield_current_view(request);
                const auto ok =
                    runtime_->yield_current(policy_.yield_resume_event);
                result = TrapResult{
                    .disposition = ok ? TrapDisposition::handled
                                      : TrapDisposition::rejected,
                    .error = ok ? TrapError::none
                                : TrapError::invalid_argument,
                    .value = ok ? 1u : 0u,
                };
                trap_trace_push(now, request, current_task(), true, result);
                return result;
            }
            case TrapService::sleep_until: {
                const auto sleep = trap_sleep_until_view<tick_type>(request);
                const auto event = policy_.sleep_event_factory != nullptr
                    ? policy_.sleep_event_factory(sleep.due)
                    : policy_.sleep_resume_event;
                const auto ok =
                    runtime_->sleep_current_until(sleep.due, event);
                result = TrapResult{
                    .disposition = ok ? TrapDisposition::handled
                                      : TrapDisposition::rejected,
                    .error = ok ? TrapError::none
                                : TrapError::invalid_argument,
                    .value = ok ? static_cast<util::u64>(sleep.due) : 0u,
                };
                trap_trace_push(now, request, current_task(), true, result);
                return result;
            }
            case TrapService::debug_write:
                if (policy_.debug_write_fn != nullptr) {
                    result =
                        policy_.debug_write_fn(policy_.debug_write_ctx, request);
                    trap_trace_push(now, request, current_task(), true, result);
                    return result;
                }
                [[fallthrough]];
            case TrapService::capability_call:
                if (policy_.capability_call_fn != nullptr) {
                    result = policy_.capability_call_fn(
                        policy_.capability_call_ctx, request);
                    trap_trace_push(now, request, current_task(), true, result);
                    return result;
                }
                [[fallthrough]];
            case TrapService::invalid:
                result = TrapResult{
                    .disposition = TrapDisposition::unsupported,
                    .error = TrapError::unsupported_service,
                    .value = 0,
                };
                trap_trace_push(now, request, current_task(), true, result);
                return result;
            }

            result = TrapResult{
                .disposition = TrapDisposition::unsupported,
                .error = TrapError::unsupported_service,
                .value = 0,
            };
            trap_trace_push(now, request, current_task(), true, result);
            return result;
        }

        [[nodiscard]] TrapResult dispatch_frame(
            const TrapFrameView& frame) noexcept
        {
            return dispatch(trap_request_from_frame(frame));
        }

    private:
        void trap_trace_push(tick_type stamp,
                             const TrapRequest& request,
                             TaskId task,
                             bool task_valid,
                             const TrapResult& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            (void)trace_->push(typename TraceBuffer::value_type{
                .stamp = stamp,
                .service = request.service,
                .origin = request.origin,
                .task = task,
                .task_valid = task_valid,
                .disposition = result.disposition,
                .error = result.error,
                .arg0 = request.arg0,
                .arg1 = request.arg1,
                .arg2 = request.arg2,
                .arg3 = request.arg3,
                .value = result.value,
            });
        }

        RuntimeBridge* runtime_{nullptr};
        policy_type policy_{};
        TraceBuffer* trace_{nullptr};
    };

    template <typename Tick>
    struct RuntimeTrapPort {
        using tick_type = Tick;

        void* self{nullptr};
        TrapResult (*dispatch_fn)(void* self, TrapRequest request) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return self != nullptr && dispatch_fn != nullptr;
        }

        [[nodiscard]] TrapResult call(TrapRequest request) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_bridge,
                    .value = 0,
                };
            }

            return dispatch_fn(self, request);
        }

        [[nodiscard]] TrapResult yield_current(
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return yield_current(TrapYieldCurrentView{}, origin);
        }

        [[nodiscard]] TrapResult yield_current(
            TrapYieldCurrentView yield,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return call(make_yield_current_trap_request(yield, origin));
        }

        [[nodiscard]] TrapResult sleep_current_until(
            Tick due,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return sleep_current_until(TrapSleepUntilView<Tick>{
                                           .due = due,
                                       },
                                       origin);
        }

        [[nodiscard]] TrapResult sleep_current_until(
            TrapSleepUntilView<Tick> sleep,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return call(make_sleep_until_trap_request(sleep, origin));
        }

        [[nodiscard]] TrapResult debug_write(
            util::u64 value,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return debug_write(TrapDebugWriteView{
                                   .value = value,
                               },
                               origin);
        }

        [[nodiscard]] TrapResult debug_write(
            TrapDebugWriteView write,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return call(make_debug_write_trap_request(write, origin));
        }

        [[nodiscard]] TrapResult capability_call(
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload = 0,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return capability_call(TrapCapabilityCallView{
                                       .capability_id = capability_id,
                                       .operation = operation,
                                       .payload = payload,
                                   },
                                   origin);
        }

        [[nodiscard]] TrapResult capability_call(
            TrapCapabilityCallView capability,
            TrapOrigin origin = TrapOrigin::kernel_thread) const noexcept
        {
            return call(make_capability_call_trap_request(capability, origin));
        }
    };

    namespace detail {
        template <typename Bridge>
        [[nodiscard]] TrapResult runtime_trap_dispatch_adapter(
            void* self,
            TrapRequest request) noexcept
        {
            return static_cast<Bridge*>(self)->dispatch(request);
        }
    }

    template <typename Bridge>
    [[nodiscard]] auto make_runtime_trap_port(Bridge& bridge) noexcept
        -> RuntimeTrapPort<typename Bridge::tick_type>
    {
        return RuntimeTrapPort<typename Bridge::tick_type>{
            .self = &bridge,
            .dispatch_fn = &detail::runtime_trap_dispatch_adapter<Bridge>,
        };
    }
}
