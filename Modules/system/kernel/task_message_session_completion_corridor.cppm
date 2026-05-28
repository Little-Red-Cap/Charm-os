module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_completion_corridor;

export import kernel.task_message_session_ownership_corridor;
import semantic.core;
import util.core;

export namespace kernel {
    namespace session_completion_corridor_detail {
        template <typename Target>
        requires semantic::HandoffTarget<Target>
        [[nodiscard]] constexpr bool handoff_target_ready(
            const Target& target) noexcept
        {
            return !target.entry_name().empty() &&
                   !target.selected_summary_path().empty();
        }

        [[nodiscard]] constexpr bool trap_matches(
            const TaskMessageSessionApiWitness& witness,
            TrapDisposition disposition,
            TrapError error,
            util::u64 value) noexcept
        {
            return witness.disposition == disposition && witness.error == error &&
                   witness.reply_value == value;
        }
    }

    struct TaskMessageSessionCompletionCorridorWitness {
        bool ready{false};
        bool has_open{false};
        bool has_request{false};
        bool has_close{false};
        bool has_ghost_open{false};
        bool has_ownership_corridor{false};
        bool handoff_ready{false};
        bool action_path{false};
        bool phase_path{false};
        bool completion_branch_path{false};
        bool identity_path{false};
        bool token_sequence_path{false};
        bool request_payload_path{false};
        bool lifecycle_path{false};
        TaskId owner{};
        TaskId reply_from{};
        TaskId reply_to{};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 request_operation{0};
        util::u64 open_payload{0};
        util::u64 request_payload{0};
        util::u64 close_payload{0};
        util::u64 ghost_open_payload{0};
        util::u64 open_reply_value{0};
        util::u64 request_reply_value{0};
        util::u64 close_reply_value{0};
        util::u64 ghost_open_reply_value{0};
        TaskMessageSessionApiWitness open{};
        TaskMessageSessionApiWitness request{};
        TaskMessageSessionApiWitness close{};
        TaskMessageSessionApiWitness ghost_open{};
        TaskMessageSessionOwnershipCorridorWitness ownership_corridor{};

        [[nodiscard]] constexpr bool action_path_ok() const noexcept
        {
            return has_open &&
                   has_request &&
                   has_close &&
                   has_ghost_open &&
                   open.action == TaskMessageSessionActionKind::open &&
                   request.action == TaskMessageSessionActionKind::request &&
                   close.action == TaskMessageSessionActionKind::close &&
                   ghost_open.action == TaskMessageSessionActionKind::open;
        }

        [[nodiscard]] constexpr bool phase_path_ok() const noexcept
        {
            return open.phase_before == TaskMessageSessionPhase::opening &&
                   open.phase_after == TaskMessageSessionPhase::open &&
                   request.phase_before ==
                       TaskMessageSessionPhase::requesting &&
                   request.phase_after == TaskMessageSessionPhase::open &&
                   close.phase_before == TaskMessageSessionPhase::closing &&
                   close.phase_after == TaskMessageSessionPhase::idle &&
                   ghost_open.phase_before ==
                       TaskMessageSessionPhase::opening &&
                   ghost_open.phase_after == TaskMessageSessionPhase::idle;
        }

        [[nodiscard]] constexpr bool completion_branch_path_ok() const noexcept
        {
            return !open.timeout &&
                   !request.timeout &&
                   !close.timeout &&
                   !ghost_open.timeout &&
                   open.session_opened &&
                   !open.session_closed &&
                   !open.session_faulted &&
                   !request.session_opened &&
                   !request.session_closed &&
                   !request.session_faulted &&
                   !close.session_opened &&
                   close.session_closed &&
                   !close.session_faulted &&
                   !ghost_open.session_opened &&
                   !ghost_open.session_closed &&
                   !ghost_open.session_faulted;
        }

        [[nodiscard]] constexpr bool identity_path_ok() const noexcept
        {
            return owner != TaskId{} &&
                   reply_from != TaskId{} &&
                   reply_to == owner &&
                   open.owner == owner &&
                   request.owner == owner &&
                   close.owner == owner &&
                   ghost_open.owner == owner &&
                   open.reply_from == reply_from &&
                   request.reply_from == reply_from &&
                   close.reply_from == reply_from &&
                   ghost_open.reply_from == reply_from &&
                   open.reply_to == reply_to &&
                   request.reply_to == reply_to &&
                   close.reply_to == reply_to &&
                   ghost_open.reply_to == reply_to;
        }

