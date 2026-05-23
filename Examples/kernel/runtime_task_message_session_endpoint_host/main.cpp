#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_session_endpoint;
import kernel.task_message_session_dispatch;
import semantic.core;

namespace demo {
    using namespace std::literals;

    inline constexpr std::uint64_t kServiceId{0x51u};
    inline constexpr std::uint64_t kBaseSessionHandle{0x9000u};
    inline constexpr std::uint64_t kOpenPayload{0xAAu};
    inline constexpr std::uint64_t kSecondOpenPayload{0xBBu};
    inline constexpr std::uint64_t kThirdOpenPayload{0xCCu};
    inline constexpr std::uint64_t kRequestOperation{0x21u};
    inline constexpr std::uint64_t kRequestPayload{33u};
    inline constexpr std::uint64_t kCloseReason{0x77u};

    struct EchoEndpointState {
        std::uint32_t request_calls{0};
        std::uint32_t close_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint64_t last_operation{0};
        std::uint64_t last_payload{0};
        std::uint64_t last_reason{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
    };

    struct EchoEndpoint {
        EchoEndpointState* state{nullptr};

        [[nodiscard]] kernel::TrapResult request(
            kernel::TaskMessageSessionEndpointRequestView request) noexcept
        {
            ++state->request_calls;
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

        [[nodiscard]] kernel::TrapResult close(
            kernel::TaskMessageSessionEndpointCloseView close) noexcept
        {
            ++state->close_calls;
            state->last_service_id = close.endpoint.service_id;
            state->last_session_handle = close.endpoint.session_handle;
            state->last_open_payload = close.endpoint.open_payload;
            state->last_reason = close.reason;
            state->last_channel_slot = close.endpoint.channel_slot;
            return kernel::task_message_session_endpoint_handled(
                close.reason + close.endpoint.channel_slot + 1u);
        }
    };

    struct EchoEndpointAcceptorState {
        std::uint32_t accept_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
        bool last_binding_valid{false};
    };

    struct EchoEndpointAcceptor {
        EchoEndpointAcceptorState* state{nullptr};
        std::array<EchoEndpoint, 2>* endpoints{nullptr};

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

            if (!binding.valid() || endpoints == nullptr ||
                endpoint.channel_slot >= endpoints->size()) {
                return kernel::task_message_session_endpoint_invalid_argument();
            }

            kernel::bind_task_message_session_endpoint(
                binding,
                (*endpoints)[endpoint.channel_slot]);
            return kernel::task_message_session_endpoint_handled(
                endpoint.session_handle + endpoint.channel_slot);
        }
    };

    struct BrokenEndpointAcceptorState {
        std::uint32_t accept_calls{0};
        std::uint64_t last_session_handle{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
        bool last_binding_valid{false};
    };

    struct BrokenEndpointAcceptor {
        BrokenEndpointAcceptorState* state{nullptr};

        [[nodiscard]] kernel::TrapResult accept(
            kernel::TaskMessageSessionEndpoint endpoint,
            kernel::TaskMessageSessionEndpointBinding binding) noexcept
        {
            ++state->accept_calls;
            state->last_session_handle = endpoint.session_handle;
            state->last_channel_slot = endpoint.channel_slot;
            state->last_binding_valid = binding.valid();
            return kernel::task_message_session_endpoint_handled(0xBADu);
        }
    };

