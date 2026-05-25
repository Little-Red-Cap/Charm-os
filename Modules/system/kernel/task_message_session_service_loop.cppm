module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_service_loop;

export import kernel.task_message_session_roundtrip;
import semantic.core;
import util.core;

export namespace kernel {
    namespace session_service_loop_detail {
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

    struct TaskMessageSessionServiceLoopWitness {
        bool ready{false};
        bool has_bootstrap{false};
        bool has_timeout{false};
        bool has_open_dispatch{false};
        bool has_open_service{false};
        bool has_roundtrip{false};
        bool has_close_dispatch{false};
        bool has_close_service{false};
        bool has_ghost_dispatch{false};
        bool has_ghost_service{false};
        bool handoff_ready{false};
        bool ownership_path{false};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        TaskMessageSessionServiceWitness bootstrap{};
        TaskMessageSessionServiceWitness timeout{};
        TaskMessageSessionDispatchWitness open_dispatch{};
        TaskMessageSessionServiceWitness open_service{};
        TaskMessageSessionRoundtripWitness roundtrip{};
        TaskMessageSessionDispatchWitness close_dispatch{};
        TaskMessageSessionServiceWitness close_service{};
        TaskMessageSessionDispatchWitness ghost_dispatch{};
        TaskMessageSessionServiceWitness ghost_service{};

        [[nodiscard]] constexpr bool bootstrap_path_ok() const noexcept
        {
            return has_bootstrap &&
                   bootstrap.reason ==
                       TaskMessageServicePumpReason::bootstrap &&
                   bootstrap.progressed &&
                   bootstrap.bootstrap_consumed &&
                   bootstrap.wait_armed &&
                   !bootstrap.hold_ready &&
                   bootstrap.active_sessions == 0u &&
                   bootstrap.active_channels == 0u &&
                   !bootstrap.dispatch_accepted &&
                   !bootstrap.dispatch_handled &&
                   !bootstrap.dispatch_replied;
        }

        [[nodiscard]] constexpr bool timeout_path_ok() const noexcept
        {
            return has_timeout &&
                   timeout.reason ==
                       TaskMessageServicePumpReason::timeout &&
                   timeout.progressed &&
                   !timeout.bootstrap_consumed &&
                   timeout.wait_armed &&
                   !timeout.hold_ready &&
                   timeout.active_sessions == 0u &&
                   timeout.active_channels == 0u &&
                   !timeout.dispatch_accepted &&
                   !timeout.dispatch_handled &&
                   !timeout.dispatch_replied;
        }

        [[nodiscard]] constexpr bool open_path_ok() const noexcept
        {
            return has_open_dispatch &&
                   has_open_service &&
                   open_dispatch.action ==
                       TaskMessageSessionActionKind::open &&
                   open_dispatch.matched &&
                   open_dispatch.handler_valid &&
                   open_dispatch.session_allocated &&
                   !open_dispatch.session_closed &&
                   open_dispatch.service_id == service_id &&
                   open_dispatch.session_handle == session_handle &&
                   open_service.dispatch_accepted &&
                   open_service.dispatch_handled &&
                   open_service.dispatch_replied &&
                   open_service.active_sessions > 0u &&
                   open_service.active_channels > 0u &&
                   open_service.reply_value == open_dispatch.value;
        }

        [[nodiscard]] constexpr bool roundtrip_path_ok() const noexcept
        {
            return has_roundtrip &&
                   roundtrip.ok() &&
                   roundtrip.service_id == service_id &&
                   roundtrip.session_handle == session_handle;
        }

        [[nodiscard]] constexpr bool close_path_ok() const noexcept
        {
            return has_close_dispatch &&
                   has_close_service &&
                   close_dispatch.action ==
                       TaskMessageSessionActionKind::close &&
                   close_dispatch.matched &&
                   close_dispatch.handler_valid &&
                   close_dispatch.session_found &&
                   close_dispatch.session_closed &&
                   close_dispatch.service_id == service_id &&
                   close_dispatch.session_handle == session_handle &&
                   close_service.dispatch_accepted &&
                   close_service.dispatch_handled &&
                   close_service.dispatch_replied &&
                   close_service.active_sessions == 0u &&
                   close_service.active_channels == 0u &&
                   close_service.reply_value == close_dispatch.value;
        }

