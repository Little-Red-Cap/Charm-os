module;

#include <array>
#include <cstddef>

export module kernel.task_syscall_dispatch;

export import kernel.task_syscall_catalog;
import util.core;

export namespace kernel {
    struct TaskSyscallRequest {
        TaskSyscallId syscall{TaskSyscallId::invalid};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
    };

    [[nodiscard]] constexpr TaskSyscallRequest make_task_syscall_yield_request(
        TrapYieldCurrentView = {}) noexcept
    {
        return TaskSyscallRequest{
            .syscall = TaskSyscallId::yield,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr TaskSyscallRequest
    make_task_syscall_sleep_until_request(
        TrapSleepUntilView<Tick> sleep) noexcept
    {
        return TaskSyscallRequest{
            .syscall = TaskSyscallId::sleep_until,
            .arg0 = static_cast<util::u64>(sleep.due),
        };
    }

    [[nodiscard]] constexpr TaskSyscallRequest
    make_task_syscall_debug_write_request(
        TrapDebugWriteView write) noexcept
    {
        return TaskSyscallRequest{
            .syscall = TaskSyscallId::debug_write,
            .arg0 = write.value,
        };
    }

    [[nodiscard]] constexpr TaskSyscallRequest
    make_task_syscall_capability_call_request(
        TrapCapabilityCallView capability) noexcept
    {
        return TaskSyscallRequest{
            .syscall = TaskSyscallId::capability_call,
            .arg0 = capability.capability_id,
            .arg1 = capability.operation,
            .arg2 = capability.payload,
        };
    }

    [[nodiscard]] constexpr TaskSyscallRequest task_syscall_request_from_trap_request(
        const TrapRequest& request) noexcept
    {
        return TaskSyscallRequest{
            .syscall = task_syscall_from_trap_service(request.service),
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
        };
    }

    [[nodiscard]] constexpr TrapRequest trap_request_from_task_syscall_request(
        TaskSyscallRequest request,
        TrapOrigin origin = TrapOrigin::kernel_thread) noexcept
    {
        return TrapRequest{
            .service = trap_service_from_task_syscall(request.syscall),
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
            .origin = origin,
        };
    }

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(const TaskSyscallRequest& request) noexcept
    {
        const auto descriptor = task_syscall_catalog_entry(request.syscall);
        switch (descriptor.view_kind) {
        case TaskSyscallViewKind::yield:
            return TaskSyscallSemanticProjection{
                .descriptor = descriptor,
                .fields = {},
                .field_count = 0,
                .result_name = descriptor.result_name,
            };
        case TaskSyscallViewKind::sleep_until:
            return TaskSyscallSemanticProjection{
                .descriptor = descriptor,
                .fields = {
                    trap_semantic_field(descriptor.wire_argument_names[0],
                                        request.arg0),
                },
                .field_count = 1,
                .result_name = descriptor.result_name,
            };
        case TaskSyscallViewKind::debug_write:
            return TaskSyscallSemanticProjection{
                .descriptor = descriptor,
                .fields = {
                    trap_semantic_field(descriptor.wire_argument_names[0],
                                        request.arg0),
                },
                .field_count = 1,
                .result_name = descriptor.result_name,
            };
        case TaskSyscallViewKind::capability_call:
            return TaskSyscallSemanticProjection{
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
        case TaskSyscallViewKind::invalid:
        case TaskSyscallViewKind::opaque:
        default:
            return TaskSyscallSemanticProjection{
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

    struct TaskSyscallDispatchTraceEvent {
        util::u64 sequence{0};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        TrapService trap_service{TrapService::invalid};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unsupported_service};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 value{0};
    };

    [[nodiscard]] constexpr TaskSyscallRequest
    task_syscall_request_from_trace_event(
        const TaskSyscallDispatchTraceEvent& event) noexcept
    {
        return TaskSyscallRequest{
            .syscall = event.syscall,
            .arg0 = event.arg0,
            .arg1 = event.arg1,
            .arg2 = event.arg2,
            .arg3 = event.arg3,
        };
    }

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(
        const TaskSyscallDispatchTraceEvent& event) noexcept
    {
        return task_syscall_semantic_projection(
            task_syscall_request_from_trace_event(event));
    }

    template <std::size_t Capacity>
    class TaskSyscallDispatchTraceBuffer {
    public:
        using value_type = TaskSyscallDispatchTraceEvent;

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

    template <typename Surface,
              typename TraceBuffer = TaskSyscallDispatchTraceBuffer<1>>
    class TaskSyscallDispatcher {
    public:
        using surface_type = Surface;
        using tick_type = typename Surface::tick_type;
        using trace_type = TraceBuffer;

        constexpr TaskSyscallDispatcher() noexcept = default;

        constexpr explicit TaskSyscallDispatcher(
            Surface surface,
            TraceBuffer* trace = nullptr) noexcept
            : surface_(surface), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return surface_.valid();
        }

        [[nodiscard]] Surface& surface() noexcept
        {
            return surface_;
        }

        [[nodiscard]] const Surface& surface() const noexcept
        {
            return surface_;
        }

        void bind_surface(Surface surface) noexcept
        {
            surface_ = surface;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] TrapResult dispatch(TaskSyscallRequest request) noexcept
        {
            const auto descriptor = task_syscall_catalog_entry(request.syscall);
            if (!valid()) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_bridge,
                    .value = 0,
                };
                trace_push(request, descriptor.trap_service, result);
                return result;
            }

            TrapResult result{};
            switch (descriptor.view_kind) {
            case TaskSyscallViewKind::yield:
                result = surface_.yield_current(TrapYieldCurrentView{});
                trace_push(request, descriptor.trap_service, result);
                return result;
            case TaskSyscallViewKind::sleep_until:
                result = surface_.sleep_current_until(
                    TrapSleepUntilView<tick_type>{
                        .due = static_cast<tick_type>(request.arg0),
                    });
                trace_push(request, descriptor.trap_service, result);
                return result;
            case TaskSyscallViewKind::debug_write:
                result = surface_.debug_write(TrapDebugWriteView{
                    .value = request.arg0,
                });
                trace_push(request, descriptor.trap_service, result);
                return result;
            case TaskSyscallViewKind::capability_call:
                result = surface_.capability_call(TrapCapabilityCallView{
                    .capability_id = request.arg0,
                    .operation = request.arg1,
                    .payload = request.arg2,
                });
                trace_push(request, descriptor.trap_service, result);
                return result;
            case TaskSyscallViewKind::invalid:
            case TaskSyscallViewKind::opaque:
            default:
                result = TrapResult{
                    .disposition = TrapDisposition::unsupported,
                    .error = TrapError::unsupported_service,
                    .value = 0,
                };
                trace_push(request, descriptor.trap_service, result);
                return result;
            }
        }

        [[nodiscard]] TrapResult dispatch(const TrapRequest& request) noexcept
        {
            return dispatch(task_syscall_request_from_trap_request(request));
        }

        [[nodiscard]] TrapResult yield() noexcept
        {
            return dispatch(make_task_syscall_yield_request());
        }

        [[nodiscard]] TrapResult sleep_until(tick_type due) noexcept
        {
            return dispatch(make_task_syscall_sleep_until_request(
                TrapSleepUntilView<tick_type>{
                    .due = due,
                }));
        }

        [[nodiscard]] TrapResult debug_write(util::u64 value) noexcept
        {
            return dispatch(make_task_syscall_debug_write_request(
                TrapDebugWriteView{
                    .value = value,
                }));
        }

        [[nodiscard]] TrapResult capability_call(util::u64 capability_id,
                                                 util::u64 operation,
                                                 util::u64 payload = 0) noexcept
        {
            return dispatch(make_task_syscall_capability_call_request(
                TrapCapabilityCallView{
                    .capability_id = capability_id,
                    .operation = operation,
                    .payload = payload,
                }));
        }

    private:
        void trace_push(TaskSyscallRequest request,
                        TrapService service,
                        const TrapResult& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .syscall = request.syscall,
                .trap_service = service,
                .disposition = result.disposition,
                .error = result.error,
                .arg0 = request.arg0,
                .arg1 = request.arg1,
                .arg2 = request.arg2,
                .arg3 = request.arg3,
                .value = result.value,
            });
        }

        Surface surface_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Surface>
    [[nodiscard]] auto make_task_syscall_dispatcher(
        Surface surface) noexcept -> TaskSyscallDispatcher<Surface>
    {
        return TaskSyscallDispatcher<Surface>{surface};
    }

    template <typename Surface, typename TraceBuffer>
    [[nodiscard]] auto make_task_syscall_dispatcher(
        Surface surface,
        TraceBuffer* trace) noexcept -> TaskSyscallDispatcher<Surface, TraceBuffer>
    {
        return TaskSyscallDispatcher<Surface, TraceBuffer>{surface, trace};
    }

    template <typename Tick>
    struct TaskSyscallDispatchPort {
        using tick_type = Tick;

        void* self{nullptr};
        TrapResult (*dispatch_fn)(void* self,
                                  TaskSyscallRequest request) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return self != nullptr && dispatch_fn != nullptr;
        }

        [[nodiscard]] TrapResult dispatch(TaskSyscallRequest request) const
            noexcept
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

        [[nodiscard]] TrapResult dispatch(const TrapRequest& request) const
            noexcept
        {
            return dispatch(task_syscall_request_from_trap_request(request));
        }

        [[nodiscard]] TrapResult yield() const noexcept
        {
            return dispatch(make_task_syscall_yield_request());
        }

        [[nodiscard]] TrapResult sleep_until(Tick due) const noexcept
        {
            return dispatch(make_task_syscall_sleep_until_request(
                TrapSleepUntilView<Tick>{
                    .due = due,
                }));
        }

        [[nodiscard]] TrapResult debug_write(util::u64 value) const noexcept
        {
            return dispatch(make_task_syscall_debug_write_request(
                TrapDebugWriteView{
                    .value = value,
                }));
        }

        [[nodiscard]] TrapResult capability_call(util::u64 capability_id,
                                                 util::u64 operation,
                                                 util::u64 payload = 0) const
            noexcept
        {
            return dispatch(make_task_syscall_capability_call_request(
                TrapCapabilityCallView{
                    .capability_id = capability_id,
                    .operation = operation,
                    .payload = payload,
                }));
        }
    };

    namespace detail {
        template <typename Dispatcher>
        [[nodiscard]] TrapResult task_syscall_dispatch_adapter(
            void* self,
            TaskSyscallRequest request) noexcept
        {
            return static_cast<Dispatcher*>(self)->dispatch(request);
        }
    }

    template <typename Dispatcher>
    [[nodiscard]] auto make_task_syscall_dispatch_port(
        Dispatcher& dispatcher) noexcept
        -> TaskSyscallDispatchPort<typename Dispatcher::tick_type>
    {
        return TaskSyscallDispatchPort<typename Dispatcher::tick_type>{
            .self = &dispatcher,
            .dispatch_fn = &detail::task_syscall_dispatch_adapter<Dispatcher>,
        };
    }
}