        [[nodiscard]] constexpr bool token_sequence_path_ok() const noexcept
        {
            return open.token != 0u &&
                   open.request_sequence != 0u &&
                   request.token == open.token + 1u &&
                   close.token == request.token + 1u &&
                   ghost_open.token == close.token + 1u &&
                   request.request_sequence == open.request_sequence + 1u &&
                   close.request_sequence == request.request_sequence + 1u &&
                   ghost_open.request_sequence ==
                       close.request_sequence + 1u &&
                   open.reply_sequence == open.request_sequence &&
                   request.reply_sequence == request.request_sequence &&
                   close.reply_sequence == close.request_sequence &&
                   ghost_open.reply_sequence == ghost_open.request_sequence;
        }

        [[nodiscard]] constexpr bool request_payload_path_ok() const noexcept
        {
            return open.service_id == service_id &&
                   request.service_id == service_id &&
                   close.service_id == service_id &&
                   ghost_open.service_id != service_id &&
                   open.session_handle == session_handle &&
                   request.session_handle == session_handle &&
                   close.session_handle == session_handle &&
                   ghost_open.session_handle == 0u &&
                   open.operation == task_message_session_open_operation &&
                   request.operation == request_operation &&
                   close.operation == task_message_session_close_operation &&
                   ghost_open.operation == task_message_session_open_operation &&
                   open.payload == open_payload &&
                   request.payload == request_payload &&
                   close.payload == close_payload &&
                   ghost_open.payload == ghost_open_payload &&
                   open.reply_value == open_reply_value &&
                   request.reply_value == request_reply_value &&
                   close.reply_value == close_reply_value &&
                   ghost_open.reply_value == ghost_open_reply_value &&
                   session_completion_corridor_detail::trap_matches(
                       open,
                       TrapDisposition::handled,
                       TrapError::none,
                       open_reply_value) &&
                   session_completion_corridor_detail::trap_matches(
                       request,
                       TrapDisposition::handled,
                       TrapError::none,
                       request_reply_value) &&
                   session_completion_corridor_detail::trap_matches(
                       close,
                       TrapDisposition::handled,
                       TrapError::none,
                       close_reply_value) &&
                   session_completion_corridor_detail::trap_matches(
                       ghost_open,
                       TrapDisposition::unsupported,
                       TrapError::unsupported_service,
                       ghost_open_reply_value);
        }

        [[nodiscard]] constexpr bool lifecycle_path_ok() const noexcept
        {
            return has_ownership_corridor &&
                   ownership_corridor.ok() &&
                   ownership_corridor.service_id == service_id &&
                   ownership_corridor.session_handle == session_handle &&
                   ownership_corridor.operation == request_operation &&
                   ownership_corridor.payload == request_payload &&
                   ownership_corridor.open_reply_value == open_reply_value &&
                   ownership_corridor.request_reply_value ==
                       request_reply_value &&
                   ownership_corridor.close_reply_value == close_reply_value;
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

            if (open.verdict() == semantic::Verdict::collapsed ||
                request.verdict() == semantic::Verdict::collapsed ||
                close.verdict() == semantic::Verdict::collapsed ||
                ghost_open.verdict() == semantic::Verdict::collapsed ||
                ownership_corridor.verdict() == semantic::Verdict::collapsed) {
                return semantic::Verdict::collapsed;
            }

            if (open.verdict() != semantic::Verdict::standing ||
                request.verdict() != semantic::Verdict::standing ||
                close.verdict() != semantic::Verdict::standing ||
                ghost_open.verdict() != semantic::Verdict::standing ||
                ownership_corridor.verdict() != semantic::Verdict::standing) {
                return semantic::Verdict::drifted;
            }

            if (!action_path || !phase_path || !completion_branch_path ||
                !identity_path || !token_sequence_path ||
                !request_payload_path || !lifecycle_path ||
                !handoff_ready) {
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

            if (open.verdict() != semantic::Verdict::standing) {
                return open.failure_domain();
            }

            if (request.verdict() != semantic::Verdict::standing) {
                return request.failure_domain();
            }

            if (close.verdict() != semantic::Verdict::standing) {
                return close.failure_domain();
            }

            if (ghost_open.verdict() != semantic::Verdict::standing) {
                return ghost_open.failure_domain();
            }

            if (ownership_corridor.verdict() != semantic::Verdict::standing) {
                return ownership_corridor.failure_domain();
            }

            if (!action_path || !phase_path) {
                return semantic::FailureDomain::selection;
            }

            if (!token_sequence_path || !request_payload_path ||
                !lifecycle_path) {
                return semantic::FailureDomain::route;
            }

            if (!completion_branch_path || !identity_path || !handoff_ready) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-completion-corridor-witness.summary";
        }
    };

