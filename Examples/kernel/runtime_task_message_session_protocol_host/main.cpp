#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_session_protocol;
import semantic.core;

namespace demo {
    using namespace std::literals;

    inline constexpr std::uint64_t kServiceId{0x51u};
    inline constexpr std::uint64_t kBaseSessionHandle{0x9000u};
    inline constexpr std::uint64_t kOpenPayload{0xAAu};
    inline constexpr std::uint64_t kSecondOpenPayload{0xBBu};
    inline constexpr std::uint64_t kEchoOperation{0x21u};
    inline constexpr std::uint64_t kStatusOperation{0x22u};
    inline constexpr std::uint64_t kBrokenOperation{0x33u};
    inline constexpr std::uint64_t kUnsupportedOperation{0x44u};
    inline constexpr std::uint64_t kEchoPayload{33u};
    inline constexpr std::uint64_t kStatusPayload{7u};
    inline constexpr std::uint64_t kUnsupportedPayload{9u};
    inline constexpr std::uint64_t kCloseReason{0x77u};

    struct RequestOpState {
        std::uint32_t calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint64_t last_operation{0};
        std::uint64_t last_payload{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
    };

    struct CloseState {
        std::uint32_t calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint64_t last_reason{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
    };

    struct EchoOperation {
        RequestOpState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskMessageSessionEndpointRequestView request) noexcept
        {
            ++state->calls;
            state->last_service_id = request.endpoint.service_id;
            state->last_session_handle = request.endpoint.session_handle;
            state->last_open_payload = request.endpoint.open_payload;
            state->last_operation = request.operation;
            state->last_payload = request.payload;
            state->last_channel_slot = request.endpoint.channel_slot;
            return kernel::task_message_session_endpoint_handled(
                request.endpoint.session_handle +
                request.operation +
                request.payload +
                request.endpoint.channel_slot);
        }
    };

    struct StatusOperation {
        RequestOpState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskMessageSessionEndpointRequestView request) noexcept
        {
            ++state->calls;
            state->last_service_id = request.endpoint.service_id;
            state->last_session_handle = request.endpoint.session_handle;
            state->last_open_payload = request.endpoint.open_payload;
            state->last_operation = request.operation;
            state->last_payload = request.payload;
            state->last_channel_slot = request.endpoint.channel_slot;
            return kernel::task_message_session_endpoint_handled(
                request.endpoint.service_id +
                request.endpoint.open_payload +
                request.payload +
                request.endpoint.channel_slot);
        }
    };