        [[nodiscard]] constexpr bool ghost_path_ok() const noexcept
        {
            return has_ghost_dispatch &&
                   has_ghost_service &&
                   ghost_dispatch.action ==
                       TaskMessageSessionActionKind::open &&
                   !ghost_dispatch.matched &&
                   !ghost_dispatch.handler_valid &&
                   !ghost_dispatch.session_found &&
                   !ghost_dispatch.session_allocated &&
                   !ghost_dispatch.session_closed &&
                   ghost_dispatch.session_handle == 0u &&
                   ghost_service.dispatch_accepted &&
                   ghost_service.dispatch_handled &&
                   ghost_service.dispatch_replied &&
                   ghost_service.active_sessions == 0u &&
                   ghost_service.active_channels == 0u &&
                   ghost_service.reply_value == ghost_dispatch.value;
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

            if (bootstrap.verdict() == semantic::Verdict::collapsed ||
                timeout.verdict() == semantic::Verdict::collapsed ||
                open_dispatch.verdict() == semantic::Verdict::collapsed ||
                open_service.verdict() == semantic::Verdict::collapsed ||
                roundtrip.verdict() == semantic::Verdict::collapsed ||
                close_dispatch.verdict() == semantic::Verdict::collapsed ||
                close_service.verdict() == semantic::Verdict::collapsed ||
                ghost_dispatch.verdict() == semantic::Verdict::collapsed ||
                ghost_service.verdict() == semantic::Verdict::collapsed) {
                return semantic::Verdict::collapsed;
            }

            if (bootstrap.verdict() != semantic::Verdict::standing ||
                timeout.verdict() != semantic::Verdict::standing ||
                open_dispatch.verdict() != semantic::Verdict::standing ||
                open_service.verdict() != semantic::Verdict::standing ||
                roundtrip.verdict() != semantic::Verdict::standing ||
                close_dispatch.verdict() != semantic::Verdict::standing ||
                close_service.verdict() != semantic::Verdict::standing ||
                ghost_dispatch.verdict() != semantic::Verdict::standing ||
                ghost_service.verdict() != semantic::Verdict::standing) {
                return semantic::Verdict::drifted;
            }

            if (!ownership_path || !handoff_ready) {
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

            if (bootstrap.verdict() != semantic::Verdict::standing) {
                return bootstrap.failure_domain();
            }

            if (timeout.verdict() != semantic::Verdict::standing) {
                return timeout.failure_domain();
            }

            if (open_dispatch.verdict() != semantic::Verdict::standing) {
                return open_dispatch.failure_domain();
            }

            if (open_service.verdict() != semantic::Verdict::standing) {
                return open_service.failure_domain();
            }

            if (roundtrip.verdict() != semantic::Verdict::standing) {
                return roundtrip.failure_domain();
            }

            if (close_dispatch.verdict() != semantic::Verdict::standing) {
                return close_dispatch.failure_domain();
            }

            if (close_service.verdict() != semantic::Verdict::standing) {
                return close_service.failure_domain();
            }

            if (ghost_dispatch.verdict() != semantic::Verdict::standing) {
                return ghost_dispatch.failure_domain();
            }

            if (ghost_service.verdict() != semantic::Verdict::standing) {
                return ghost_service.failure_domain();
            }

            if (!ownership_path) {
                return semantic::FailureDomain::route;
            }

            if (!handoff_ready) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-service-loop-witness.summary";
        }
    };

    struct TaskMessageSessionServiceLoopWitnessHandoffTarget {
        const TaskMessageSessionServiceLoopWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-service-loop-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-service-loop-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionServiceLoopWitness>(
            std::array<std::string_view, 23>{
                "ready",
                "has_bootstrap",
                "has_timeout",
                "has_open_dispatch",
                "has_open_service",
                "has_roundtrip",
                "has_close_dispatch",
                "has_close_service",
                "has_ghost_dispatch",
                "has_ghost_service",
                "handoff_ready",
                "ownership_path",
                "service_id",
                "session_handle",
                "bootstrap",
                "timeout",
                "open_dispatch",
                "open_service",
                "roundtrip",
                "close_dispatch",
                "close_service",
                "ghost_dispatch",
                "ghost_service",
            }));