    struct TaskMessageSessionCompletionCorridorWitnessHandoffTarget {
        const TaskMessageSessionCompletionCorridorWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-completion-corridor-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr
                       ? witness->summary_path()
                       : std::string_view{
                             "task-message-session-completion-corridor-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionCompletionCorridorWitness>(
            std::array<std::string_view, 33>{
                "ready",
                "has_open",
                "has_request",
                "has_close",
                "has_ghost_open",
                "has_ownership_corridor",
                "handoff_ready",
                "action_path",
                "phase_path",
                "completion_branch_path",
                "identity_path",
                "token_sequence_path",
                "request_payload_path",
                "lifecycle_path",
                "owner",
                "reply_from",
                "reply_to",
                "service_id",
                "session_handle",
                "request_operation",
                "open_payload",
                "request_payload",
                "close_payload",
                "ghost_open_payload",
                "open_reply_value",
                "request_reply_value",
                "close_reply_value",
                "ghost_open_reply_value",
                "open",
                "request",
                "close",
                "ghost_open",
                "ownership_corridor",
            }));

    static_assert(
        semantic::WitnessCarrier<TaskMessageSessionCompletionCorridorWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionCompletionCorridorWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionCompletionCorridorWitness
    task_message_session_completion_corridor_witness(
        TaskMessageSessionApiWitness open,
        TaskMessageSessionApiWitness request,
        TaskMessageSessionApiWitness close,
        TaskMessageSessionApiWitness ghost_open,
        TaskMessageSessionOwnershipCorridorWitness ownership_corridor) noexcept
    {
        auto witness = TaskMessageSessionCompletionCorridorWitness{
            .has_open = task_message_session_api_witness_ready(open),
            .has_request = task_message_session_api_witness_ready(request),
            .has_close = task_message_session_api_witness_ready(close),
            .has_ghost_open = task_message_session_api_witness_ready(ghost_open),
            .has_ownership_corridor =
                task_message_session_ownership_corridor_witness_ready(
                    ownership_corridor),
            .owner = open.owner,
            .reply_from = open.reply_from,
            .reply_to = open.reply_to,
            .service_id = open.service_id,
            .session_handle = open.session_handle,
            .request_operation = request.operation,
            .open_payload = open.payload,
            .request_payload = request.payload,
            .close_payload = close.payload,
            .ghost_open_payload = ghost_open.payload,
            .open_reply_value = open.reply_value,
            .request_reply_value = request.reply_value,
            .close_reply_value = close.reply_value,
            .ghost_open_reply_value = ghost_open.reply_value,
            .open = open,
            .request = request,
            .close = close,
            .ghost_open = ghost_open,
            .ownership_corridor = ownership_corridor,
        };

        witness.ready = witness.has_open && witness.has_request &&
                        witness.has_close && witness.has_ghost_open &&
                        witness.has_ownership_corridor;

        const auto open_handoff =
            task_message_session_api_witness_handoff_target(witness.open);
        const auto request_handoff =
            task_message_session_api_witness_handoff_target(witness.request);
        const auto close_handoff =
            task_message_session_api_witness_handoff_target(witness.close);
        const auto ghost_open_handoff =
            task_message_session_api_witness_handoff_target(
                witness.ghost_open);
        const auto ownership_handoff =
            task_message_session_ownership_corridor_witness_handoff_target(
                witness.ownership_corridor);
        witness.handoff_ready =
            session_completion_corridor_detail::handoff_target_ready(
                open_handoff) &&
            session_completion_corridor_detail::handoff_target_ready(
                request_handoff) &&
            session_completion_corridor_detail::handoff_target_ready(
                close_handoff) &&
            session_completion_corridor_detail::handoff_target_ready(
                ghost_open_handoff) &&
            session_completion_corridor_detail::handoff_target_ready(
                ownership_handoff);
        witness.action_path = witness.action_path_ok();
        witness.phase_path = witness.phase_path_ok();
        witness.completion_branch_path = witness.completion_branch_path_ok();
        witness.identity_path = witness.identity_path_ok();
        witness.token_sequence_path = witness.token_sequence_path_ok();
        witness.request_payload_path = witness.request_payload_path_ok();
        witness.lifecycle_path = witness.lifecycle_path_ok();
        return witness;
    }

    [[nodiscard]] constexpr bool
    task_message_session_completion_corridor_witness_ready(
        const TaskMessageSessionCompletionCorridorWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr
        TaskMessageSessionCompletionCorridorWitnessHandoffTarget
        task_message_session_completion_corridor_witness_handoff_target(
            const TaskMessageSessionCompletionCorridorWitness& witness) noexcept
    {
        return TaskMessageSessionCompletionCorridorWitnessHandoffTarget{
            .witness = &witness,
        };
    }
}
