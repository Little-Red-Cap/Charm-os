module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_api;

export import kernel.task_message_syscall_api;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageSessionActionKind : util::u8 {
        none = 0,
        open,
        request,
        close,
    };

    [[nodiscard]] constexpr const char* task_message_session_action_kind_name(
        TaskMessageSessionActionKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageSessionActionKind::none:
            return "none";
        case TaskMessageSessionActionKind::open:
            return "open";
        case TaskMessageSessionActionKind::request:
            return "request";
        case TaskMessageSessionActionKind::close:
            return "close";
        }
        return "unknown";
    }

    enum class TaskMessageSessionPhase : util::u8 {
        idle = 0,
        opening,
        open,
        requesting,
        closing,
        faulted,
    };

    [[nodiscard]] constexpr const char* task_message_session_phase_name(
        TaskMessageSessionPhase phase) noexcept
    {
        switch (phase) {
        case TaskMessageSessionPhase::idle:
            return "idle";
        case TaskMessageSessionPhase::opening:
            return "opening";
        case TaskMessageSessionPhase::open:
            return "open";
        case TaskMessageSessionPhase::requesting:
            return "requesting";
        case TaskMessageSessionPhase::closing:
            return "closing";
        case TaskMessageSessionPhase::faulted:
            return "faulted";
        }
        return "unknown";
    }

    inline constexpr util::u64 task_message_session_open_operation =
        0xFFFF'FF00ull;
    inline constexpr util::u64 task_message_session_close_operation =
        0xFFFF'FF01ull;

    [[nodiscard]] constexpr bool is_task_message_session_control_operation(
        util::u64 operation) noexcept
    {
        return operation == task_message_session_open_operation ||
               operation == task_message_session_close_operation;
    }

    struct TaskMessageSessionOpenView {
        util::u64 service_id{0};
        util::u64 payload{0};
    };

    struct TaskMessageSessionRequestView {
        util::u64 operation{0};
        util::u64 payload{0};
    };

    struct TaskMessageSessionCloseView {
        util::u64 reason{0};
    };

    template <typename RawCompletion>
    struct TaskMessageSessionCompletion {
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        TaskMessageSessionPhase phase_before{TaskMessageSessionPhase::idle};
        TaskMessageSessionPhase phase_after{TaskMessageSessionPhase::idle};
        bool timeout{false};
        bool session_opened{false};
        bool session_closed{false};
        bool session_faulted{false};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 reply_value{0};
        TrapResult trap{
            .disposition = TrapDisposition::rejected,
            .error = TrapError::none,
            .value = 0,
        };
        RawCompletion raw{};
    };

    struct TaskMessageSessionApiWitness {
        bool ready{false};
        bool state_observed{false};
        bool valid{false};
        bool busy{false};
        bool opened{false};
        bool faulted{false};
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        TaskMessageSessionPhase phase_before{TaskMessageSessionPhase::idle};
        TaskMessageSessionPhase phase_after{TaskMessageSessionPhase::idle};
        bool timeout{false};
        bool session_opened{false};
        bool session_closed{false};
        bool session_faulted{false};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        bool has_lower_provenance{false};
        TaskMessageSyscallApiWitness lower_provenance{};

        [[nodiscard]] constexpr bool open_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   phase_before == TaskMessageSessionPhase::opening &&
                   phase_after ==
                       (session_opened ? TaskMessageSessionPhase::open
                                       : TaskMessageSessionPhase::idle) &&
                   service_id != 0u &&
                   operation == task_message_session_open_operation &&
                   ((!timeout && session_opened && !session_closed &&
                     !session_faulted && session_handle != 0u &&
                     disposition == TrapDisposition::handled) ||
                    ((timeout || disposition != TrapDisposition::handled) &&
                     !session_opened && !session_closed && !session_faulted &&
                     session_handle == 0u));
        }

        [[nodiscard]] constexpr bool request_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::request &&
                   phase_before == TaskMessageSessionPhase::requesting &&
                   phase_after == TaskMessageSessionPhase::open &&
                   !is_task_message_session_control_operation(operation) &&
                   service_id != 0u && session_handle != 0u &&
                   !session_opened && !session_closed && !session_faulted;
        }

        [[nodiscard]] constexpr bool close_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::close &&
                   phase_before == TaskMessageSessionPhase::closing &&
                   phase_after == (timeout ? TaskMessageSessionPhase::faulted
                                           : TaskMessageSessionPhase::idle) &&
                   service_id != 0u && session_handle != 0u &&
                   operation == task_message_session_close_operation &&
                   ((timeout && !session_opened && !session_closed &&
                     session_faulted) ||
                    (!timeout && !session_opened && session_closed &&
                     !session_faulted));
        }

        [[nodiscard]] constexpr bool state_consistent() const noexcept
        {
            if (phase_after == TaskMessageSessionPhase::open) {
                return opened && !faulted && session_handle != 0u;
            }

            if (phase_after == TaskMessageSessionPhase::faulted) {
                return !opened && faulted && session_handle != 0u;
            }

            if (phase_after == TaskMessageSessionPhase::idle) {
                return !opened && !faulted;
            }

            return false;
        }

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            const auto expected_kind =
                timeout ? TaskMessageSyscallApiWitnessKind::timeout
                        : TaskMessageSyscallApiWitnessKind::reply;

            switch (action) {
            case TaskMessageSessionActionKind::open:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind == expected_kind &&
                       lower_provenance.timeout == timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageSessionActionKind::request:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind == expected_kind &&
                       lower_provenance.timeout == timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageSessionActionKind::close:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind == expected_kind &&
                       lower_provenance.timeout == timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageSessionActionKind::none:
                return false;
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

            if (!state_consistent()) {
                return semantic::Verdict::drifted;
            }

            if (!(open_branch_ok() || request_branch_ok() ||
                  close_branch_ok())) {
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

            if (!state_consistent()) {
                return semantic::FailureDomain::handoff;
            }

            if (action == TaskMessageSessionActionKind::none) {
                return semantic::FailureDomain::selection;
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            if (!(open_branch_ok() || request_branch_ok() ||
                  close_branch_ok())) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-api-witness.summary";
        }
    };

    struct TaskMessageSessionApiWitnessHandoffTarget {
        const TaskMessageSessionApiWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-api-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-api-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSessionApiWitness>(
            std::array<std::string_view, 22>{
                "ready",
                "state_observed",
                "valid",
                "busy",
                "opened",
                "faulted",
                "action",
                "phase_before",
                "phase_after",
                "timeout",
                "session_opened",
                "session_closed",
                "session_faulted",
                "service_id",
                "session_handle",
                "operation",
                "payload",
                "reply_value",
                "disposition",
                "error",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSessionApiWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSessionApiWitnessHandoffTarget>);

    template <typename RawCompletion>
    [[nodiscard]] constexpr TaskMessageSessionApiWitness
    task_message_session_api_witness(
        const TaskMessageSessionCompletion<RawCompletion>& completion) noexcept
    {
        return TaskMessageSessionApiWitness{
            .ready = completion.action != TaskMessageSessionActionKind::none,
            .opened = completion.phase_after == TaskMessageSessionPhase::open,
            .faulted =
                completion.phase_after == TaskMessageSessionPhase::faulted,
            .action = completion.action,
            .phase_before = completion.phase_before,
            .phase_after = completion.phase_after,
            .timeout = completion.timeout,
            .session_opened = completion.session_opened,
            .session_closed = completion.session_closed,
            .session_faulted = completion.session_faulted,
            .service_id = completion.service_id,
            .session_handle = completion.session_handle,
            .operation = completion.operation,
            .payload = completion.payload,
            .reply_value = completion.reply_value,
            .disposition = completion.trap.disposition,
            .error = completion.trap.error,
        };
    }

    template <typename Api, typename RawCompletion>
    [[nodiscard]] constexpr TaskMessageSessionApiWitness
    task_message_session_api_witness(
        const Api& api,
        const TaskMessageSessionCompletion<RawCompletion>& completion) noexcept
    {
        auto witness = task_message_session_api_witness(completion);
        witness.state_observed = true;
        witness.valid = api.valid();
        witness.busy = api.busy();
        return witness;
    }

    template <typename RawCompletion>
    [[nodiscard]] constexpr TaskMessageSessionApiWitness
    task_message_session_api_witness(
        const TaskMessageSessionCompletion<RawCompletion>& completion,
        const TaskMessageSyscallApiWitness& lower) noexcept
    {
        auto witness = task_message_session_api_witness(completion);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <typename Api, typename RawCompletion>
    [[nodiscard]] constexpr TaskMessageSessionApiWitness
    task_message_session_api_witness(
        const Api& api,
        const TaskMessageSessionCompletion<RawCompletion>& completion,
        const TaskMessageSyscallApiWitness& lower) noexcept
    {
        auto witness = task_message_session_api_witness(api, completion);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_session_api_witness_ready(
        const TaskMessageSessionApiWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionApiWitnessHandoffTarget
    task_message_session_api_witness_handoff_target(
        const TaskMessageSessionApiWitness& witness) noexcept
    {
        return TaskMessageSessionApiWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <typename Syscalls>
    class TaskMessageSessionApi {
    public:
        using syscalls_type = Syscalls;
        using tick_type = typename Syscalls::tick_type;
        using result_type = typename Syscalls::result_type;
        using syscall_completion_type = typename Syscalls::completion_type;
        using completion_type =
            TaskMessageSessionCompletion<syscall_completion_type>;

        constexpr TaskMessageSessionApi() noexcept = default;

        constexpr explicit TaskMessageSessionApi(Syscalls syscalls) noexcept
            : syscalls_(syscalls)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return syscalls_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return pending_.action != TaskMessageSessionActionKind::none ||
                   syscalls_.busy() || syscalls_.pending_requests() != 0u ||
                   syscalls_.pending_completions() != 0u;
        }

        [[nodiscard]] bool opened() const noexcept
        {
            return phase_ == TaskMessageSessionPhase::open;
        }

        [[nodiscard]] bool faulted() const noexcept
        {
            return phase_ == TaskMessageSessionPhase::faulted;
        }

        [[nodiscard]] TaskMessageSessionPhase phase() const noexcept
        {
            return phase_;
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return syscalls_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return syscalls_.pending_completions();
        }

        [[nodiscard]] util::u64 service_id() const noexcept
        {
            return service_id_;
        }

        [[nodiscard]] util::u64 session_handle() const noexcept
        {
            return session_handle_;
        }

        [[nodiscard]] Syscalls& syscalls() noexcept
        {
            return syscalls_;
        }

        [[nodiscard]] const Syscalls& syscalls() const noexcept
        {
            return syscalls_;
        }

        void bind_syscalls(Syscalls syscalls) noexcept
        {
            syscalls_ = syscalls;
            reset_state();
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            syscalls_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool open(util::u64 service_id,
                                tick_type wait_due) noexcept
        {
            return open(TaskMessageSessionOpenView{
                            .service_id = service_id,
                        },
                        wait_due);
        }

        [[nodiscard]] bool open(TaskMessageSessionOpenView open_view,
                                tick_type wait_due) noexcept
        {
            if (!can_begin(TaskMessageSessionPhase::idle)) {
                return false;
            }

            if (!syscalls_.sys_capability_call(open_view.service_id,
                                               task_message_session_open_operation,
                                               open_view.payload,
                                               wait_due)) {
                return false;
            }

            pending_ = PendingAction{
                .action = TaskMessageSessionActionKind::open,
                .service_id = open_view.service_id,
                .session_handle = 0,
                .operation = task_message_session_open_operation,
                .payload = open_view.payload,
            };
            phase_ = TaskMessageSessionPhase::opening;
            return true;
        }

        [[nodiscard]] bool request(util::u64 operation,
                                   util::u64 payload,
                                   tick_type wait_due) noexcept
        {
            return request(TaskMessageSessionRequestView{
                               .operation = operation,
                               .payload = payload,
                           },
                           wait_due);
        }

        [[nodiscard]] bool request(TaskMessageSessionRequestView request_view,
                                   tick_type wait_due) noexcept
        {
            if (is_task_message_session_control_operation(
                    request_view.operation) ||
                !can_begin(TaskMessageSessionPhase::open)) {
                return false;
            }

            if (!syscalls_.sys_capability_call(session_handle_,
                                               request_view.operation,
                                               request_view.payload,
                                               wait_due)) {
                return false;
            }

            pending_ = PendingAction{
                .action = TaskMessageSessionActionKind::request,
                .service_id = service_id_,
                .session_handle = session_handle_,
                .operation = request_view.operation,
                .payload = request_view.payload,
            };
            phase_ = TaskMessageSessionPhase::requesting;
            return true;
        }

        [[nodiscard]] bool close(util::u64 reason,
                                 tick_type wait_due) noexcept
        {
            return close(TaskMessageSessionCloseView{
                             .reason = reason,
                         },
                         wait_due);
        }

        [[nodiscard]] bool close(TaskMessageSessionCloseView close_view,
                                 tick_type wait_due) noexcept
        {
            if (!can_begin(TaskMessageSessionPhase::open)) {
                return false;
            }

            if (!syscalls_.sys_capability_call(session_handle_,
                                               task_message_session_close_operation,
                                               close_view.reason,
                                               wait_due)) {
                return false;
            }

            pending_ = PendingAction{
                .action = TaskMessageSessionActionKind::close,
                .service_id = service_id_,
                .session_handle = session_handle_,
                .operation = task_message_session_close_operation,
                .payload = close_view.reason,
            };
            phase_ = TaskMessageSessionPhase::closing;
            return true;
        }

        [[nodiscard]] bool reset() noexcept
        {
            if (busy()) {
                return false;
            }

            reset_state();
            return true;
        }

        [[nodiscard]] bool kick() noexcept
        {
            return syscalls_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return syscalls_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            if (pending_.action == TaskMessageSessionActionKind::none) {
                return false;
            }

            syscall_completion_type raw{};
            if (!syscalls_.receive_completion(raw)) {
                return false;
            }

            out = completion_type{
                .action = pending_.action,
                .phase_before = phase_,
                .phase_after = phase_,
                .timeout = raw.timeout,
                .session_opened = false,
                .session_closed = false,
                .session_faulted = false,
                .service_id = pending_.service_id,
                .session_handle = pending_.session_handle,
                .operation = pending_.operation,
                .payload = pending_.payload,
                .reply_value = raw.trap.value,
                .trap = raw.trap,
                .raw = raw,
            };

            switch (pending_.action) {
            case TaskMessageSessionActionKind::open:
                if (!raw.timeout &&
                    raw.trap.disposition == TrapDisposition::handled) {
                    service_id_ = pending_.service_id;
                    session_handle_ = raw.trap.value;
                    phase_ = TaskMessageSessionPhase::open;
                    out.session_handle = session_handle_;
                    out.session_opened = true;
                } else {
                    clear_session();
                    phase_ = TaskMessageSessionPhase::idle;
                    out.session_handle = 0;
                }
                break;
            case TaskMessageSessionActionKind::request:
                phase_ = TaskMessageSessionPhase::open;
                out.session_handle = pending_.session_handle;
                break;
            case TaskMessageSessionActionKind::close:
                if (raw.timeout) {
                    phase_ = TaskMessageSessionPhase::faulted;
                    out.session_faulted = true;
                } else {
                    clear_session();
                    phase_ = TaskMessageSessionPhase::idle;
                    out.session_closed = true;
                }
                out.session_handle = pending_.session_handle;
                break;
            case TaskMessageSessionActionKind::none:
                break;
            }

            out.phase_after = phase_;
            pending_ = PendingAction{};
            return true;
        }

    private:
        struct PendingAction {
            TaskMessageSessionActionKind action{
                TaskMessageSessionActionKind::none};
            util::u64 service_id{0};
            util::u64 session_handle{0};
            util::u64 operation{0};
            util::u64 payload{0};
        };

        [[nodiscard]] bool can_begin(
            TaskMessageSessionPhase required_phase) const noexcept
        {
            return phase_ == required_phase &&
                   pending_.action == TaskMessageSessionActionKind::none &&
                   !syscalls_.busy() && syscalls_.pending_requests() == 0u &&
                   syscalls_.pending_completions() == 0u;
        }

        void clear_session() noexcept
        {
            service_id_ = 0;
            session_handle_ = 0;
        }

        void reset_state() noexcept
        {
            pending_ = PendingAction{};
            clear_session();
            phase_ = TaskMessageSessionPhase::idle;
        }

        Syscalls syscalls_{};
        PendingAction pending_{};
        util::u64 service_id_{0};
        util::u64 session_handle_{0};
        TaskMessageSessionPhase phase_{TaskMessageSessionPhase::idle};
    };

    template <typename Syscalls>
    [[nodiscard]] auto make_task_message_session_api(
        Syscalls syscalls) noexcept -> TaskMessageSessionApi<Syscalls>
    {
        return TaskMessageSessionApi<Syscalls>{syscalls};
    }
}