    static_assert(
        semantic::WitnessCarrier<TaskMessageSessionServiceLoopWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionServiceLoopWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionServiceLoopWitness
    task_message_session_service_loop_witness(
        TaskMessageSessionServiceWitness bootstrap,
        TaskMessageSessionServiceWitness timeout,
        TaskMessageSessionDispatchWitness open_dispatch,
        TaskMessageSessionServiceWitness open_service,
        TaskMessageSessionRoundtripWitness roundtrip,
        TaskMessageSessionDispatchWitness close_dispatch,
        TaskMessageSessionServiceWitness close_service,
        TaskMessageSessionDispatchWitness ghost_dispatch,
        TaskMessageSessionServiceWitness ghost_service) noexcept
    {
        auto witness = TaskMessageSessionServiceLoopWitness{
            .has_bootstrap = task_message_session_service_witness_ready(
                bootstrap),
            .has_timeout = task_message_session_service_witness_ready(
                timeout),
            .has_open_dispatch =
                task_message_session_dispatch_witness_ready(open_dispatch),
            .has_open_service = task_message_session_service_witness_ready(
                open_service),
            .has_roundtrip =
                task_message_session_roundtrip_witness_ready(roundtrip),
            .has_close_dispatch =
                task_message_session_dispatch_witness_ready(close_dispatch),
            .has_close_service = task_message_session_service_witness_ready(
                close_service),
            .has_ghost_dispatch =
                task_message_session_dispatch_witness_ready(ghost_dispatch),
            .has_ghost_service = task_message_session_service_witness_ready(
                ghost_service),
            .service_id = open_dispatch.service_id,
            .session_handle = open_dispatch.session_handle,
            .bootstrap = bootstrap,
            .timeout = timeout,
            .open_dispatch = open_dispatch,
            .open_service = open_service,
            .roundtrip = roundtrip,
            .close_dispatch = close_dispatch,
            .close_service = close_service,
            .ghost_dispatch = ghost_dispatch,
            .ghost_service = ghost_service,
        };

        witness.ready = witness.has_bootstrap &&
                        witness.has_timeout &&
                        witness.has_open_dispatch &&
                        witness.has_open_service &&
                        witness.has_roundtrip &&
                        witness.has_close_dispatch &&
                        witness.has_close_service &&
                        witness.has_ghost_dispatch &&
                        witness.has_ghost_service;

        const auto bootstrap_handoff =
            task_message_session_service_witness_handoff_target(
                witness.bootstrap);
        const auto timeout_handoff =
            task_message_session_service_witness_handoff_target(
                witness.timeout);
        const auto open_dispatch_handoff =
            task_message_session_dispatch_witness_handoff_target(
                witness.open_dispatch);
        const auto open_service_handoff =
            task_message_session_service_witness_handoff_target(
                witness.open_service);
        const auto roundtrip_handoff =
            task_message_session_roundtrip_witness_handoff_target(
                witness.roundtrip);
        const auto close_dispatch_handoff =
            task_message_session_dispatch_witness_handoff_target(
                witness.close_dispatch);
        const auto close_service_handoff =
            task_message_session_service_witness_handoff_target(
                witness.close_service);
        const auto ghost_dispatch_handoff =
            task_message_session_dispatch_witness_handoff_target(
                witness.ghost_dispatch);
        const auto ghost_service_handoff =
            task_message_session_service_witness_handoff_target(
                witness.ghost_service);
        witness.handoff_ready =
            session_service_loop_detail::handoff_target_ready(
                bootstrap_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                timeout_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                open_dispatch_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                open_service_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                roundtrip_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                close_dispatch_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                close_service_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                ghost_dispatch_handoff) &&
            session_service_loop_detail::handoff_target_ready(
                ghost_service_handoff);
        witness.ownership_path = witness.bootstrap_path_ok() &&
                                  witness.timeout_path_ok() &&
                                  witness.open_path_ok() &&
                                  witness.roundtrip_path_ok() &&
                                  witness.close_path_ok() &&
                                  witness.ghost_path_ok();
        return witness;
    }

    template <std::size_t DispatchCapacity,
              std::size_t AcceptorCapacity,
              std::size_t ProtocolCapacity,
              std::size_t ServiceCapacity,
              std::size_t PumpCapacity>
    [[nodiscard]] constexpr TaskMessageSessionServiceLoopWitness
    task_message_session_service_loop_witness(
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
        const auto roundtrip = task_message_session_roundtrip_witness(
            dispatch_trace,
            acceptor_trace,
            protocol_trace,
            service_trace,
            pump_trace);

        const auto* bootstrap_event =
            session_service_loop_detail::find_last_trace_event_if(
            service_trace,
            [](const TaskMessageSessionServiceTraceEvent& event) noexcept {
                return event.reason ==
                           TaskMessageServicePumpReason::bootstrap &&
                       event.bootstrap_consumed;
            });
        const auto* timeout_event =
            session_service_loop_detail::find_last_trace_event_if(
            service_trace,
            [](const TaskMessageSessionServiceTraceEvent& event) noexcept {
                return event.reason == TaskMessageServicePumpReason::timeout;
            });
        const auto* open_dispatch_event =
            session_service_loop_detail::find_last_trace_event_if(
            dispatch_trace,
            [](const TaskMessageSessionDispatchTraceEvent& event) noexcept {
                return event.action == TaskMessageSessionActionKind::open &&
                       event.session_allocated;
            });
        const auto* close_dispatch_event =
            session_service_loop_detail::find_last_trace_event_if(
            dispatch_trace,
            [](const TaskMessageSessionDispatchTraceEvent& event) noexcept {
                return event.action == TaskMessageSessionActionKind::close &&
                       event.session_closed;
            });
        const auto* ghost_dispatch_event =
            session_service_loop_detail::find_last_trace_event_if(
            dispatch_trace,
            [](const TaskMessageSessionDispatchTraceEvent& event) noexcept {
                return event.action == TaskMessageSessionActionKind::open &&
                       !event.matched &&
                       !event.session_allocated &&
                       !event.session_closed;
            });

        const auto open_dispatch =
            open_dispatch_event != nullptr
                ? task_message_session_dispatch_witness(*open_dispatch_event)
                : TaskMessageSessionDispatchWitness{};
        const auto close_dispatch =
            close_dispatch_event != nullptr
                ? task_message_session_dispatch_witness(*close_dispatch_event)
                : TaskMessageSessionDispatchWitness{};
        const auto ghost_dispatch =
            ghost_dispatch_event != nullptr
                ? task_message_session_dispatch_witness(*ghost_dispatch_event)
                : TaskMessageSessionDispatchWitness{};

        const auto* open_service_event =
            session_service_loop_detail::find_last_trace_event_if(
            service_trace,
            [&](const TaskMessageSessionServiceTraceEvent& event) noexcept {
                return event.dispatch_accepted &&
                       event.dispatch_handled &&
                       event.dispatch_replied &&
                       event.active_sessions > 0u &&
                       event.active_channels > 0u &&
                       event.reply_value == open_dispatch.value;
            });
        const auto* close_service_event =
            session_service_loop_detail::find_last_trace_event_if(
            service_trace,
            [&](const TaskMessageSessionServiceTraceEvent& event) noexcept {
                return event.dispatch_accepted &&
                       event.dispatch_handled &&
                       event.dispatch_replied &&
                       event.active_sessions == 0u &&
                       event.active_channels == 0u &&
                       event.reply_value == close_dispatch.value;
            });
        const auto* ghost_service_event =
            session_service_loop_detail::find_last_trace_event_if(
            service_trace,
            [&](const TaskMessageSessionServiceTraceEvent& event) noexcept {
                return event.dispatch_accepted &&
                       event.dispatch_handled &&
                       event.dispatch_replied &&
                       event.active_sessions == 0u &&
                       event.active_channels == 0u &&
                       event.reply_value == ghost_dispatch.value;
            });

        return task_message_session_service_loop_witness(
            bootstrap_event != nullptr
                ? task_message_session_service_witness(*bootstrap_event)
                : TaskMessageSessionServiceWitness{},
            timeout_event != nullptr
                ? task_message_session_service_witness(*timeout_event)
                : TaskMessageSessionServiceWitness{},
            open_dispatch,
            open_service_event != nullptr
                ? task_message_session_service_witness(*open_service_event)
                : TaskMessageSessionServiceWitness{},
            roundtrip,
            close_dispatch,
            close_service_event != nullptr
                ? task_message_session_service_witness(*close_service_event)
                : TaskMessageSessionServiceWitness{},
            ghost_dispatch,
            ghost_service_event != nullptr
                ? task_message_session_service_witness(*ghost_service_event)
                : TaskMessageSessionServiceWitness{});
    }

    [[nodiscard]] constexpr bool
    task_message_session_service_loop_witness_ready(
        const TaskMessageSessionServiceLoopWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionServiceLoopWitnessHandoffTarget
    task_message_session_service_loop_witness_handoff_target(
        const TaskMessageSessionServiceLoopWitness& witness) noexcept
    {
        return TaskMessageSessionServiceLoopWitnessHandoffTarget{
            .witness = &witness,
        };
    }
}