    using AcceptorTrace = kernel::TaskMessageSessionServiceAcceptorTraceBuffer<8>;
    using DispatchTrace = kernel::TaskMessageSessionDispatchTraceBuffer<8>;

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
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_endpoint_handler_bridge() noexcept
    {
        EchoEndpointState endpoint_state{};
        EchoEndpoint endpoint{
            .state = &endpoint_state,
        };
        auto handler = kernel::make_task_message_session_endpoint_handler(endpoint);
        const auto channel = kernel::TaskMessageSessionChannel{
            .service_id = kServiceId,
            .service_name = "echo-endpoint",
            .session_handle = kBaseSessionHandle,
            .open_payload = kOpenPayload,
            .channel_slot = 1u,
        };
        const auto endpoint_view =
            kernel::make_task_message_session_endpoint(channel);
        const auto endpoint_witness =
            kernel::task_message_session_endpoint_witness(endpoint_view);
        const auto request_view =
            kernel::TaskMessageSessionEndpointRequestView{
                .endpoint = endpoint_view,
                .operation = kRequestOperation,
                .payload = kRequestPayload,
            };
        const auto close_view =
            kernel::TaskMessageSessionEndpointCloseView{
                .endpoint = endpoint_view,
                .reason = kCloseReason,
            };
        const auto request = handler.request(
            channel,
            kernel::TaskMessageSessionRequestDispatchView{
                .service_id = kServiceId,
                .session_handle = kBaseSessionHandle,
                .operation = kRequestOperation,
                .payload = kRequestPayload,
            });
        const auto close = handler.close(
            channel,
            kernel::TaskMessageSessionCloseDispatchView{
                .service_id = kServiceId,
                .session_handle = kBaseSessionHandle,
                .reason = kCloseReason,
            });
        const auto request_witness =
            kernel::task_message_session_endpoint_request_witness(
                request_view,
                request);
        const auto close_witness =
            kernel::task_message_session_endpoint_close_witness(close_view,
                                                                close);

        return handler.valid() &&
               endpoint_witness.verdict() == semantic::Verdict::standing &&
               endpoint_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               endpoint_witness.endpoint_branch_ok() &&
               endpoint_view.service_id == kServiceId &&
               same_text(endpoint_view.service_name, "echo-endpoint"sv) &&
               endpoint_view.session_handle == kBaseSessionHandle &&
               endpoint_view.open_payload == kOpenPayload &&
               endpoint_view.channel_slot == 1u &&
               trap_result_matches(request,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kRequestOperation +
                                       kRequestPayload + 1u) &&
               request_witness.verdict() == semantic::Verdict::standing &&
               request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               request_witness.request_branch_ok() &&
               trap_result_matches(close,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 2u) &&
               close_witness.verdict() == semantic::Verdict::standing &&
               close_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               close_witness.close_branch_ok() &&
               trap_result_matches(
                   kernel::task_message_session_endpoint_invalid_argument(),
                   kernel::TrapDisposition::rejected,
                   kernel::TrapError::invalid_argument) &&
               trap_result_matches(
                   kernel::task_message_session_endpoint_unbound_adapter(),
                   kernel::TrapDisposition::rejected,
                   kernel::TrapError::unbound_adapter) &&
               trap_result_matches(
                   kernel::task_message_session_endpoint_unsupported(),
                   kernel::TrapDisposition::unsupported,
                   kernel::TrapError::unsupported_service) &&
               endpoint_state.request_calls == 1u &&
               endpoint_state.close_calls == 1u &&
               endpoint_state.last_service_id == kServiceId &&
               endpoint_state.last_session_handle == kBaseSessionHandle &&
               endpoint_state.last_open_payload == kOpenPayload &&
               endpoint_state.last_operation == kRequestOperation &&
               endpoint_state.last_payload == kRequestPayload &&
               endpoint_state.last_reason == kCloseReason &&
               endpoint_state.last_channel_slot == 1u;
    }

