module;

#include <array>
#include <cstddef>
#include <limits>
#include <string_view>

export module kernel.task_message_syscall_bridge;

export import kernel.task_message_table;
export import kernel.task_syscall_table;
import semantic.core;
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

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallBridgeTraceEvent>(
            std::array<std::string_view, 15>{
                "sequence",
                "from",
                "label",
                "syscall",
                "value",
                "request_sequence",
                "ingress_supported",
                "disposition",
                "error",
                "arg0",
                "arg1",
                "arg2",
                "arg3",
                "reply_value",
                "handled",
            }));

    struct TaskMessageSyscallBridgeWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
        TaskId from{};
        util::u64 label{0};
        util::u64 request_sequence{0};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        bool ingress_supported{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unsupported_service};
        util::u64 reply_value{0};
        bool handled{false};
        bool has_lower_provenance{false};
        TaskSyscallTableWitness lower_provenance{};

        [[nodiscard]] constexpr bool trap_terminal_ok() const noexcept
        {
            return disposition == TrapDisposition::handled &&
                   error == TrapError::none;
        }

        [[nodiscard]] constexpr bool branch_supported_ok() const noexcept
        {
            return ingress_supported && handled == trap_terminal_ok();
        }

        [[nodiscard]] constexpr bool branch_unsupported_ok() const noexcept
        {
            return !ingress_supported &&
                   disposition == TrapDisposition::unsupported &&
                   error == TrapError::unsupported_service && !handled;
        }

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return verdict() == semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr semantic::Result result() const noexcept
        {
            return verdict() == semantic::Verdict::standing
                       ? semantic::Result::ok
                       : semantic::Result::failed;
        }

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            return lower_provenance.ready &&
                   lower_provenance.syscall == syscall &&
                   lower_provenance.disposition == disposition &&
                   lower_provenance.error == error &&
                   lower_provenance.value == reply_value;
        }

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (!(branch_supported_ok() || branch_unsupported_ok())) {
                return semantic::Verdict::drifted;
            }

            if (!lower_route_consistent()) {
                return semantic::Verdict::drifted;
            }

            return semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            if (!ready) {
                return semantic::FailureDomain::input;
            }

            const bool supported_shape_mismatch =
                ingress_supported && handled != trap_terminal_ok();
            const bool unsupported_shape_mismatch =
                !ingress_supported &&
                !(disposition == TrapDisposition::unsupported &&
                  error == TrapError::unsupported_service && !handled);

            if (supported_shape_mismatch || unsupported_shape_mismatch) {
                if (supported_shape_mismatch) {
                    return semantic::FailureDomain::handoff;
                }

                return semantic::FailureDomain::selection;
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-syscall-bridge-witness.summary";
        }
    };

    struct TaskMessageSyscallBridgeWitnessHandoffTarget {
        const TaskMessageSyscallBridgeWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-syscall-bridge-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-syscall-bridge-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallBridgeWitness>(
            std::array<std::string_view, 14>{
                "sequence",
                "ready",
                "has_trace",
                "from",
                "label",
                "request_sequence",
                "syscall",
                "ingress_supported",
                "disposition",
                "error",
                "reply_value",
                "handled",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSyscallBridgeWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSyscallBridgeWitnessHandoffTarget>);

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

    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitness
    task_message_syscall_bridge_witness(
        const TaskMessageSyscallBridgeResult& result) noexcept
    {
        return TaskMessageSyscallBridgeWitness{
            .ready = true,
            .from = result.request.from,
            .label = result.request.label,
            .request_sequence = result.request.sequence,
            .syscall = result.syscall_request.syscall,
            .ingress_supported = result.ingress_supported,
            .disposition = result.trap.disposition,
            .error = result.trap.error,
            .reply_value = result.trap.value,
            .handled = result.message.handled,
        };
    }

    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitness
    task_message_syscall_bridge_witness(
        const TaskMessageSyscallBridgeResult& result,
        const TaskSyscallTableWitness& lower) noexcept
    {
        auto witness = task_message_syscall_bridge_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitness
    task_message_syscall_bridge_witness(
        const TaskMessageSyscallBridgeTraceEvent& event) noexcept
    {
        return TaskMessageSyscallBridgeWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .from = event.from,
            .label = event.label,
            .request_sequence = event.request_sequence,
            .syscall = event.syscall,
            .ingress_supported = event.ingress_supported,
            .disposition = event.disposition,
            .error = event.error,
            .reply_value = event.reply_value,
            .handled = event.handled,
        };
    }

    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitness
    task_message_syscall_bridge_witness(
        const TaskMessageSyscallBridgeTraceEvent& event,
        const TaskSyscallTableWitness& lower) noexcept
    {
        auto witness = task_message_syscall_bridge_witness(event);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_syscall_bridge_witness_ready(
        const TaskMessageSyscallBridgeWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitnessHandoffTarget
    task_message_syscall_bridge_witness_handoff_target(
        const TaskMessageSyscallBridgeWitness& witness) noexcept
    {
        return TaskMessageSyscallBridgeWitnessHandoffTarget{
            .witness = &witness,
        };
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

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitness
    task_message_syscall_bridge_witness(
        const TaskMessageSyscallBridgeTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSyscallBridgeWitness{};
        }

        return task_message_syscall_bridge_witness(*terminal);
    }

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallBridgeWitness
    task_message_syscall_bridge_witness(
        const TaskMessageSyscallBridgeTraceBuffer<Capacity>& trace,
        const TaskSyscallTableWitness& lower) noexcept
    {
        auto witness = task_message_syscall_bridge_witness(trace);
        if (!witness.ready) {
            return witness;
        }

        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

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