    struct CloseHook {
        CloseState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskMessageSessionEndpointCloseView close) noexcept
        {
            ++state->calls;
            state->last_service_id = close.endpoint.service_id;
            state->last_session_handle = close.endpoint.session_handle;
            state->last_open_payload = close.endpoint.open_payload;
            state->last_reason = close.reason;
            state->last_channel_slot = close.endpoint.channel_slot;
            return kernel::task_message_session_endpoint_handled(
                close.reason + close.endpoint.channel_slot + 1u);
        }
    };

    using ProtocolTrace = kernel::TaskMessageSessionProtocolTraceBuffer<8>;
    using AcceptorTrace = kernel::TaskMessageSessionServiceAcceptorTraceBuffer<8>;
    using DispatchTrace = kernel::TaskMessageSessionDispatchTraceBuffer<8>;
    using Protocol = kernel::TaskMessageSessionProtocol<2, ProtocolTrace>;

    struct ProtocolAcceptorState {
        std::uint32_t accept_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
        bool last_binding_valid{false};
    };

    struct ProtocolAcceptor {
        ProtocolAcceptorState* state{nullptr};
        std::array<Protocol, 2>* protocols{nullptr};

        [[nodiscard]] kernel::TrapResult accept(
            kernel::TaskMessageSessionEndpoint endpoint,
            kernel::TaskMessageSessionEndpointBinding binding) noexcept
        {
            ++state->accept_calls;
            state->last_service_id = endpoint.service_id;
            state->last_session_handle = endpoint.session_handle;
            state->last_open_payload = endpoint.open_payload;
            state->last_channel_slot = endpoint.channel_slot;
            state->last_binding_valid = binding.valid();

            if (!binding.valid() || protocols == nullptr ||
                endpoint.channel_slot >= protocols->size()) {
                return kernel::task_message_session_endpoint_invalid_argument();
            }

            kernel::bind_task_message_session_endpoint(
                binding,
                (*protocols)[endpoint.channel_slot]);
            return kernel::task_message_session_endpoint_handled(
                endpoint.session_handle + endpoint.channel_slot);
        }
    };

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition &&
               result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_protocol_table() noexcept
    {
        RequestOpState echo_state{};
        RequestOpState status_state{};
        CloseState close_state{};
        EchoOperation echo{
            .state = &echo_state,
        };
        StatusOperation status{
            .state = &status_state,
        };
        CloseHook close{
            .state = &close_state,
        };
        ProtocolTrace trace{};
        auto protocol = kernel::make_task_message_session_protocol<2>(
            std::array<kernel::TaskMessageSessionProtocolEntry, 2>{
                kernel::task_message_session_protocol_entry(
                    kEchoOperation,
                    "echo",
                    echo),
                kernel::task_message_session_protocol_entry(
                    kStatusOperation,
                    "status",
                    status),
            },
            kernel::make_task_message_session_protocol_close_handler(close),
            &trace);

        const auto endpoint = kernel::TaskMessageSessionEndpoint{
            .service_id = kServiceId,
            .service_name = "echo-protocol",
            .session_handle = kBaseSessionHandle,
            .open_payload = kOpenPayload,
            .channel_slot = 1u,
        };
        const auto lookup_echo = protocol.lookup(kEchoOperation);
        const auto lookup_missing = protocol.lookup(kUnsupportedOperation);
        const auto echo_result = protocol.dispatch_request(
            kernel::TaskMessageSessionEndpointRequestView{
                .endpoint = endpoint,
                .operation = kEchoOperation,
                .payload = kEchoPayload,
            });
        const auto status_result = protocol.dispatch_request(
            kernel::TaskMessageSessionEndpointRequestView{
                .endpoint = endpoint,
                .operation = kStatusOperation,
                .payload = kStatusPayload,
            });
        const auto unsupported_result = protocol.dispatch_request(
            kernel::TaskMessageSessionEndpointRequestView{
                .endpoint = endpoint,
                .operation = kUnsupportedOperation,
                .payload = kUnsupportedPayload,
            });
        const auto close_result = protocol.dispatch_close(
            kernel::TaskMessageSessionEndpointCloseView{
                .endpoint = endpoint,
                .reason = kCloseReason,
            });

        ProtocolTrace default_close_trace{};
        auto default_close_protocol =
            kernel::make_task_message_session_protocol<1>(
                std::array<kernel::TaskMessageSessionProtocolEntry, 1>{
                    kernel::task_message_session_protocol_entry(
                        kEchoOperation,
                        "echo",
                        echo),
                },
                &default_close_trace);
        const auto default_close_result = default_close_protocol.close(
            kernel::TaskMessageSessionEndpointCloseView{
                .endpoint = endpoint,
                .reason = kCloseReason,
            });

        ProtocolTrace broken_trace{};
        auto broken_protocol = kernel::make_task_message_session_protocol<1>(
            std::array<kernel::TaskMessageSessionProtocolEntry, 1>{
                kernel::task_message_session_protocol_entry(
                    kBrokenOperation,
                    "broken"),
            },
            &broken_trace);
        const auto broken_result = broken_protocol.dispatch_request(
            kernel::TaskMessageSessionEndpointRequestView{
                .endpoint = endpoint,
                .operation = kBrokenOperation,
                .payload = 1u,
            });

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        const auto* third = trace.at(2u);
        const auto* fourth = trace.at(3u);
        const auto* default_close_event = default_close_trace.at(0u);
        const auto* broken_event = broken_trace.at(0u);
        if (first == nullptr || second == nullptr || third == nullptr ||
            fourth == nullptr || default_close_event == nullptr ||
            broken_event == nullptr) {
            return false;
        }

        const auto echo_witness =
            kernel::task_message_session_protocol_witness(*first);
        const auto unsupported_witness =
            kernel::task_message_session_protocol_witness(*third);
        const auto close_witness =
            kernel::task_message_session_protocol_witness(*fourth);
        const auto default_close_witness =
            kernel::task_message_session_protocol_witness(
                *default_close_event);
        const auto broken_witness =
            kernel::task_message_session_protocol_witness(*broken_event);
        const auto handoff =
            kernel::task_message_session_protocol_witness_handoff_target(
                close_witness);

        return protocol.valid() &&
               Protocol::capacity() == 2u &&
               lookup_echo.matched &&
               lookup_echo.slot == 0u &&
               lookup_echo.entry != nullptr &&
               same_text(lookup_echo.entry->operation_name, "echo"sv) &&
               !lookup_missing.matched &&
               trap_result_matches(echo_result.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kEchoOperation +
                                       kEchoPayload + 1u) &&
               echo_result.matched && echo_result.handler_valid &&
               echo_result.slot == 0u &&
               same_text(echo_result.operation_name, "echo"sv) &&
               trap_result_matches(status_result.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kServiceId + kOpenPayload +
                                       kStatusPayload + 1u) &&
               status_result.matched && status_result.handler_valid &&
               status_result.slot == 1u &&
               same_text(status_result.operation_name, "status"sv) &&
               trap_result_matches(unsupported_result.trap,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               !unsupported_result.matched &&
               !unsupported_result.handler_valid &&
               unsupported_result.slot ==
                   kernel::task_message_session_protocol_unmapped_slot &&
               trap_result_matches(close_result.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 2u) &&
               close_result.close_handler_valid &&
               trap_result_matches(default_close_result,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0u) &&
               trap_result_matches(broken_result.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               broken_result.matched && !broken_result.handler_valid &&
               broken_result.slot == 0u &&
               echo_state.calls == 1u &&
               echo_state.last_service_id == kServiceId &&
               echo_state.last_session_handle == kBaseSessionHandle &&
               echo_state.last_open_payload == kOpenPayload &&
               echo_state.last_operation == kEchoOperation &&
               echo_state.last_payload == kEchoPayload &&
               echo_state.last_channel_slot == 1u &&
               status_state.calls == 1u &&
               status_state.last_operation == kStatusOperation &&
               status_state.last_payload == kStatusPayload &&
               status_state.last_channel_slot == 1u &&
               close_state.calls == 1u &&
               close_state.last_service_id == kServiceId &&
               close_state.last_session_handle == kBaseSessionHandle &&
               close_state.last_open_payload == kOpenPayload &&
               close_state.last_reason == kCloseReason &&
               close_state.last_channel_slot == 1u &&
               trace.size() == 4u &&
               first->sequence == 1u &&
               first->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::request &&
               first->service_id == kServiceId &&
               same_text(first->service_name, "echo-protocol"sv) &&
               first->session_handle == kBaseSessionHandle &&
               first->open_payload == kOpenPayload &&
               first->channel_slot == 1u &&
               first->operation == kEchoOperation &&
               same_text(first->operation_name, "echo"sv) &&
               first->payload == kEchoPayload &&
               first->slot == 0u &&
               first->matched &&
               first->handler_valid &&
               !first->close_handler_valid &&
               first->value ==
                   kBaseSessionHandle + kEchoOperation + kEchoPayload + 1u &&
               second->sequence == 2u &&
               second->slot == 1u &&
               same_text(second->operation_name, "status"sv) &&
               second->value ==
                   kServiceId + kOpenPayload + kStatusPayload + 1u &&
               third->sequence == 3u &&
               third->slot ==
                   kernel::task_message_session_protocol_unmapped_slot &&
               !third->matched &&
               !third->handler_valid &&
               third->error == kernel::TrapError::unsupported_service &&
               fourth->sequence == 4u &&
               fourth->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::close &&
               fourth->operation == kernel::task_message_session_close_operation &&
               same_text(fourth->operation_name, "close"sv) &&
               fourth->payload == kCloseReason &&
               fourth->close_handler_valid &&
               fourth->value == kCloseReason + 2u &&
               default_close_trace.size() == 1u &&
               default_close_event->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::close &&
               !default_close_event->close_handler_valid &&
               default_close_event->value == 0u &&
               broken_trace.size() == 1u &&
               broken_event->sequence == 1u &&
               broken_event->matched &&
               !broken_event->handler_valid &&
               broken_event->error == kernel::TrapError::unbound_adapter &&
               kernel::task_message_session_protocol_witness_ready(
                   echo_witness) &&
               echo_witness.verdict() == semantic::Verdict::standing &&
               echo_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               echo_witness.request_handler_branch_ok() &&
               kernel::task_message_session_protocol_witness_ready(
                   unsupported_witness) &&
               unsupported_witness.verdict() ==
                   semantic::Verdict::standing &&
               unsupported_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               unsupported_witness.request_unmapped_branch_ok() &&
               close_witness.verdict() == semantic::Verdict::standing &&
               close_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               close_witness.close_branch_ok() &&
               default_close_witness.verdict() ==
                   semantic::Verdict::standing &&
               default_close_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               default_close_witness.close_branch_ok() &&
               broken_witness.verdict() == semantic::Verdict::standing &&
               broken_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               broken_witness.request_unbound_branch_ok() &&
               std::string_view{handoff.entry_name()} ==
                   "task-message-session-protocol-witness"sv &&
               std::string_view{handoff.selected_summary_path()} ==
                   "task-message-session-protocol-witness.summary"sv;
    }

    [[nodiscard]] bool probe_endpoint_bridge() noexcept
    {
        RequestOpState echo_state{};
        CloseState close_state{};
        EchoOperation echo{
            .state = &echo_state,
        };
        CloseHook close{
            .state = &close_state,
        };
        ProtocolTrace trace{};
        auto protocol = kernel::make_task_message_session_protocol<1>(
            std::array<kernel::TaskMessageSessionProtocolEntry, 1>{
                kernel::task_message_session_protocol_entry(
                    kEchoOperation,
                    "echo",
                    echo),
            },
            kernel::make_task_message_session_protocol_close_handler(close),
            &trace);

        kernel::TaskMessageSessionChannelHandler handler{};
        const auto binding = kernel::TaskMessageSessionEndpointBinding{
            .out_handler = &handler,
        };
        const auto channel = kernel::TaskMessageSessionChannel{
            .service_id = kServiceId,
            .service_name = "echo-protocol",
            .session_handle = kBaseSessionHandle,
            .open_payload = kOpenPayload,
            .channel_slot = 0u,
        };
        kernel::bind_task_message_session_endpoint(binding, protocol);
        const auto request = handler.request(
            channel,
            kernel::TaskMessageSessionRequestDispatchView{
                .service_id = kServiceId,
                .session_handle = kBaseSessionHandle,
                .operation = kEchoOperation,
                .payload = kEchoPayload,
            });
        const auto close_result = handler.close(
            channel,
            kernel::TaskMessageSessionCloseDispatchView{
                .service_id = kServiceId,
                .session_handle = kBaseSessionHandle,
                .reason = kCloseReason,
            });
        binding.clear();
        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        if (first == nullptr || second == nullptr) {
            return false;
        }

        return binding.valid() &&
               trap_result_matches(request,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kEchoOperation +
                                       kEchoPayload) &&
               trap_result_matches(close_result,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               echo_state.calls == 1u &&
               echo_state.last_service_id == kServiceId &&
               echo_state.last_session_handle == kBaseSessionHandle &&
               echo_state.last_open_payload == kOpenPayload &&
               echo_state.last_operation == kEchoOperation &&
               echo_state.last_payload == kEchoPayload &&
               echo_state.last_channel_slot == 0u &&
               close_state.calls == 1u &&
               close_state.last_reason == kCloseReason &&
               close_state.last_channel_slot == 0u &&
               trace.size() == 2u &&
               first->sequence == 1u &&
               first->matched &&
               first->handler_valid &&
               same_text(first->operation_name, "echo"sv) &&
               first->value ==
                   kBaseSessionHandle + kEchoOperation + kEchoPayload &&
               second->sequence == 2u &&
               second->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::close &&
               second->close_handler_valid &&
               second->value == kCloseReason + 1u &&
               !handler.valid();
    }

    [[nodiscard]] bool probe_dispatcher_integration() noexcept
    {
        std::array<RequestOpState, 2> echo_states{};
        std::array<RequestOpState, 2> status_states{};
        std::array<CloseState, 2> close_states{};
        std::array<ProtocolTrace, 2> protocol_traces{};
        std::array<EchoOperation, 2> echo_handlers{
            EchoOperation{.state = &echo_states[0]},
            EchoOperation{.state = &echo_states[1]},
        };
        std::array<StatusOperation, 2> status_handlers{
            StatusOperation{.state = &status_states[0]},
            StatusOperation{.state = &status_states[1]},
        };
        std::array<CloseHook, 2> close_handlers{
            CloseHook{.state = &close_states[0]},
            CloseHook{.state = &close_states[1]},
        };
        std::array<Protocol, 2> protocols{
            kernel::make_task_message_session_protocol<2>(
                std::array<kernel::TaskMessageSessionProtocolEntry, 2>{
                    kernel::task_message_session_protocol_entry(
                        kEchoOperation,
                        "echo",
                        echo_handlers[0]),
                    kernel::task_message_session_protocol_entry(
                        kStatusOperation,
                        "status",
                        status_handlers[0]),
                },
                kernel::make_task_message_session_protocol_close_handler(
                    close_handlers[0]),
                &protocol_traces[0]),
            kernel::make_task_message_session_protocol<2>(
                std::array<kernel::TaskMessageSessionProtocolEntry, 2>{
                    kernel::task_message_session_protocol_entry(
                        kEchoOperation,
                        "echo",
                        echo_handlers[1]),
                    kernel::task_message_session_protocol_entry(
                        kStatusOperation,
                        "status",
                        status_handlers[1]),
                },
                kernel::make_task_message_session_protocol_close_handler(
                    close_handlers[1]),
                &protocol_traces[1]),
        };
        ProtocolAcceptorState acceptor_state{};
        ProtocolAcceptor protocol_acceptor{
            .state = &acceptor_state,
            .protocols = &protocols,
        };
        AcceptorTrace acceptor_trace{};
        auto service_acceptor =
            kernel::make_task_message_session_service_acceptor<2>(
                kernel::make_task_message_session_endpoint_acceptor(
                    protocol_acceptor),
                "echo-protocol",
                &acceptor_trace);
        DispatchTrace dispatch_trace{};
        auto dispatcher = kernel::make_task_message_session_dispatcher<1, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 1>{
                kernel::task_message_session_service_acceptor_entry(
                    kServiceId,
                    "echo-protocol",
                    service_acceptor),
            },
            &dispatch_trace);
        dispatcher.bind_next_session_handle(kBaseSessionHandle);

        const auto open = dispatcher.open_session(kServiceId, kOpenPayload);
        const auto request_echo = dispatcher.request_session(
            kBaseSessionHandle,
            kEchoOperation,
            kEchoPayload);
        const auto request_status = dispatcher.request_session(
            kBaseSessionHandle,
            kStatusOperation,
            kStatusPayload);
        const auto request_unsupported = dispatcher.request_session(
            kBaseSessionHandle,
            kUnsupportedOperation,
            kUnsupportedPayload);
        const auto close = dispatcher.close_session(
            kBaseSessionHandle,
            kCloseReason);
        const auto reopen = dispatcher.open_session(
            kServiceId,
            kSecondOpenPayload);

        const auto* session_slot = dispatcher.session(0u);
        const auto* channel_slot = service_acceptor.channel(0u);
        const auto* dispatch_first = dispatch_trace.at(0u);
        const auto* dispatch_second = dispatch_trace.at(1u);
        const auto* dispatch_third = dispatch_trace.at(2u);
        const auto* dispatch_fourth = dispatch_trace.at(3u);
        const auto* dispatch_fifth = dispatch_trace.at(4u);
        const auto* dispatch_sixth = dispatch_trace.at(5u);
        const auto* accept_first = acceptor_trace.at(0u);
        const auto* accept_second = acceptor_trace.at(1u);
        const auto* accept_third = acceptor_trace.at(2u);
        const auto* accept_fourth = acceptor_trace.at(3u);
        const auto* accept_fifth = acceptor_trace.at(4u);
        const auto* accept_sixth = acceptor_trace.at(5u);
        const auto* protocol_first = protocol_traces[0].at(0u);
        const auto* protocol_second = protocol_traces[0].at(1u);
        const auto* protocol_third = protocol_traces[0].at(2u);
        const auto* protocol_fourth = protocol_traces[0].at(3u);
        if (session_slot == nullptr || channel_slot == nullptr ||
            dispatch_first == nullptr || dispatch_second == nullptr ||
            dispatch_third == nullptr || dispatch_fourth == nullptr ||
            dispatch_fifth == nullptr || dispatch_sixth == nullptr ||
            accept_first == nullptr || accept_second == nullptr ||
            accept_third == nullptr || accept_fourth == nullptr ||
            accept_fifth == nullptr || accept_sixth == nullptr ||
            protocol_first == nullptr || protocol_second == nullptr ||
            protocol_third == nullptr || protocol_fourth == nullptr) {
            return false;
        }

        return dispatcher.valid() &&
               service_acceptor.valid() &&
               trap_result_matches(open.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open.matched && open.handler_valid &&
               open.session_allocated &&
               open.service_slot == 0u &&
               open.session_slot == 0u &&
               open.session_handle == kBaseSessionHandle &&
               trap_result_matches(request_echo.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kEchoOperation +
                                       kEchoPayload) &&
               request_echo.matched &&
               request_echo.handler_valid &&
               request_echo.session_found &&
               trap_result_matches(request_status.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kServiceId + kOpenPayload +
                                       kStatusPayload) &&
               request_status.matched &&
               request_status.handler_valid &&
               request_status.session_found &&
               trap_result_matches(request_unsupported.trap,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               request_unsupported.matched &&
               request_unsupported.handler_valid &&
               request_unsupported.session_found &&
               trap_result_matches(close.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               close.session_closed &&
               trap_result_matches(reopen.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + 1u) &&
               reopen.session_allocated &&
               reopen.session_handle == kBaseSessionHandle + 1u &&
               dispatcher.active_sessions() == 1u &&
               service_acceptor.active_channels() == 1u &&
               session_slot->active &&
               session_slot->service_id == kServiceId &&
               session_slot->session_handle == kBaseSessionHandle + 1u &&
               channel_slot->active &&
               channel_slot->channel.service_id == kServiceId &&
               same_text(channel_slot->channel.service_name, "echo-protocol"sv) &&
               channel_slot->channel.session_handle ==
                   kBaseSessionHandle + 1u &&
               channel_slot->channel.open_payload == kSecondOpenPayload &&
               channel_slot->channel.channel_slot == 0u &&
               acceptor_state.accept_calls == 2u &&
               acceptor_state.last_service_id == kServiceId &&
               acceptor_state.last_session_handle == kBaseSessionHandle + 1u &&
               acceptor_state.last_open_payload == kSecondOpenPayload &&
               acceptor_state.last_channel_slot == 0u &&
               acceptor_state.last_binding_valid &&
               echo_states[0].calls == 1u &&
               echo_states[0].last_session_handle == kBaseSessionHandle &&
               echo_states[0].last_open_payload == kOpenPayload &&
               echo_states[0].last_operation == kEchoOperation &&
               echo_states[0].last_payload == kEchoPayload &&
               echo_states[0].last_channel_slot == 0u &&
               status_states[0].calls == 1u &&
               status_states[0].last_operation == kStatusOperation &&
               status_states[0].last_payload == kStatusPayload &&
               status_states[0].last_channel_slot == 0u &&
               close_states[0].calls == 1u &&
               close_states[0].last_reason == kCloseReason &&
               close_states[0].last_channel_slot == 0u &&
               echo_states[1].calls == 0u &&
               status_states[1].calls == 0u &&
               close_states[1].calls == 0u &&
               dispatch_trace.size() == 6u &&
               dispatch_first->sequence == 1u &&
               dispatch_first->action ==
                   kernel::TaskMessageSessionActionKind::open &&
               dispatch_first->session_handle == kBaseSessionHandle &&
               same_text(dispatch_first->service_name, "echo-protocol"sv) &&
               dispatch_first->matched &&
               dispatch_first->handler_valid &&
               dispatch_first->session_allocated &&
               dispatch_second->sequence == 2u &&
               dispatch_second->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               dispatch_second->value ==
                   kBaseSessionHandle + kEchoOperation + kEchoPayload &&
               dispatch_third->sequence == 3u &&
               dispatch_third->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               dispatch_third->value ==
                   kServiceId + kOpenPayload + kStatusPayload &&
               dispatch_fourth->sequence == 4u &&
               dispatch_fourth->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               dispatch_fourth->error ==
                   kernel::TrapError::unsupported_service &&
               dispatch_fifth->sequence == 5u &&
               dispatch_fifth->action ==
                   kernel::TaskMessageSessionActionKind::close &&
               dispatch_fifth->session_closed &&
               dispatch_sixth->sequence == 6u &&
               dispatch_sixth->session_handle == kBaseSessionHandle + 1u &&
               dispatch_sixth->session_allocated &&
               acceptor_trace.size() == 6u &&
               accept_first->sequence == 1u &&
               accept_first->action ==
                   kernel::TaskMessageSessionActionKind::open &&
               accept_first->channel_bound &&
               same_text(accept_first->service_name, "echo-protocol"sv) &&
               accept_second->sequence == 2u &&
               accept_second->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               accept_second->channel_found &&
               accept_second->value ==
                   kBaseSessionHandle + kEchoOperation + kEchoPayload &&
               accept_third->sequence == 3u &&
               accept_third->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               accept_third->value ==
                   kServiceId + kOpenPayload + kStatusPayload &&
               accept_fourth->sequence == 4u &&
               accept_fourth->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               accept_fourth->channel_found &&
               accept_fourth->error ==
                   kernel::TrapError::unsupported_service &&
               accept_fifth->sequence == 5u &&
               accept_fifth->action ==
                   kernel::TaskMessageSessionActionKind::close &&
               accept_fifth->channel_closed &&
               accept_sixth->sequence == 6u &&
               accept_sixth->session_handle == kBaseSessionHandle + 1u &&
               accept_sixth->channel_bound &&
               protocol_traces[0].size() == 4u &&
               protocol_first->sequence == 1u &&
               protocol_first->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::request &&
               protocol_first->slot == 0u &&
               same_text(protocol_first->operation_name, "echo"sv) &&
               protocol_first->value ==
                   kBaseSessionHandle + kEchoOperation + kEchoPayload &&
               protocol_second->sequence == 2u &&
               protocol_second->slot == 1u &&
               same_text(protocol_second->operation_name, "status"sv) &&
               protocol_second->value ==
                   kServiceId + kOpenPayload + kStatusPayload &&
               protocol_third->sequence == 3u &&
               protocol_third->slot ==
                   kernel::task_message_session_protocol_unmapped_slot &&
               !protocol_third->matched &&
               protocol_third->error ==
                   kernel::TrapError::unsupported_service &&
               protocol_fourth->sequence == 4u &&
               protocol_fourth->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::close &&
               protocol_fourth->close_handler_valid &&
               protocol_fourth->value == kCloseReason + 1u &&
               protocol_traces[1].size() == 0u;
    }
}

int main()
{
    const bool table_ok = demo::probe_protocol_table();
    const bool endpoint_ok = demo::probe_endpoint_bridge();
    const bool dispatcher_ok = demo::probe_dispatcher_integration();
    const bool ok = table_ok && endpoint_ok && dispatcher_ok;
    const auto collapsed =
        kernel::task_message_session_protocol_witness(
            kernel::TaskMessageSessionProtocolTraceBuffer<1>{});

    std::printf(
        "[runtime-task-message-session-protocol-demo] ok=%d table=%d endpoint=%d dispatcher=%d\n",
        ok ? 1 : 0,
        table_ok ? 1 : 0,
        endpoint_ok ? 1 : 0,
        dispatcher_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-protocol-witness] ok=%d collapsed=%s route=%s summary=%s\n",
        ok ? 1 : 0,
        semantic::verdict_name(collapsed.verdict()),
        semantic::failure_domain_name(collapsed.failure_domain()),
        collapsed.summary_path().data());
    return ok ? 0 : 1;
}