    [[nodiscard]] bool probe_endpoint_service_acceptor_bridge() noexcept
    {
        std::array<EchoEndpointState, 2> endpoint_states{};
        std::array<EchoEndpoint, 2> endpoints{
            EchoEndpoint{.state = &endpoint_states[0]},
            EchoEndpoint{.state = &endpoint_states[1]},
        };
        EchoEndpointAcceptorState accept_state{};
        EchoEndpointAcceptor endpoint_acceptor{
            .state = &accept_state,
            .endpoints = &endpoints,
        };
        AcceptorTrace trace{};
        auto service_acceptor =
            kernel::make_task_message_session_service_acceptor<2>(
                kernel::make_task_message_session_endpoint_acceptor(
                    endpoint_acceptor),
                "echo-endpoint",
                &trace);

        const auto open0 = service_acceptor.open_session(
            kServiceId,
            kBaseSessionHandle,
            kOpenPayload);
        const auto open1 = service_acceptor.open_session(
            kServiceId,
            kBaseSessionHandle + 1u,
            kSecondOpenPayload);
        const auto open_full = service_acceptor.open_session(
            kServiceId,
            kBaseSessionHandle + 2u,
            kThirdOpenPayload);
        const auto request0 = service_acceptor.request_session(
            kBaseSessionHandle,
            kRequestOperation,
            kRequestPayload);
        const auto close0 =
            service_acceptor.close_session(kBaseSessionHandle, kCloseReason);
        const auto reopen0 = service_acceptor.open_session(
            kServiceId,
            kBaseSessionHandle + 2u,
            kThirdOpenPayload);
        const auto missing_request =
            service_acceptor.request_session(0xDEADu, 1u, 2u);

        const auto* slot0 = service_acceptor.channel(0u);
        const auto* slot1 = service_acceptor.channel(1u);
        const auto* first = trace.at(0u);
        const auto* third = trace.at(2u);
        const auto* fifth = trace.at(4u);
        const auto* sixth = trace.at(5u);
        const auto* seventh = trace.at(6u);
        if (slot0 == nullptr || slot1 == nullptr || first == nullptr ||
            third == nullptr || fifth == nullptr || sixth == nullptr ||
            seventh == nullptr) {
            return false;
        }

        BrokenEndpointAcceptorState broken_state{};
        BrokenEndpointAcceptor broken_acceptor{
            .state = &broken_state,
        };
        AcceptorTrace broken_trace{};
        auto broken_service_acceptor =
            kernel::make_task_message_session_service_acceptor<1>(
                kernel::make_task_message_session_endpoint_acceptor(
                    broken_acceptor),
                "broken-endpoint",
                &broken_trace);
        const auto broken_open = broken_service_acceptor.open_session(
            kServiceId,
            kBaseSessionHandle,
            kOpenPayload);
        const auto broken_endpoint = kernel::TaskMessageSessionEndpoint{
            .service_id = kServiceId,
            .service_name = "broken-endpoint",
            .session_handle = kBaseSessionHandle,
            .open_payload = kOpenPayload,
            .channel_slot = 0u,
        };
        auto broken_handler = kernel::TaskMessageSessionChannelHandler{};
        const auto broken_binding =
            kernel::TaskMessageSessionEndpointBinding{
                .out_handler = &broken_handler,
            };
        const auto broken_accept_witness =
            kernel::task_message_session_endpoint_accept_witness(
                broken_endpoint,
                broken_binding,
                broken_handler,
                broken_open.trap);
        const auto* broken_first = broken_trace.at(0u);
        if (broken_first == nullptr) {
            return false;
        }

        return service_acceptor.valid() &&
               trap_result_matches(open0.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open0.action == kernel::TaskMessageSessionActionKind::open &&
               open0.service_id == kServiceId &&
               same_text(open0.service_name, "echo-endpoint"sv) &&
               open0.session_handle == kBaseSessionHandle &&
               open0.channel_slot == 0u &&
               open0.acceptor_valid && open0.channel_bound &&
               trap_result_matches(open1.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + 2u) &&
               open1.channel_slot == 1u && open1.channel_bound &&
               trap_result_matches(open_full.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument) &&
               open_full.channel_slot ==
                   kernel::task_message_session_channel_unmapped_slot &&
               !open_full.channel_bound &&
               trap_result_matches(request0.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kRequestOperation +
                                       kRequestPayload) &&
               request0.channel_found && request0.channel_slot == 0u &&
               trap_result_matches(close0.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               close0.channel_found && close0.channel_closed &&
               trap_result_matches(reopen0.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + 2u) &&
               reopen0.channel_slot == 0u && reopen0.channel_bound &&
               trap_result_matches(missing_request.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument) &&
               !missing_request.channel_found &&
               service_acceptor.active_channels() == 2u &&
               slot0->active && slot1->active &&
               slot0->channel.session_handle == kBaseSessionHandle + 2u &&
               slot0->channel.open_payload == kThirdOpenPayload &&
               slot1->channel.session_handle == kBaseSessionHandle + 1u &&
               slot1->channel.open_payload == kSecondOpenPayload &&
               accept_state.accept_calls == 3u &&
               accept_state.last_service_id == kServiceId &&
               accept_state.last_session_handle == kBaseSessionHandle + 2u &&
               accept_state.last_open_payload == kThirdOpenPayload &&
               accept_state.last_channel_slot == 0u &&
               accept_state.last_binding_valid &&
               endpoint_states[0].request_calls == 1u &&
               endpoint_states[0].close_calls == 1u &&
               endpoint_states[0].last_session_handle == kBaseSessionHandle &&
               endpoint_states[0].last_open_payload == kOpenPayload &&
               endpoint_states[0].last_operation == kRequestOperation &&
               endpoint_states[0].last_payload == kRequestPayload &&
               endpoint_states[0].last_reason == kCloseReason &&
               endpoint_states[0].last_channel_slot == 0u &&
               endpoint_states[1].request_calls == 0u &&
               endpoint_states[1].close_calls == 0u &&
               trace.size() == 7u &&
               first->sequence == 1u &&
               first->action == kernel::TaskMessageSessionActionKind::open &&
               first->service_id == kServiceId &&
               same_text(first->service_name, "echo-endpoint"sv) &&
               first->session_handle == kBaseSessionHandle &&
               first->channel_slot == 0u &&
               first->acceptor_valid && first->channel_bound &&
               third->sequence == 3u &&
               third->action == kernel::TaskMessageSessionActionKind::open &&
               third->channel_slot ==
                   kernel::task_message_session_channel_unmapped_slot &&
               third->error == kernel::TrapError::invalid_argument &&
               !third->channel_bound &&
               fifth->sequence == 5u &&
               fifth->action == kernel::TaskMessageSessionActionKind::close &&
               fifth->channel_found && fifth->channel_closed &&
               sixth->sequence == 6u &&
               sixth->session_handle == kBaseSessionHandle + 2u &&
               sixth->channel_slot == 0u &&
               sixth->channel_bound &&
               seventh->sequence == 7u &&
               seventh->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               !seventh->channel_found &&
               seventh->error == kernel::TrapError::invalid_argument &&
               trap_result_matches(broken_open.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               broken_state.accept_calls == 1u &&
               broken_state.last_session_handle == kBaseSessionHandle &&
               broken_state.last_channel_slot == 0u &&
               broken_state.last_binding_valid &&
               broken_accept_witness.verdict() ==
                   semantic::Verdict::standing &&
               broken_accept_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               broken_accept_witness.accept_branch_ok() &&
               broken_trace.size() == 1u &&
               broken_first->sequence == 1u &&
               broken_first->acceptor_valid &&
               !broken_first->channel_bound &&
               broken_first->error == kernel::TrapError::unbound_adapter;
    }

    [[nodiscard]] bool probe_dispatcher_integration() noexcept
    {
        std::array<EchoEndpointState, 2> endpoint_states{};
        std::array<EchoEndpoint, 2> endpoints{
            EchoEndpoint{.state = &endpoint_states[0]},
            EchoEndpoint{.state = &endpoint_states[1]},
        };
        EchoEndpointAcceptorState accept_state{};
        EchoEndpointAcceptor endpoint_acceptor{
            .state = &accept_state,
            .endpoints = &endpoints,
        };
        AcceptorTrace acceptor_trace{};
        auto service_acceptor =
            kernel::make_task_message_session_service_acceptor<2>(
                kernel::make_task_message_session_endpoint_acceptor(
                    endpoint_acceptor),
                "echo-endpoint",
                &acceptor_trace);
        DispatchTrace dispatch_trace{};
        auto dispatcher = kernel::make_task_message_session_dispatcher<1, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 1>{
                kernel::task_message_session_service_acceptor_entry(
                    kServiceId,
                    "echo-endpoint",
                    service_acceptor),
            },
            &dispatch_trace);
        dispatcher.bind_next_session_handle(kBaseSessionHandle);

        const auto open = dispatcher.open_session(kServiceId, kOpenPayload);
        const auto request = dispatcher.request_session(
            kBaseSessionHandle,
            kRequestOperation,
            kRequestPayload);
        const auto close =
            dispatcher.close_session(kBaseSessionHandle, kCloseReason);
        const auto reopen = dispatcher.open_session(
            kServiceId,
            kSecondOpenPayload);

        const auto* session_slot = dispatcher.session(0u);
        const auto* channel_slot = service_acceptor.channel(0u);
        const auto* dispatch_first = dispatch_trace.at(0u);
        const auto* dispatch_second = dispatch_trace.at(1u);
        const auto* dispatch_third = dispatch_trace.at(2u);
        const auto* dispatch_fourth = dispatch_trace.at(3u);
        const auto* accept_first = acceptor_trace.at(0u);
        const auto* accept_second = acceptor_trace.at(1u);
        const auto* accept_third = acceptor_trace.at(2u);
        const auto* accept_fourth = acceptor_trace.at(3u);
        if (session_slot == nullptr || channel_slot == nullptr ||
            dispatch_first == nullptr || dispatch_second == nullptr ||
            dispatch_third == nullptr || dispatch_fourth == nullptr ||
            accept_first == nullptr || accept_second == nullptr ||
            accept_third == nullptr || accept_fourth == nullptr) {
            return false;
        }

        return dispatcher.valid() && service_acceptor.valid() &&
               trap_result_matches(open.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open.matched && open.handler_valid &&
               open.session_allocated &&
               open.service_slot == 0u && open.session_slot == 0u &&
               open.service_id == kServiceId &&
               open.session_handle == kBaseSessionHandle &&
               trap_result_matches(request.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kRequestOperation +
                                       kRequestPayload) &&
               request.matched && request.handler_valid &&
               request.session_found &&
               request.service_slot == 0u && request.session_slot == 0u &&
               trap_result_matches(close.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               close.matched && close.handler_valid &&
               close.session_found && close.session_closed &&
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
               channel_slot->channel.session_handle == kBaseSessionHandle + 1u &&
               channel_slot->channel.open_payload == kSecondOpenPayload &&
               channel_slot->channel.channel_slot == 0u &&
               accept_state.accept_calls == 2u &&
               accept_state.last_session_handle == kBaseSessionHandle + 1u &&
               accept_state.last_open_payload == kSecondOpenPayload &&
               accept_state.last_binding_valid &&
               endpoint_states[0].request_calls == 1u &&
               endpoint_states[0].close_calls == 1u &&
               endpoint_states[0].last_session_handle == kBaseSessionHandle &&
               endpoint_states[0].last_open_payload == kOpenPayload &&
               dispatch_trace.size() == 4u &&
               dispatch_first->sequence == 1u &&
               dispatch_first->action ==
                   kernel::TaskMessageSessionActionKind::open &&
               dispatch_first->session_handle == kBaseSessionHandle &&
               same_text(dispatch_first->service_name, "echo-endpoint"sv) &&
               dispatch_first->matched &&
               dispatch_first->handler_valid &&
               dispatch_first->session_allocated &&
               dispatch_second->sequence == 2u &&
               dispatch_second->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               dispatch_second->session_found &&
               dispatch_second->value ==
                   kBaseSessionHandle + kRequestOperation + kRequestPayload &&
               dispatch_third->sequence == 3u &&
               dispatch_third->action ==
                   kernel::TaskMessageSessionActionKind::close &&
               dispatch_third->session_closed &&
               dispatch_fourth->sequence == 4u &&
               dispatch_fourth->session_handle == kBaseSessionHandle + 1u &&
               dispatch_fourth->session_allocated &&
               acceptor_trace.size() == 4u &&
               accept_first->sequence == 1u &&
               accept_first->action ==
                   kernel::TaskMessageSessionActionKind::open &&
               same_text(accept_first->service_name, "echo-endpoint"sv) &&
               accept_first->channel_slot == 0u &&
               accept_first->channel_bound &&
               accept_second->sequence == 2u &&
               accept_second->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               accept_second->channel_found &&
               accept_third->sequence == 3u &&
               accept_third->action ==
                   kernel::TaskMessageSessionActionKind::close &&
               accept_third->channel_closed &&
               accept_fourth->sequence == 4u &&
               accept_fourth->session_handle == kBaseSessionHandle + 1u &&
               accept_fourth->channel_bound;
    }
}

int main()
{
    const bool handler_ok = demo::probe_endpoint_handler_bridge();
    const bool acceptor_ok = demo::probe_endpoint_service_acceptor_bridge();
    const bool dispatcher_ok = demo::probe_dispatcher_integration();
    const bool ok = handler_ok && acceptor_ok && dispatcher_ok;

    std::printf(
        "[runtime-task-message-session-endpoint-demo] ok=%d handler=%d acceptor=%d dispatcher=%d\n",
        ok ? 1 : 0,
        handler_ok ? 1 : 0,
        acceptor_ok ? 1 : 0,
        dispatcher_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-endpoint-witness] ok=%d collapsed=%s summary=%s\n",
        ok ? 1 : 0,
        semantic::verdict_name(
            kernel::TaskMessageSessionEndpointWitness{}.verdict()),
        kernel::TaskMessageSessionEndpointWitness{}.summary_path().data());
    return ok ? 0 : 1;
}
