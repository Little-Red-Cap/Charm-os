module;

#include <array>
#include <cstddef>
#include <limits>

export module kernel.task_message_syscall_bridge;

export import kernel.task_message_table;
export import kernel.task_syscall_dispatch;
import util.core;

export namespace kernel {
    [[nodiscard]] constexpr util::u64 task_message_syscall_label(
        TaskSyscallId syscall) noexcept
    {
        return static_cast<util::u64>(static_cast<util::u16>(syscall));
    }

    [[nodiscard]] constexpr const char* task_message_syscall_label_name(
        TaskSyscallId syscall) noexcept
    {
        return task_syscall_name(syscall);
    }

    [[nodiscard]] constexpr TaskSyscallId task_message_syscall_id(
        util::u64 label) noexcept
    {
        if (label > std::numeric_limits<util::u16>::max()) {
            return TaskSyscallId::invalid;
        }

        return static_cast<TaskSyscallId>(static_cast<util::u16>(label));
    }

    [[nodiscard]] constexpr bool task_message_syscall_ingress_supported(
        TaskSyscallId syscall) noexcept
    {
        switch (syscall) {
        case TaskSyscallId::yield:
        case TaskSyscallId::sleep_until:
        case TaskSyscallId::debug_write:
            return true;
        case TaskSyscallId::capability_call:
        case TaskSyscallId::invalid:
            return false;
        }
        return false;
    }

    [[nodiscard]] constexpr TaskSyscallRequest
    task_message_syscall_request_from_message(
        const RuntimeMailboxRequest& request) noexcept
    {
        return TaskSyscallRequest{
            .syscall = task_message_syscall_id(request.label),
            .arg0 = request.value,
        };
    }

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_message_syscall_semantic_projection(
        const RuntimeMailboxRequest& request) noexcept
    {
        return task_syscall_semantic_projection(
            task_message_syscall_request_from_message(request));
    }

    struct TaskMessageSyscallBridgeResult {
        RuntimeMailboxRequest request{};
        TaskSyscallRequest syscall_request{};
        bool ingress_supported{false};
        TrapResult trap{};
        TaskMessageHandleResult message{};
    };

    struct TaskMessageSyscallBridgeTraceEvent {
        util::u64 sequence{0};
        TaskId from{};
        util::u64 label{0};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        util::u64 value{0};
        util::u64 request_sequence{0};
        bool ingress_supported{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unsupported_service};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 reply_value{0};
        bool handled{false};
    };

    [[nodiscard]] constexpr RuntimeMailboxRequest
    task_message_request_from_trace_event(
        const TaskMessageSyscallBridgeTraceEvent& event) noexcept
    {
        return RuntimeMailboxRequest{
            .from = event.from,
            .label = event.label,
            .value = event.value,
            .sequence = event.request_sequence,
        };
    }

    [[nodiscard]] constexpr TaskSyscallRequest
    task_message_syscall_request_from_trace_event(
        const TaskMessageSyscallBridgeTraceEvent& event) noexcept
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
    task_message_syscall_semantic_projection(
        const TaskMessageSyscallBridgeTraceEvent& event) noexcept
    {
        return task_syscall_semantic_projection(
            task_message_syscall_request_from_trace_event(event));
    }

    template <std::size_t Capacity>
    class TaskMessageSyscallBridgeTraceBuffer {
    public:
        using value_type = TaskMessageSyscallBridgeTraceEvent;

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

    template <typename Dispatch,
              typename TraceBuffer = TaskMessageSyscallBridgeTraceBuffer<1>>
    class TaskMessageSyscallBridge {
    public:
        using dispatch_type = Dispatch;
        using trace_type = TraceBuffer;
        using result_type = TaskMessageSyscallBridgeResult;

        constexpr TaskMessageSyscallBridge() noexcept = default;

        constexpr explicit TaskMessageSyscallBridge(
            Dispatch dispatch,
            TraceBuffer* trace = nullptr) noexcept
            : dispatch_(dispatch), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            if constexpr (requires(const Dispatch& dispatch) {
                              dispatch.valid();
                          }) {
                return dispatch_.valid();
            } else {
                return true;
            }
        }

        [[nodiscard]] Dispatch& dispatch_target() noexcept
        {
            return dispatch_;
        }

        [[nodiscard]] const Dispatch& dispatch_target() const noexcept
        {
            return dispatch_;
        }

        void bind_dispatch_target(Dispatch dispatch) noexcept
        {
            dispatch_ = dispatch;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] result_type dispatch_message(
            const RuntimeMailboxRequest& request) noexcept
        {
            const auto syscall_request =
                task_message_syscall_request_from_message(request);
            const auto ingress_supported =
                task_message_syscall_ingress_supported(syscall_request.syscall);

            auto trap = TrapResult{
                .disposition = TrapDisposition::unsupported,
                .error = TrapError::unsupported_service,
                .value = 0,
            };
            auto message = unhandled_task_message();

            if (ingress_supported) {
                trap = dispatch_.dispatch(syscall_request);
                if (trap.ok()) {
                    message = handled_task_message(trap.value);
                }
            }

            const auto result = result_type{
                .request = request,
                .syscall_request = syscall_request,
                .ingress_supported = ingress_supported,
                .trap = trap,
                .message = message,
            };
            trace_push(result);
            return result;
        }

        [[nodiscard]] TaskMessageHandleResult dispatch(
            const RuntimeMailboxRequest& request) noexcept
        {
            return dispatch_message(request).message;
        }

    private:
        void trace_push(const result_type& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .from = result.request.from,
                .label = result.request.label,
                .syscall = result.syscall_request.syscall,
                .value = result.request.value,
                .request_sequence = result.request.sequence,
                .ingress_supported = result.ingress_supported,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
                .arg0 = result.syscall_request.arg0,
                .arg1 = result.syscall_request.arg1,
                .arg2 = result.syscall_request.arg2,
                .arg3 = result.syscall_request.arg3,
                .reply_value = result.trap.value,
                .handled = result.message.handled,
            });
        }

        Dispatch dispatch_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Dispatch>
    [[nodiscard]] auto make_task_message_syscall_bridge(
        Dispatch dispatch) noexcept -> TaskMessageSyscallBridge<Dispatch>
    {
        return TaskMessageSyscallBridge<Dispatch>{dispatch};
    }

    template <typename Dispatch, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_syscall_bridge(
        Dispatch dispatch,
        TraceBuffer* trace) noexcept
        -> TaskMessageSyscallBridge<Dispatch, TraceBuffer>
    {
        return TaskMessageSyscallBridge<Dispatch, TraceBuffer>{
            dispatch,
            trace,
        };
    }
}
