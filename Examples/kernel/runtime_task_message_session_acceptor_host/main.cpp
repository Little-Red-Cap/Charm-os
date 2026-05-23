#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_session_acceptor;
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

    struct EchoChannelState {
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

    struct EchoChannel {
        EchoChannelState* state{nullptr};

        [[nodiscard]] kernel::TrapResult request(
            const kernel::TaskMessageSessionChannel& channel,
            kernel::TaskMessageSessionRequestDispatchView request_view) noexcept
        {
            ++state->request_calls;
            state->last_service_id = channel.service_id;
            state->last_session_handle = channel.session_handle;
            state->last_open_payload = channel.open_payload;
            state->last_operation = request_view.operation;
            state->last_payload = request_view.payload;
            state->last_channel_slot = channel.channel_slot;
            return handled_result(channel.session_handle +
                                  request_view.operation +
                                  request_view.payload +
                                  channel.channel_slot);
        }

        [[nodiscard]] kernel::TrapResult close(
            const kernel::TaskMessageSessionChannel& channel,
            kernel::TaskMessageSessionCloseDispatchView close_view) noexcept
        {
            ++state->close_calls;
            state->last_service_id = channel.service_id;
            state->last_session_handle = channel.session_handle;
            state->last_open_payload = channel.open_payload;
            state->last_reason = close_view.reason;
            state->last_channel_slot = channel.channel_slot;
            return handled_result(close_view.reason + channel.channel_slot + 1u);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled_result(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }
    };

    struct EchoAcceptorState {
        std::uint32_t accept_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
    };

    struct EchoAcceptor {
        EchoAcceptorState* state{nullptr};
        std::array<EchoChannel, 2>* channels{nullptr};

        [[nodiscard]] kernel::TrapResult accept(
            const kernel::TaskMessageSessionChannel& channel,
            kernel::TaskMessageSessionChannelHandler& out_handler) noexcept
        {
            ++state->accept_calls;
            state->last_service_id = channel.service_id;
            state->last_session_handle = channel.session_handle;
            state->last_open_payload = channel.open_payload;
            state->last_channel_slot = channel.channel_slot;

            if (channels == nullptr || channel.channel_slot >= channels->size()) {
                return rejected_invalid_argument();
            }

            out_handler = kernel::make_task_message_session_channel_handler(
                (*channels)[channel.channel_slot]);
            return handled_result(channel.session_handle + channel.channel_slot);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled_result(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult
        rejected_invalid_argument() noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::invalid_argument,
                .value = 0,
            };
        }
    };

    struct BrokenAcceptorState {
        std::uint32_t accept_calls{0};
        std::uint64_t last_session_handle{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
    };

    struct BrokenAcceptor {
        BrokenAcceptorState* state{nullptr};

        [[nodiscard]] kernel::TrapResult accept(
            const kernel::TaskMessageSessionChannel& channel,
            kernel::TaskMessageSessionChannelHandler&) noexcept
        {
            ++state->accept_calls;
            state->last_session_handle = channel.session_handle;
            state->last_channel_slot = channel.channel_slot;
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = 0xBADu,
            };
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

    [[nodiscard]] bool probe_direct_service_acceptor_lifecycle() noexcept
    {
        std::array<EchoChannelState, 2> channel_states{};
        std::array<EchoChannel, 2> channels{
            EchoChannel{.state = &channel_states[0]},
            EchoChannel{.state = &channel_states[1]},
        };
        EchoAcceptorState accept_state{};
        EchoAcceptor echo_acceptor{
            .state = &accept_state,
            .channels = &channels,
        };
        AcceptorTrace trace{};
        auto service_acceptor =
            kernel::make_task_message_session_service_acceptor<2>(
                echo_acceptor,
                "echo-channel",
                &trace);

        const auto open0 = service_acceptor.open_session(
            kServiceId, kBaseSessionHandle, kOpenPayload);
        const auto open1 = service_acceptor.open_session(
            kServiceId, kBaseSessionHandle + 1u, kSecondOpenPayload);
        const auto open_full = service_acceptor.open_session(
            kServiceId, kBaseSessionHandle + 2u, kThirdOpenPayload);
        const auto request0 = service_acceptor.request_session(
            kBaseSessionHandle, kRequestOperation, kRequestPayload);
        const auto close0 =
            service_acceptor.close_session(kBaseSessionHandle, kCloseReason);
        const auto reopen0 = service_acceptor.open_session(
            kServiceId, kBaseSessionHandle + 2u, kThirdOpenPayload);
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

        const auto first_witness =
            kernel::task_message_session_service_acceptor_witness(*first);
        const auto third_witness =
            kernel::task_message_session_service_acceptor_witness(*third);
        const auto request_witness =
            kernel::task_message_session_service_acceptor_witness(request0);
        const auto fifth_witness =
            kernel::task_message_session_service_acceptor_witness(*fifth);
        const auto sixth_witness =
            kernel::task_message_session_service_acceptor_witness(*sixth);
        const auto seventh_witness =
            kernel::task_message_session_service_acceptor_witness(*seventh);
        const auto handoff =
            kernel::
                task_message_session_service_acceptor_witness_handoff_target(
                    sixth_witness);

        return service_acceptor.valid() &&
               trap_result_matches(open0.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open0.action == kernel::TaskMessageSessionActionKind::open &&
               open0.service_id == kServiceId &&
               same_text(open0.service_name, "echo-channel"sv) &&
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
               request0.action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request0.channel_found && request0.channel_slot == 0u &&
               trap_result_matches(close0.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               close0.action == kernel::TaskMessageSessionActionKind::close &&
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
               slot0->channel.service_id == kServiceId &&
               slot0->channel.session_handle == kBaseSessionHandle + 2u &&
               slot0->channel.open_payload == kThirdOpenPayload &&
               slot0->channel.channel_slot == 0u &&
               slot1->channel.session_handle == kBaseSessionHandle + 1u &&
               slot1->channel.open_payload == kSecondOpenPayload &&
               slot1->channel.channel_slot == 1u &&
               accept_state.accept_calls == 3u &&
               accept_state.last_service_id == kServiceId &&
               accept_state.last_session_handle == kBaseSessionHandle + 2u &&
               accept_state.last_open_payload == kThirdOpenPayload &&
               accept_state.last_channel_slot == 0u &&
               channel_states[0].request_calls == 1u &&
               channel_states[0].close_calls == 1u &&
               channel_states[0].last_service_id == kServiceId &&
               channel_states[0].last_session_handle == kBaseSessionHandle &&
               channel_states[0].last_open_payload == kOpenPayload &&
               channel_states[0].last_operation == kRequestOperation &&
               channel_states[0].last_payload == kRequestPayload &&
               channel_states[0].last_reason == kCloseReason &&
               channel_states[0].last_channel_slot == 0u &&
               channel_states[1].request_calls == 0u &&
               channel_states[1].close_calls == 0u &&
               trace.size() == 7u &&
               first->sequence == 1u &&
               first->action == kernel::TaskMessageSessionActionKind::open &&
               first->service_id == kServiceId &&
               same_text(first->service_name, "echo-channel"sv) &&
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
               fifth->session_handle == kBaseSessionHandle &&
               sixth->sequence == 6u &&
               sixth->session_handle == kBaseSessionHandle + 2u &&
               sixth->channel_slot == 0u &&
               sixth->channel_bound &&
               seventh->sequence == 7u &&
               seventh->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               !seventh->channel_found &&
               seventh->error == kernel::TrapError::invalid_argument &&
               same_text(kernel::task_message_session_action_kind_name(
                             sixth->action),
                         "open"sv) &&
               kernel::task_message_session_service_acceptor_witness_ready(
                   first_witness) &&
               first_witness.verdict() == semantic::Verdict::standing &&
               first_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               first_witness.open_bound_branch_ok() &&
               third_witness.verdict() == semantic::Verdict::standing &&
               third_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               third_witness.open_full_branch_ok() &&
               request_witness.verdict() == semantic::Verdict::standing &&
               request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               request_witness.request_branch_ok() &&
               fifth_witness.verdict() == semantic::Verdict::standing &&
               fifth_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               fifth_witness.close_branch_ok() &&
               sixth_witness.verdict() == semantic::Verdict::standing &&
               sixth_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               sixth_witness.open_bound_branch_ok() &&
               seventh_witness.verdict() == semantic::Verdict::standing &&
               seventh_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               seventh_witness.channel_missing_branch_ok() &&
               std::string_view{handoff.entry_name()} ==
                   "task-message-session-acceptor-witness"sv &&
               std::string_view{handoff.selected_summary_path()} ==
                   "task-message-session-acceptor-witness.summary"sv;
    }

    [[nodiscard]] bool probe_unbound_and_broken_acceptors() noexcept
    {
        AcceptorTrace unbound_trace{};
        auto unbound_acceptor =
            kernel::make_task_message_session_service_acceptor<1>(
                kernel::TaskMessageSessionChannelAcceptor{},
                "unbound-channel",
                &unbound_trace);
        const auto unbound_open = unbound_acceptor.open_session(
            kServiceId, kBaseSessionHandle, kOpenPayload);

        BrokenAcceptorState broken_state{};
        BrokenAcceptor broken{
            .state = &broken_state,
        };
        AcceptorTrace broken_trace{};
        auto broken_acceptor =
            kernel::make_task_message_session_service_acceptor<1>(
                broken,
                "broken-channel",
                &broken_trace);
        const auto broken_open = broken_acceptor.open_session(
            kServiceId, kBaseSessionHandle, kOpenPayload);
        const auto broken_request =
            broken_acceptor.request_session(kBaseSessionHandle, 1u, 2u);

        const auto* unbound_first = unbound_trace.at(0u);
        const auto* broken_first = broken_trace.at(0u);
        const auto* broken_second = broken_trace.at(1u);
        if (unbound_first == nullptr || broken_first == nullptr ||
            broken_second == nullptr) {
            return false;
        }

        const auto unbound_witness =
            kernel::task_message_session_service_acceptor_witness(
                *unbound_first);
        const auto broken_open_witness =
            kernel::task_message_session_service_acceptor_witness(
                *broken_first);
        const auto broken_request_witness =
            kernel::task_message_session_service_acceptor_witness(
                *broken_second);

        return !unbound_acceptor.valid() &&
               trap_result_matches(unbound_open.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               !unbound_open.acceptor_valid &&
               !unbound_open.channel_bound &&
               unbound_acceptor.active_channels() == 0u &&
               unbound_trace.size() == 1u &&
               unbound_first->sequence == 1u &&
               !unbound_first->acceptor_valid &&
               unbound_first->error == kernel::TrapError::unbound_adapter &&
               trap_result_matches(broken_open.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               broken_open.acceptor_valid &&
               !broken_open.channel_bound &&
               broken_acceptor.active_channels() == 0u &&
               broken_state.accept_calls == 1u &&
               broken_state.last_session_handle == kBaseSessionHandle &&
               broken_state.last_channel_slot == 0u &&
               trap_result_matches(broken_request.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument) &&
               broken_trace.size() == 2u &&
               broken_first->sequence == 1u &&
               broken_first->acceptor_valid &&
               !broken_first->channel_bound &&
               broken_first->error == kernel::TrapError::unbound_adapter &&
               broken_second->sequence == 2u &&
               !broken_second->channel_found &&
               broken_second->error == kernel::TrapError::invalid_argument &&
               unbound_witness.verdict() == semantic::Verdict::standing &&
               unbound_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               unbound_witness.unbound_acceptor_branch_ok() &&
               broken_open_witness.verdict() == semantic::Verdict::standing &&
               broken_open_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               broken_open_witness.open_unbound_handler_branch_ok() &&
               broken_request_witness.verdict() ==
                   semantic::Verdict::standing &&
               broken_request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               broken_request_witness.channel_missing_branch_ok();
    }

    [[nodiscard]] bool probe_dispatcher_facade_integration() noexcept
    {
        std::array<EchoChannelState, 2> channel_states{};
        std::array<EchoChannel, 2> channels{
            EchoChannel{.state = &channel_states[0]},
            EchoChannel{.state = &channel_states[1]},
        };
        EchoAcceptorState accept_state{};
        EchoAcceptor echo_acceptor{
            .state = &accept_state,
            .channels = &channels,
        };
        AcceptorTrace acceptor_trace{};
        auto service_acceptor =
            kernel::make_task_message_session_service_acceptor<2>(
                echo_acceptor,
                "echo-facade",
                &acceptor_trace);
        DispatchTrace dispatch_trace{};
        auto dispatcher = kernel::make_task_message_session_dispatcher<1, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 1>{
                kernel::task_message_session_service_acceptor_entry(
                    kServiceId,
                    "echo-facade",
                    service_acceptor),
            },
            &dispatch_trace);
        dispatcher.bind_next_session_handle(kBaseSessionHandle);

        const auto open = dispatcher.open_session(kServiceId, kOpenPayload);
        const auto request = dispatcher.request_session(
            kBaseSessionHandle, kRequestOperation, kRequestPayload);
        const auto close =
            dispatcher.close_session(kBaseSessionHandle, kCloseReason);
        const auto reopen = dispatcher.open_session(kServiceId, kSecondOpenPayload);

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

        const auto accept_first_witness =
            kernel::task_message_session_service_acceptor_witness(
                *accept_first);
        const auto accept_second_witness =
            kernel::task_message_session_service_acceptor_witness(
                *accept_second);
        const auto accept_third_witness =
            kernel::task_message_session_service_acceptor_witness(
                *accept_third);
        const auto accept_terminal_witness =
            kernel::task_message_session_service_acceptor_witness(
                acceptor_trace);

        return dispatcher.valid() && service_acceptor.valid() &&
               trap_result_matches(open.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open.action == kernel::TaskMessageSessionActionKind::open &&
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
               request.action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request.matched && request.handler_valid &&
               request.session_found &&
               request.service_slot == 0u && request.session_slot == 0u &&
               trap_result_matches(close.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               close.action == kernel::TaskMessageSessionActionKind::close &&
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
               channel_states[0].request_calls == 1u &&
               channel_states[0].close_calls == 1u &&
               channel_states[0].last_session_handle == kBaseSessionHandle &&
               channel_states[0].last_open_payload == kOpenPayload &&
               dispatch_trace.size() == 4u &&
               dispatch_first->sequence == 1u &&
               dispatch_first->action ==
                   kernel::TaskMessageSessionActionKind::open &&
               dispatch_first->session_handle == kBaseSessionHandle &&
               same_text(dispatch_first->service_name, "echo-facade"sv) &&
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
               same_text(accept_first->service_name, "echo-facade"sv) &&
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
               accept_fourth->channel_bound &&
               accept_first_witness.verdict() ==
                   semantic::Verdict::standing &&
               accept_first_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               accept_first_witness.open_bound_branch_ok() &&
               accept_second_witness.verdict() ==
                   semantic::Verdict::standing &&
               accept_second_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               accept_second_witness.request_branch_ok() &&
               accept_third_witness.verdict() ==
                   semantic::Verdict::standing &&
               accept_third_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               accept_third_witness.close_branch_ok() &&
               accept_terminal_witness.verdict() ==
                   semantic::Verdict::standing &&
               accept_terminal_witness.open_bound_branch_ok();
    }
}

int main()
{
    const bool direct_ok = demo::probe_direct_service_acceptor_lifecycle();
    const bool invalid_ok = demo::probe_unbound_and_broken_acceptors();
    const bool dispatcher_ok = demo::probe_dispatcher_facade_integration();
    const bool ok = direct_ok && invalid_ok && dispatcher_ok;

    std::printf(
        "[runtime-task-message-session-acceptor-demo] ok=%d direct=%d invalid=%d dispatcher=%d\n",
        ok ? 1 : 0,
        direct_ok ? 1 : 0,
        invalid_ok ? 1 : 0,
        dispatcher_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-acceptor-witness] ok=%d collapsed=%s summary=%s\n",
        ok ? 1 : 0,
        semantic::verdict_name(
            kernel::TaskMessageSessionServiceAcceptorWitness{}.verdict()),
        kernel::TaskMessageSessionServiceAcceptorWitness{}
            .summary_path()
            .data());
    return ok ? 0 : 1;
}
