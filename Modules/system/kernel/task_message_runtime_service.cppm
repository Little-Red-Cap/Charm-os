module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_runtime_service;

export import kernel.runtime_service;
export import kernel.task_message_syscall_pump;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageRuntimeServiceWitnessKind : util::u8 {
        issue = 0,
        reply,
        timeout,
        completion_drop,
    };

    [[nodiscard]] constexpr const char*
    task_message_runtime_service_witness_kind_name(
        TaskMessageRuntimeServiceWitnessKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageRuntimeServiceWitnessKind::issue:
            return "issue";
        case TaskMessageRuntimeServiceWitnessKind::reply:
            return "reply";
        case TaskMessageRuntimeServiceWitnessKind::timeout:
            return "timeout";
        case TaskMessageRuntimeServiceWitnessKind::completion_drop:
            return "completion-drop";
        }
        return "unknown";
    }

    struct TaskMessageRuntimeServiceWitness {
        bool ready{false};
        bool state_observed{false};
        bool valid{false};
        bool busy{false};
        std::size_t pending_requests{0};
        std::size_t pending_completions{0};
        TaskMessageRuntimeServiceWitnessKind kind{
            TaskMessageRuntimeServiceWitnessKind::issue};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        util::u64 wait_due{0};
        bool issued{false};
        bool completion_ready{false};
        bool completion_pushed{false};
        bool completion_dropped{false};
        bool timeout{false};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        bool has_lower_provenance{false};
        TaskMessageSyscallPumpWitness lower_provenance{};

        [[nodiscard]] constexpr bool issue_branch_ok() const noexcept
        {
            return kind == TaskMessageRuntimeServiceWitnessKind::issue &&
                   issued && owner != TaskId{} && token != 0u &&
                   request_sequence != 0u;
        }

        [[nodiscard]] constexpr bool reply_branch_ok() const noexcept
        {
            return kind == TaskMessageRuntimeServiceWitnessKind::reply &&
                   completion_ready && completion_pushed &&
                   !completion_dropped && !timeout;
        }

        [[nodiscard]] constexpr bool timeout_branch_ok() const noexcept
        {
            return kind == TaskMessageRuntimeServiceWitnessKind::timeout &&
                   completion_ready && completion_pushed &&
                   !completion_dropped && timeout;
        }

        [[nodiscard]] constexpr bool completion_drop_branch() const noexcept
        {
            return kind ==
                       TaskMessageRuntimeServiceWitnessKind::completion_drop &&
                   completion_ready && completion_dropped &&
                   !completion_pushed;
        }

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            switch (kind) {
            case TaskMessageRuntimeServiceWitnessKind::issue:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageSyscallPumpTraceKind::issue &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.wait_due == wait_due;
            case TaskMessageRuntimeServiceWitnessKind::reply:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageSyscallPumpTraceKind::reply &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       !lower_provenance.timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageRuntimeServiceWitnessKind::timeout:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageSyscallPumpTraceKind::timeout &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageRuntimeServiceWitnessKind::completion_drop:
                return lower_provenance.ready &&
                       lower_provenance.kind ==
                           TaskMessageSyscallPumpTraceKind::completion_drop &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.timeout == timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            }
            return false;
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

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (completion_drop_branch()) {
                return lower_route_consistent()
                           ? semantic::Verdict::drifted
                           : semantic::Verdict::drifted;
            }

            if (!(issue_branch_ok() || reply_branch_ok() ||
                  timeout_branch_ok())) {
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

            if (completion_drop_branch()) {
                return semantic::FailureDomain::handoff;
            }

            if (kind == TaskMessageRuntimeServiceWitnessKind::issue) {
                if (!issued || owner == TaskId{} || token == 0u ||
                    request_sequence == 0u) {
                    return semantic::FailureDomain::selection;
                }
            } else {
                const bool completion_shape_mismatch =
                    !completion_ready ||
                    !completion_pushed ||
                    completion_dropped ||
                    (kind == TaskMessageRuntimeServiceWitnessKind::reply &&
                     timeout) ||
                    (kind == TaskMessageRuntimeServiceWitnessKind::timeout &&
                     !timeout);
                if (completion_shape_mismatch) {
                    return semantic::FailureDomain::handoff;
                }
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-runtime-service-witness.summary";
        }
    };

    struct TaskMessageRuntimeServiceWitnessHandoffTarget {
        const TaskMessageRuntimeServiceWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-runtime-service-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-runtime-service-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageRuntimeServiceWitness>(
            std::array<std::string_view, 21>{
                "ready",
                "state_observed",
                "valid",
                "busy",
                "pending_requests",
                "pending_completions",
                "kind",
                "owner",
                "token",
                "request_sequence",
                "wait_due",
                "issued",
                "completion_ready",
                "completion_pushed",
                "completion_dropped",
                "timeout",
                "reply_value",
                "disposition",
                "error",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageRuntimeServiceWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageRuntimeServiceWitnessHandoffTarget>);

    template <typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeServiceWitness
    task_message_runtime_service_witness(const Result& result) noexcept
    {
        auto witness = TaskMessageRuntimeServiceWitness{};
        witness.ready = result.issued || result.completion_ready ||
                        result.completion_dropped;
        witness.issued = result.issued;
        witness.completion_ready = result.completion_ready;
        witness.completion_pushed = result.completion_pushed;
        witness.completion_dropped = result.completion_dropped;
        if (result.completion_dropped) {
            witness.kind =
                TaskMessageRuntimeServiceWitnessKind::completion_drop;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
            return witness;
        }

        if (result.completion_ready) {
            witness.kind = result.completion.timeout
                               ? TaskMessageRuntimeServiceWitnessKind::timeout
                               : TaskMessageRuntimeServiceWitnessKind::reply;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
            return witness;
        }

        if (result.issued) {
            witness.kind = TaskMessageRuntimeServiceWitnessKind::issue;
            witness.owner = result.issued_request.owner;
            witness.token = result.issued_request.token;
            witness.request_sequence = result.issued_request.request_sequence;
            witness.wait_due =
                static_cast<util::u64>(result.issued_request.wait_due);
        }

        return witness;
    }

    template <typename Service, typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeServiceWitness
    task_message_runtime_service_witness(const Service& service,
                                         const Result& result) noexcept
    {
        auto witness = task_message_runtime_service_witness(result);
        witness.state_observed = true;
        witness.valid = service.valid();
        witness.busy = service.busy();
        witness.pending_requests = service.pending_requests();
        witness.pending_completions = service.pending_completions();
        return witness;
    }

    template <typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeServiceWitness
    task_message_runtime_service_witness(
        const Result& result,
        const TaskMessageSyscallPumpWitness& lower) noexcept
    {
        auto witness = task_message_runtime_service_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <typename Service, typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeServiceWitness
    task_message_runtime_service_witness(
        const Service& service,
        const Result& result,
        const TaskMessageSyscallPumpWitness& lower) noexcept
    {
        auto witness = task_message_runtime_service_witness(service, result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_runtime_service_witness_ready(
        const TaskMessageRuntimeServiceWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageRuntimeServiceWitnessHandoffTarget
    task_message_runtime_service_witness_handoff_target(
        const TaskMessageRuntimeServiceWitness& witness) noexcept
    {
        return TaskMessageRuntimeServiceWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <typename Pump>
    class TaskMessageRuntimeServiceFacade {
    public:
        using pump_type = Pump;
        using tick_type = typename Pump::tick_type;
        using reply_type = typename Pump::reply_type;
        using request_type = typename Pump::request_type;
        using completion_type = typename Pump::completion_type;
        using result_type = typename Pump::result_type;

        constexpr TaskMessageRuntimeServiceFacade() noexcept = default;

        constexpr explicit TaskMessageRuntimeServiceFacade(Pump pump) noexcept
            : pump_(pump)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return pump_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return pump_.busy();
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return pump_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return pump_.pending_completions();
        }

        [[nodiscard]] Pump& pump() noexcept
        {
            return pump_;
        }

        [[nodiscard]] const Pump& pump() const noexcept
        {
            return pump_;
        }

        void bind_pump(Pump pump) noexcept
        {
            pump_ = pump;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            pump_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool enqueue(request_type request) noexcept
        {
            return pump_.enqueue(request);
        }

        [[nodiscard]] bool enqueue(TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return pump_.enqueue(request, wait_due);
        }

        [[nodiscard]] bool enqueue(TaskId owner,
                                   TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return pump_.enqueue(owner, request, wait_due);
        }

        [[nodiscard]] bool enqueue(TaskId owner,
                                   util::u64 token,
                                   util::u64 request_sequence,
                                   TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return pump_.enqueue(
                owner, token, request_sequence, request, wait_due);
        }

        [[nodiscard]] bool yield_current(tick_type wait_due) noexcept
        {
            return yield_current(TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool yield_current(TrapYieldCurrentView,
                                         tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_yield_request(), wait_due);
        }

        [[nodiscard]] bool yield_current(TaskId owner,
                                         tick_type wait_due) noexcept
        {
            return yield_current(owner, TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool yield_current(TaskId owner,
                                         TrapYieldCurrentView,
                                         tick_type wait_due) noexcept
        {
            return enqueue(owner, make_task_syscall_yield_request(), wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_current_until(
            TrapSleepUntilView<Tick> sleep,
            tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_sleep_until_request(sleep),
                           wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_current_until(
            TaskId owner,
            TrapSleepUntilView<Tick> sleep,
            tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_sleep_until_request(sleep),
                           wait_due);
        }

        [[nodiscard]] bool debug_write(util::u64 value,
                                       tick_type wait_due) noexcept
        {
            return debug_write(TrapDebugWriteView{
                                   .value = value,
                               },
                               wait_due);
        }

        [[nodiscard]] bool debug_write(TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_debug_write_request(write),
                           wait_due);
        }

        [[nodiscard]] bool debug_write(TaskId owner,
                                       util::u64 value,
                                       tick_type wait_due) noexcept
        {
            return debug_write(owner,
                               TrapDebugWriteView{
                                   .value = value,
                               },
                               wait_due);
        }

        [[nodiscard]] bool debug_write(TaskId owner,
                                       TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_debug_write_request(write),
                           wait_due);
        }

        [[nodiscard]] bool capability_call(
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload,
            tick_type wait_due) noexcept
        {
            return capability_call(TrapCapabilityCallView{
                                       .capability_id = capability_id,
                                       .operation = operation,
                                       .payload = payload,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_capability_call_request(capability),
                           wait_due);
        }

        [[nodiscard]] bool capability_call(
            TaskId owner,
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload,
            tick_type wait_due) noexcept
        {
            return capability_call(owner,
                                   TrapCapabilityCallView{
                                       .capability_id = capability_id,
                                       .operation = operation,
                                       .payload = payload,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool capability_call(
            TaskId owner,
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_capability_call_request(capability),
                           wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            return pump_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return pump_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return pump_.receive_completion(out);
        }

    private:
        Pump pump_{};
    };

    template <typename Pump>
    [[nodiscard]] auto make_task_message_runtime_service_facade(
        Pump pump) noexcept -> TaskMessageRuntimeServiceFacade<Pump>
    {
        return TaskMessageRuntimeServiceFacade<Pump>{pump};
    }
}
