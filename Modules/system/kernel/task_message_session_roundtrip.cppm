module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_roundtrip;

export import kernel.task_message_session_service;
export import kernel.task_message_session_protocol;
export import kernel.task_message_syscall_pump;
import semantic.core;
import util.core;

export namespace kernel {
    namespace detail {
        template <typename Trace, typename Predicate>
        [[nodiscard]] constexpr const typename Trace::value_type*
        find_last_trace_event_if(const Trace& trace,
                                 Predicate predicate) noexcept
        {
            for (std::size_t remaining = trace.size(); remaining > 0u;
                 --remaining) {
                const auto* event = trace.at(remaining - 1u);
                if (event != nullptr && predicate(*event)) {
                    return event;
                }
            }

            return nullptr;
        }

        template <typename Target>
        requires semantic::HandoffTarget<Target>
        [[nodiscard]] constexpr bool handoff_target_ready(
            const Target& target) noexcept
        {
            return !target.entry_name().empty() &&
                   !target.selected_summary_path().empty();
        }
    }

    struct TaskMessageSessionRoundtripWitness {
        bool ready{false};
        bool has_dispatch{false};
        bool has_acceptor{false};
        bool has_protocol{false};
        bool has_service{false};
        bool has_pump{false};
        bool handoff_ready{false};
        bool request_path{false};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 reply_value{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        TaskMessageSessionDispatchWitness dispatch{};
        TaskMessageSessionServiceAcceptorWitness acceptor{};
        TaskMessageSessionProtocolWitness protocol{};
        TaskMessageSessionServiceWitness service{};
        TaskMessageSyscallPumpWitness pump{};

        [[nodiscard]] constexpr bool request_branch_ok() const noexcept
        {
            return has_dispatch &&
                   has_acceptor &&
                   has_protocol &&
                   has_service &&
                   has_pump &&
                   dispatch.action == TaskMessageSessionActionKind::request &&
                   acceptor.action == TaskMessageSessionActionKind::request &&
                   protocol.kind ==
                       TaskMessageSessionProtocolTraceKind::request &&
                   service.dispatch_accepted &&
                   service.dispatch_handled &&
                   service.dispatch_replied &&
                   pump.kind == TaskMessageSyscallPumpTraceKind::reply &&
                   dispatch.service_id == service_id &&
                   acceptor.service_id == service_id &&
                   protocol.service_id == service_id &&
                   dispatch.session_handle == session_handle &&
                   acceptor.session_handle == session_handle &&
                   protocol.session_handle == session_handle &&
                   dispatch.operation == operation &&
                   acceptor.operation == operation &&
                   protocol.operation == operation &&
                   dispatch.payload == payload &&
                   acceptor.payload == payload &&
                   protocol.payload == payload &&
                   acceptor.channel_slot == channel_slot &&
                   protocol.channel_slot == channel_slot &&
                   dispatch.value == reply_value &&
                   acceptor.value == reply_value &&
                   protocol.value == reply_value &&
                   service.reply_value == reply_value &&
                   pump.reply_value == reply_value;
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

            if (dispatch.verdict() == semantic::Verdict::collapsed ||
                acceptor.verdict() == semantic::Verdict::collapsed ||
                protocol.verdict() == semantic::Verdict::collapsed ||
                service.verdict() == semantic::Verdict::collapsed ||
                pump.verdict() == semantic::Verdict::collapsed) {
                return semantic::Verdict::collapsed;
            }

            if (dispatch.verdict() != semantic::Verdict::standing ||
                acceptor.verdict() != semantic::Verdict::standing ||
                protocol.verdict() != semantic::Verdict::standing ||
                service.verdict() != semantic::Verdict::standing ||
                pump.verdict() != semantic::Verdict::standing) {
                return semantic::Verdict::drifted;
            }

            if (!request_path || !handoff_ready) {
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

            if (dispatch.verdict() != semantic::Verdict::standing) {
                return dispatch.failure_domain();
            }

            if (acceptor.verdict() != semantic::Verdict::standing) {
                return acceptor.failure_domain();
            }

            if (protocol.verdict() != semantic::Verdict::standing) {
                return protocol.failure_domain();
            }

            if (service.verdict() != semantic::Verdict::standing) {
                return service.failure_domain();
            }

            if (pump.verdict() != semantic::Verdict::standing) {
                return pump.failure_domain();
            }

            if (!request_path) {
                return semantic::FailureDomain::route;
            }

            if (!handoff_ready) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-roundtrip-witness.summary";
        }
    };

    struct TaskMessageSessionRoundtripWitnessHandoffTarget {
        const TaskMessageSessionRoundtripWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-roundtrip-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-roundtrip-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionRoundtripWitness>(
            std::array<std::string_view, 19>{
                "ready",
                "has_dispatch",
                "has_acceptor",
                "has_protocol",
                "has_service",
                "has_pump",
                "handoff_ready",
                "request_path",
                "service_id",
                "session_handle",
                "operation",
                "payload",
                "reply_value",
                "channel_slot",
                "dispatch",
                "acceptor",
                "protocol",
                "service",
                "pump",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSessionRoundtripWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSessionRoundtripWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionRoundtripWitness
    task_message_session_roundtrip_witness(
        TaskMessageSessionDispatchWitness dispatch,
        TaskMessageSessionServiceAcceptorWitness acceptor,
        TaskMessageSessionProtocolWitness protocol,
        TaskMessageSessionServiceWitness service,
        TaskMessageSyscallPumpWitness pump) noexcept
    {
        auto witness = TaskMessageSessionRoundtripWitness{
            .has_dispatch = task_message_session_dispatch_witness_ready(
                dispatch),
            .has_acceptor =
                task_message_session_service_acceptor_witness_ready(
                    acceptor),
            .has_protocol = task_message_session_protocol_witness_ready(
                protocol),
            .has_service = task_message_session_service_witness_ready(service),
            .has_pump = task_message_syscall_pump_witness_ready(pump),
            .service_id = dispatch.service_id,
            .session_handle = dispatch.session_handle,
            .operation = dispatch.operation,
            .payload = dispatch.payload,
            .reply_value = dispatch.value,
            .channel_slot = acceptor.channel_slot,
            .dispatch = dispatch,
            .acceptor = acceptor,
            .protocol = protocol,
            .service = service,
            .pump = pump,
        };

        witness.ready = witness.has_dispatch &&
                        witness.has_acceptor &&
                        witness.has_protocol &&
                        witness.has_service &&
                        witness.has_pump;

        const auto dispatch_handoff =
            task_message_session_dispatch_witness_handoff_target(
                witness.dispatch);
        const auto acceptor_handoff =
            task_message_session_service_acceptor_witness_handoff_target(
                witness.acceptor);
        const auto protocol_handoff =
            task_message_session_protocol_witness_handoff_target(
                witness.protocol);
        const auto service_handoff =
            task_message_session_service_witness_handoff_target(
                witness.service);
        const auto pump_handoff =
            task_message_syscall_pump_witness_handoff_target(witness.pump);
        witness.handoff_ready =
            detail::handoff_target_ready(dispatch_handoff) &&
            detail::handoff_target_ready(acceptor_handoff) &&
            detail::handoff_target_ready(protocol_handoff) &&
            detail::handoff_target_ready(service_handoff) &&
            detail::handoff_target_ready(pump_handoff);
        witness.request_path = witness.request_branch_ok();
        return witness;
    }

    template <std::size_t DispatchCapacity,
              std::size_t AcceptorCapacity,
              std::size_t ProtocolCapacity,
              std::size_t ServiceCapacity,
              std::size_t PumpCapacity>
    [[nodiscard]] constexpr TaskMessageSessionRoundtripWitness
    task_message_session_roundtrip_witness(
        const TaskMessageSessionDispatchTraceBuffer<DispatchCapacity>&
            dispatch_trace,
        const TaskMessageSessionServiceAcceptorTraceBuffer<AcceptorCapacity>&
            acceptor_trace,
        const TaskMessageSessionProtocolTraceBuffer<ProtocolCapacity>&
            protocol_trace,
        const TaskMessageSessionServiceTraceBuffer<ServiceCapacity>&
            service_trace,
        const TaskMessageSyscallPumpTraceBuffer<PumpCapacity>&
            pump_trace) noexcept
    {
        const auto* dispatch_event = detail::find_last_trace_event_if(
            dispatch_trace,
            [](const TaskMessageSessionDispatchTraceEvent& event) noexcept {
                return event.action == TaskMessageSessionActionKind::request;
            });
        if (dispatch_event == nullptr) {
            return TaskMessageSessionRoundtripWitness{};
        }

        const auto dispatch =
            task_message_session_dispatch_witness(*dispatch_event);

        const auto* acceptor_event = detail::find_last_trace_event_if(
            acceptor_trace,
            [&](const TaskMessageSessionServiceAcceptorTraceEvent& event)
                noexcept {
                    return event.action ==
                               TaskMessageSessionActionKind::request &&
                           event.service_id == dispatch.service_id &&
                           event.session_handle == dispatch.session_handle &&
                           event.operation == dispatch.operation &&
                           event.payload == dispatch.payload;
                });
        const auto* protocol_event = detail::find_last_trace_event_if(
            protocol_trace,
            [&](const TaskMessageSessionProtocolTraceEvent& event) noexcept {
                return event.kind ==
                           TaskMessageSessionProtocolTraceKind::request &&
                       event.service_id == dispatch.service_id &&
                       event.session_handle == dispatch.session_handle &&
                       event.operation == dispatch.operation &&
                       event.payload == dispatch.payload;
            });
        const auto* service_event = detail::find_last_trace_event_if(
            service_trace,
            [&](const TaskMessageSessionServiceTraceEvent& event) noexcept {
                return event.dispatch_accepted &&
                       event.dispatch_handled &&
                       event.dispatch_replied &&
                       event.reply_value == dispatch.value;
            });
        const auto* pump_event = detail::find_last_trace_event_if(
            pump_trace,
            [&](const TaskMessageSyscallPumpTraceEvent& event) noexcept {
                return event.kind == TaskMessageSyscallPumpTraceKind::reply &&
                       event.reply_value == dispatch.value;
            });

        return task_message_session_roundtrip_witness(
            dispatch,
            acceptor_event != nullptr
                ? task_message_session_service_acceptor_witness(
                      *acceptor_event)
                : TaskMessageSessionServiceAcceptorWitness{},
            protocol_event != nullptr
                ? task_message_session_protocol_witness(*protocol_event)
                : TaskMessageSessionProtocolWitness{},
            service_event != nullptr
                ? task_message_session_service_witness(*service_event)
                : TaskMessageSessionServiceWitness{},
            pump_event != nullptr
                ? task_message_syscall_pump_witness(*pump_event)
                : TaskMessageSyscallPumpWitness{});
    }

    [[nodiscard]] constexpr bool task_message_session_roundtrip_witness_ready(
        const TaskMessageSessionRoundtripWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionRoundtripWitnessHandoffTarget
    task_message_session_roundtrip_witness_handoff_target(
        const TaskMessageSessionRoundtripWitness& witness) noexcept
    {
        return TaskMessageSessionRoundtripWitnessHandoffTarget{
            .witness = &witness,
        };
    }
}
