module;

#include <array>
#include <string_view>

export module kernel.task_message_session_ownership_corridor;

export import kernel.task_message_session_service_loop;
import semantic.core;
import util.core;

export namespace kernel {
    namespace session_ownership_corridor_detail {
        template <typename Target>
        requires semantic::HandoffTarget<Target>
        [[nodiscard]] constexpr bool handoff_target_ready(
            const Target& target) noexcept
        {
            return !target.entry_name().empty() &&
                   !target.selected_summary_path().empty();
        }
    }

    struct TaskMessageSessionOwnershipCorridorWitness {
        bool ready{false};
        bool has_roundtrip{false};
        bool has_service_loop{false};
        bool handoff_ready{false};
        bool client_continuity_path{false};
        bool server_ownership_path{false};
        bool shared_roundtrip_path{false};
        bool lifecycle_path{false};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 open_reply_value{0};
        util::u64 request_reply_value{0};
        util::u64 close_reply_value{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        TaskMessageSessionRoundtripWitness roundtrip{};
        TaskMessageSessionServiceLoopWitness service_loop{};

        [[nodiscard]] constexpr bool shared_roundtrip_ok() const noexcept
        {
            return has_roundtrip &&
                   has_service_loop &&
                   task_message_session_roundtrip_witness_ready(
                       service_loop.roundtrip) &&
                   roundtrip.service_id == service_id &&
                   service_loop.service_id == service_id &&
                   service_loop.roundtrip.service_id == service_id &&
                   roundtrip.session_handle == session_handle &&
                   service_loop.session_handle == session_handle &&
                   service_loop.roundtrip.session_handle == session_handle &&
                   roundtrip.operation == operation &&
                   service_loop.roundtrip.operation == operation &&
                   roundtrip.payload == payload &&
                   service_loop.roundtrip.payload == payload &&
                   roundtrip.reply_value == request_reply_value &&
                   service_loop.roundtrip.reply_value ==
                       request_reply_value &&
                   roundtrip.channel_slot == channel_slot &&
                   service_loop.roundtrip.channel_slot == channel_slot;
        }

        [[nodiscard]] constexpr bool lifecycle_path_ok() const noexcept
        {
            return has_service_loop &&
                   service_loop.open_dispatch.service_id == service_id &&
                   service_loop.open_dispatch.session_handle ==
                       session_handle &&
                   service_loop.open_dispatch.value == open_reply_value &&
                   service_loop.open_service.reply_value == open_reply_value &&
                   service_loop.close_dispatch.service_id == service_id &&
                   service_loop.close_dispatch.session_handle ==
                       session_handle &&
                   service_loop.close_dispatch.value == close_reply_value &&
                   service_loop.close_service.reply_value ==
                       close_reply_value &&
                   service_loop.roundtrip.reply_value ==
                       request_reply_value;
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

            if (roundtrip.verdict() == semantic::Verdict::collapsed ||
                service_loop.verdict() == semantic::Verdict::collapsed) {
                return semantic::Verdict::collapsed;
            }

            if (roundtrip.verdict() != semantic::Verdict::standing ||
                service_loop.verdict() != semantic::Verdict::standing) {
                return semantic::Verdict::drifted;
            }

            if (!client_continuity_path || !server_ownership_path ||
                !shared_roundtrip_path || !lifecycle_path ||
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

            if (roundtrip.verdict() != semantic::Verdict::standing) {
                return roundtrip.failure_domain();
            }

            if (service_loop.verdict() != semantic::Verdict::standing) {
                return service_loop.failure_domain();
            }

            if (!shared_roundtrip_path || !lifecycle_path) {
                return semantic::FailureDomain::route;
            }

            if (!handoff_ready) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-ownership-corridor-witness.summary";
        }
    };

    struct TaskMessageSessionOwnershipCorridorWitnessHandoffTarget {
        const TaskMessageSessionOwnershipCorridorWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-ownership-corridor-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-ownership-corridor-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionOwnershipCorridorWitness>(
            std::array<std::string_view, 18>{
                "ready",
                "has_roundtrip",
                "has_service_loop",
                "handoff_ready",
                "client_continuity_path",
                "server_ownership_path",
                "shared_roundtrip_path",
                "lifecycle_path",
                "service_id",
                "session_handle",
                "operation",
                "payload",
                "open_reply_value",
                "request_reply_value",
                "close_reply_value",
                "channel_slot",
                "roundtrip",
                "service_loop",
            }));

    static_assert(
        semantic::WitnessCarrier<TaskMessageSessionOwnershipCorridorWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionOwnershipCorridorWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionOwnershipCorridorWitness
    task_message_session_ownership_corridor_witness(
        TaskMessageSessionRoundtripWitness roundtrip,
        TaskMessageSessionServiceLoopWitness service_loop) noexcept
    {
        auto witness = TaskMessageSessionOwnershipCorridorWitness{
            .has_roundtrip =
                task_message_session_roundtrip_witness_ready(roundtrip),
            .has_service_loop =
                task_message_session_service_loop_witness_ready(service_loop),
            .client_continuity_path = roundtrip.request_path,
            .server_ownership_path = service_loop.ownership_path,
            .service_id =
                roundtrip.service_id != 0u ? roundtrip.service_id
                                           : service_loop.service_id,
            .session_handle =
                roundtrip.session_handle != 0u ? roundtrip.session_handle
                                               : service_loop.session_handle,
            .operation = roundtrip.operation,
            .payload = roundtrip.payload,
            .open_reply_value = service_loop.open_dispatch.value,
            .request_reply_value = roundtrip.reply_value,
            .close_reply_value = service_loop.close_dispatch.value,
            .channel_slot = roundtrip.channel_slot,
            .roundtrip = roundtrip,
            .service_loop = service_loop,
        };

        witness.ready = witness.has_roundtrip && witness.has_service_loop;

        const auto roundtrip_handoff =
            task_message_session_roundtrip_witness_handoff_target(
                witness.roundtrip);
        const auto service_loop_handoff =
            task_message_session_service_loop_witness_handoff_target(
                witness.service_loop);
        witness.handoff_ready =
            session_ownership_corridor_detail::handoff_target_ready(
                roundtrip_handoff) &&
            session_ownership_corridor_detail::handoff_target_ready(
                service_loop_handoff);
        witness.shared_roundtrip_path = witness.shared_roundtrip_ok();
        witness.lifecycle_path = witness.lifecycle_path_ok();
        return witness;
    }

    [[nodiscard]] constexpr TaskMessageSessionOwnershipCorridorWitness
    task_message_session_ownership_corridor_witness(
        TaskMessageSessionServiceLoopWitness service_loop) noexcept
    {
        return task_message_session_ownership_corridor_witness(
            service_loop.roundtrip,
            service_loop);
    }

    [[nodiscard]] constexpr bool
    task_message_session_ownership_corridor_witness_ready(
        const TaskMessageSessionOwnershipCorridorWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr
        TaskMessageSessionOwnershipCorridorWitnessHandoffTarget
        task_message_session_ownership_corridor_witness_handoff_target(
            const TaskMessageSessionOwnershipCorridorWitness& witness) noexcept
    {
        return TaskMessageSessionOwnershipCorridorWitnessHandoffTarget{
            .witness = &witness,
        };
    }
}
