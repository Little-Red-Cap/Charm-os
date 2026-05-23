module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_syscall_api;

export import kernel.task_message_runtime_api;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageSyscallApiWitnessKind : util::u8 {
        issue = 0,
        reply,
        timeout,
        completion_drop,
    };

    [[nodiscard]] constexpr const char*
    task_message_syscall_api_witness_kind_name(
        TaskMessageSyscallApiWitnessKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageSyscallApiWitnessKind::issue:
            return "issue";
        case TaskMessageSyscallApiWitnessKind::reply:
            return "reply";
        case TaskMessageSyscallApiWitnessKind::timeout:
            return "timeout";
        case TaskMessageSyscallApiWitnessKind::completion_drop:
            return "completion-drop";
        }
        return "unknown";
    }

    struct TaskMessageSyscallApiWitness {
        bool ready{false};
        bool state_observed{false};
        bool valid{false};
        bool busy{false};
        std::size_t pending_requests{0};
        std::size_t pending_completions{0};
        TaskMessageSyscallApiWitnessKind kind{
            TaskMessageSyscallApiWitnessKind::issue};
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
        TaskMessageRuntimeApiWitness lower_provenance{};

        [[nodiscard]] constexpr bool issue_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallApiWitnessKind::issue && issued &&
                   owner != TaskId{} && token != 0u &&
                   request_sequence != 0u;
        }

        [[nodiscard]] constexpr bool reply_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallApiWitnessKind::reply &&
                   completion_ready && completion_pushed &&
                   !completion_dropped && !timeout;
        }

        [[nodiscard]] constexpr bool timeout_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallApiWitnessKind::timeout &&
                   completion_ready && completion_pushed &&
                   !completion_dropped && timeout;
        }

        [[nodiscard]] constexpr bool completion_drop_branch() const noexcept
        {
            return kind == TaskMessageSyscallApiWitnessKind::completion_drop &&
                   completion_ready && completion_dropped &&
                   !completion_pushed;
        }

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            switch (kind) {
            case TaskMessageSyscallApiWitnessKind::issue:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeApiWitnessKind::issue &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.wait_due == wait_due;
            case TaskMessageSyscallApiWitnessKind::reply:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeApiWitnessKind::reply &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       !lower_provenance.timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageSyscallApiWitnessKind::timeout:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeApiWitnessKind::timeout &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageSyscallApiWitnessKind::completion_drop:
                return lower_provenance.ready &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeApiWitnessKind::completion_drop &&
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
                return semantic::Verdict::drifted;
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

            if (kind == TaskMessageSyscallApiWitnessKind::issue) {
                if (!issued || owner == TaskId{} || token == 0u ||
                    request_sequence == 0u) {
                    return semantic::FailureDomain::selection;
                }
            } else {
                const bool completion_shape_mismatch =
                    !completion_ready ||
                    !completion_pushed ||
                    completion_dropped ||
                    (kind == TaskMessageSyscallApiWitnessKind::reply &&
                     timeout) ||
                    (kind == TaskMessageSyscallApiWitnessKind::timeout &&
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
            return "task-message-syscall-api-witness.summary";
        }
    };

    struct TaskMessageSyscallApiWitnessHandoffTarget {
        const TaskMessageSyscallApiWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-syscall-api-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-syscall-api-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallApiWitness>(
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

    static_assert(semantic::WitnessCarrier<TaskMessageSyscallApiWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSyscallApiWitnessHandoffTarget>);

    template <typename Result>
    [[nodiscard]] constexpr TaskMessageSyscallApiWitness
    task_message_syscall_api_witness(const Result& result) noexcept
    {
        auto witness = TaskMessageSyscallApiWitness{};
        witness.ready = result.issued || result.completion_ready ||
                        result.completion_dropped;
        witness.issued = result.issued;
        witness.completion_ready = result.completion_ready;
        witness.completion_pushed = result.completion_pushed;
        witness.completion_dropped = result.completion_dropped;
        if (result.completion_dropped) {
            witness.kind = TaskMessageSyscallApiWitnessKind::completion_drop;
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
                               ? TaskMessageSyscallApiWitnessKind::timeout
                               : TaskMessageSyscallApiWitnessKind::reply;
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
            witness.kind = TaskMessageSyscallApiWitnessKind::issue;
            witness.owner = result.issued_request.owner;
            witness.token = result.issued_request.token;
            witness.request_sequence = result.issued_request.request_sequence;
            witness.wait_due =
                static_cast<util::u64>(result.issued_request.wait_due);
        }

        return witness;
    }

    template <typename Api, typename Result>
    [[nodiscard]] constexpr TaskMessageSyscallApiWitness
    task_message_syscall_api_witness(const Api& api,
                                     const Result& result) noexcept
    {
        auto witness = task_message_syscall_api_witness(result);
        witness.state_observed = true;
        witness.valid = api.valid();
        witness.busy = api.busy();
        witness.pending_requests = api.pending_requests();
        witness.pending_completions = api.pending_completions();
        return witness;
    }

    template <typename Result>
    [[nodiscard]] constexpr TaskMessageSyscallApiWitness
    task_message_syscall_api_witness(
        const Result& result,
        const TaskMessageRuntimeApiWitness& lower) noexcept
    {
        auto witness = task_message_syscall_api_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <typename Api, typename Result>
    [[nodiscard]] constexpr TaskMessageSyscallApiWitness
    task_message_syscall_api_witness(
        const Api& api,
        const Result& result,
        const TaskMessageRuntimeApiWitness& lower) noexcept
    {
        auto witness = task_message_syscall_api_witness(api, result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_syscall_api_witness_ready(
        const TaskMessageSyscallApiWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSyscallApiWitnessHandoffTarget
    task_message_syscall_api_witness_handoff_target(
        const TaskMessageSyscallApiWitness& witness) noexcept
    {
        return TaskMessageSyscallApiWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <typename Runtime>
    class TaskMessageSyscallApi {
    public:
        using runtime_type = Runtime;
        using tick_type = typename Runtime::tick_type;
        using completion_type = typename Runtime::completion_type;
        using result_type = typename Runtime::result_type;

        constexpr TaskMessageSyscallApi() noexcept = default;

        constexpr explicit TaskMessageSyscallApi(Runtime runtime) noexcept
            : runtime_(runtime)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return runtime_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return runtime_.busy();
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return runtime_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return runtime_.pending_completions();
        }

        [[nodiscard]] Runtime& runtime() noexcept
        {
            return runtime_;
        }

        [[nodiscard]] const Runtime& runtime() const noexcept
        {
            return runtime_;
        }

        void bind_runtime(Runtime runtime) noexcept
        {
            runtime_ = runtime;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            runtime_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool sys_yield(tick_type wait_due) noexcept
        {
            return sys_yield(TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool sys_yield(TrapYieldCurrentView yield_view,
                                     tick_type wait_due) noexcept
        {
            return runtime_.yield(yield_view, wait_due);
        }

        [[nodiscard]] bool sys_sleep_until(tick_type due,
                                           tick_type wait_due) noexcept
        {
            return sys_sleep_until(TrapSleepUntilView<tick_type>{
                                       .due = due,
                                   },
                                   wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sys_sleep_until(TrapSleepUntilView<Tick> sleep,
                                           tick_type wait_due) noexcept
        {
            return runtime_.sleep_until(sleep, wait_due);
        }

        [[nodiscard]] bool sys_debug_write(util::u64 value,
                                           tick_type wait_due) noexcept
        {
            return sys_debug_write(TrapDebugWriteView{
                                       .value = value,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool sys_debug_write(TrapDebugWriteView write,
                                           tick_type wait_due) noexcept
        {
            return runtime_.debug_write(write, wait_due);
        }

        [[nodiscard]] bool sys_capability_call(util::u64 capability_id,
                                               util::u64 operation,
                                               tick_type wait_due) noexcept
        {
            return sys_capability_call(
                capability_id, operation, 0u, wait_due);
        }

        [[nodiscard]] bool sys_capability_call(util::u64 capability_id,
                                               util::u64 operation,
                                               util::u64 payload,
                                               tick_type wait_due) noexcept
        {
            return sys_capability_call(TrapCapabilityCallView{
                                           .capability_id = capability_id,
                                           .operation = operation,
                                           .payload = payload,
                                       },
                                       wait_due);
        }

        [[nodiscard]] bool sys_capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return runtime_.capability_call(capability, wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            return runtime_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return runtime_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return runtime_.receive_completion(out);
        }

    private:
        Runtime runtime_{};
    };

    template <typename Runtime>
    [[nodiscard]] auto make_task_message_syscall_api(
        Runtime runtime) noexcept -> TaskMessageSyscallApi<Runtime>
    {
        return TaskMessageSyscallApi<Runtime>{runtime};
    }
}
