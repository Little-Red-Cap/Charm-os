#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_session_service;
import semantic.core;

namespace demo {
    using namespace std::literals;

    using Tick = std::uint64_t;

    inline constexpr std::uint64_t kServiceId{0x51u};
    inline constexpr std::uint64_t kBaseSessionHandle{0x9000u};
    inline constexpr std::uint64_t kOpenPayload{0xAAu};
    inline constexpr std::uint64_t kRequestOperation{0x21u};
    inline constexpr std::uint64_t kRequestPayload{33u};
    inline constexpr std::uint64_t kCloseReason{0x77u};
    inline constexpr Tick kBootstrapDue{7u};
    inline constexpr Tick kOpenDue{11u};
    inline constexpr Tick kRequestDue{13u};
    inline constexpr Tick kCloseDue{17u};
    inline constexpr std::size_t kBudget{2u};

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
                                  request_view.payload);
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
            return handled_result(channel.session_handle);
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

    struct FakePumpState {
        bool bound{true};
        bool wait_ok{true};
        std::uint32_t wait_calls{0};
        std::uint32_t step_calls{0};
        Tick last_wait_due{0};
        Tick last_due{0};
        std::size_t last_budget{0};
        kernel::Event last_event{};
        kernel::Event bootstrap{
            kernel::make_event(kernel::EventId::user0, 1u)};
        kernel::Event receive{
            kernel::make_event(kernel::EventId::message,
                               kernel::runtime_mailbox_receive_ready_code)};
        kernel::Event timeout{
            kernel::make_event(kernel::EventId::sync,
                               kernel::runtime_mailbox_receive_timeout_code)};
        kernel::TaskMessageServicePumpResult next_result{};
    };

    struct FakePump {
        using tick_type = Tick;
        using result_type = kernel::TaskMessageServicePumpResult;

        FakePumpState* state{nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] kernel::Event bootstrap_event() const noexcept
        {
            return valid() ? state->bootstrap
                           : kernel::make_event(kernel::EventId::user0);
        }

        void bind_bootstrap_event(kernel::Event event) noexcept
        {
            if (!valid()) {
                return;
            }

            state->bootstrap = event;
        }

        [[nodiscard]] kernel::Event receive_event() const noexcept
        {
            return valid() ? state->receive
                           : kernel::make_event(kernel::EventId::message);
        }

        [[nodiscard]] kernel::Event receive_timeout_event() const noexcept
        {
            return valid() ? state->timeout
                           : kernel::make_event(kernel::EventId::sync);
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->wait_calls;
            state->last_wait_due = due;
            return state->wait_ok;
        }

        [[nodiscard]] result_type step(kernel::Event event,
                                       std::size_t budget,
                                       tick_type due) noexcept
        {
            if (!valid()) {
                return result_type{};
            }

            ++state->step_calls;
            state->last_event = event;
            state->last_budget = budget;
            state->last_due = due;
            return state->next_result;
        }
    };

    using AcceptorTrace = kernel::TaskMessageSessionServiceAcceptorTraceBuffer<8>;
    using ServiceTrace = kernel::TaskMessageSessionServiceTraceBuffer<8>;
    using ServiceAcceptor =
        kernel::TaskMessageSessionServiceAcceptor<2, AcceptorTrace>;
    using Dispatcher = kernel::TaskMessageSessionDispatcher<1, 2>;
    using SessionService =
        kernel::TaskMessageSessionService<FakePump,
                                          Dispatcher,
                                          ServiceAcceptor,
                                          ServiceTrace>;

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

    [[nodiscard]] bool inspect_service_trace(ServiceTrace& trace) noexcept
    {
        const auto* bootstrap = trace.at(0u);
        const auto* open = trace.at(1u);
        const auto* request = trace.at(2u);
        const auto* close = trace.at(3u);
        if (bootstrap == nullptr || open == nullptr || request == nullptr ||
            close == nullptr) {
            return false;
        }

        const auto bootstrap_witness =
            kernel::task_message_session_service_witness(*bootstrap);
        const auto open_witness =
            kernel::task_message_session_service_witness(*open);
        const auto request_witness =
            kernel::task_message_session_service_witness(*request);
        const auto close_witness =
            kernel::task_message_session_service_witness(*close);
        const auto terminal_witness =
            kernel::task_message_session_service_witness(trace);
        const auto handoff =
            kernel::task_message_session_service_witness_handoff_target(
                terminal_witness);

        return trace.size() == 4u &&
               bootstrap->sequence == 1u &&
               bootstrap->reason ==
                   kernel::TaskMessageServicePumpReason::bootstrap &&
               bootstrap->event_id == kernel::EventId::user0 &&
               bootstrap->event_value == 7u &&
               bootstrap->due == kBootstrapDue &&
               bootstrap->budget == kBudget &&
               bootstrap->served == 0u &&
               bootstrap->active_sessions == 0u &&
               bootstrap->active_channels == 0u &&
               bootstrap->progressed &&
               bootstrap->bootstrap_consumed &&
               bootstrap->wait_armed &&
               !bootstrap->hold_ready &&
               !bootstrap->dispatch_accepted &&
               open->sequence == 2u &&
               open->reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               open->event_id == kernel::EventId::message &&
               open->event_value == kernel::runtime_mailbox_receive_ready_code &&
               open->due == kOpenDue &&
               open->budget == kBudget &&
               open->served == 1u &&
               open->active_sessions == 1u &&
               open->active_channels == 1u &&
               open->progressed &&
               !open->bootstrap_consumed &&
               open->wait_armed &&
               !open->hold_ready &&
               open->dispatch_accepted &&
               open->dispatch_handled &&
               open->dispatch_replied &&
               open->reply_value == kBaseSessionHandle &&
               request->sequence == 3u &&
               request->reason ==
                   kernel::TaskMessageServicePumpReason::budget_reached &&
               request->event_id == kernel::EventId::message &&
               request->due == kRequestDue &&
               request->budget == kBudget &&
               request->served == 1u &&
               request->active_sessions == 1u &&
               request->active_channels == 1u &&
               request->progressed &&
               !request->bootstrap_consumed &&
               !request->wait_armed &&
               request->hold_ready &&
               request->dispatch_accepted &&
               request->dispatch_handled &&
               request->dispatch_replied &&
               request->reply_value ==
                   kBaseSessionHandle + kRequestOperation + kRequestPayload &&
               close->sequence == 4u &&
               close->reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               close->event_id == kernel::EventId::message &&
               close->due == kCloseDue &&
               close->budget == kBudget &&
               close->served == 1u &&
               close->active_sessions == 0u &&
               close->active_channels == 0u &&
               close->progressed &&
               !close->bootstrap_consumed &&
               close->wait_armed &&
               !close->hold_ready &&
               close->dispatch_accepted &&
               close->dispatch_handled &&
               close->dispatch_replied &&
               close->reply_value == kCloseReason + 1u &&
               kernel::task_message_session_service_witness_ready(
                   bootstrap_witness) &&
               bootstrap_witness.verdict() == semantic::Verdict::standing &&
               bootstrap_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               bootstrap_witness.bootstrap_branch_ok() &&
               open_witness.verdict() == semantic::Verdict::standing &&
               open_witness.failure_domain() == semantic::FailureDomain::none &&
               open_witness.dispatch_branch_ok() &&
               request_witness.verdict() == semantic::Verdict::standing &&
               request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               request_witness.dispatch_branch_ok() &&
               close_witness.verdict() == semantic::Verdict::standing &&
               close_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               close_witness.dispatch_branch_ok() &&
               terminal_witness.verdict() == semantic::Verdict::standing &&
               terminal_witness.dispatch_branch_ok() &&
               std::string_view{handoff.entry_name()} ==
                   "task-message-session-service-witness"sv &&
               std::string_view{handoff.selected_summary_path()} ==
                   "task-message-session-service-witness.summary"sv;
    }

    [[nodiscard]] bool probe_session_service_facade() noexcept
    {
        SessionService unbound{};
        const auto unbound_wait = unbound.wait_receive_until(3u);
        const auto unbound_step = unbound.step(
            kernel::make_event(kernel::EventId::user0, 1u),
            kBudget,
            5u);

        std::array<EchoChannelState, 2> channel_states{};
        std::array<EchoChannel, 2> channels{
            EchoChannel{
                .state = &channel_states[0],
            },
            EchoChannel{
                .state = &channel_states[1],
            },
        };
        EchoAcceptorState acceptor_state{};
        EchoAcceptor echo_acceptor{
            .state = &acceptor_state,
            .channels = &channels,
        };
        AcceptorTrace acceptor_trace{};
        auto service_acceptor =
            kernel::make_task_message_session_service_acceptor<2>(
                echo_acceptor,
                "echo-session",
                &acceptor_trace);
        auto session_dispatcher = kernel::make_task_message_session_dispatcher<1, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 1>{
                kernel::task_message_session_service_acceptor_entry(
                    kServiceId,
                    "echo-session",
                    service_acceptor),
            });
        session_dispatcher.bind_next_session_handle(kBaseSessionHandle);

        FakePumpState pump_state{};
        ServiceTrace service_trace{};
        auto session_service = kernel::make_task_message_session_service(
            FakePump{
                .state = &pump_state,
            },
            session_dispatcher,
            service_acceptor,
            &service_trace);

        const auto rebound_bootstrap =
            kernel::make_event(kernel::EventId::user0, 7u);
        session_service.bind_bootstrap_event(rebound_bootstrap);
        const bool wait_ok = session_service.wait_receive_until(5u);

        pump_state.next_result = kernel::TaskMessageServicePumpResult{
            .progressed = true,
            .bootstrap_consumed = true,
            .wait_armed = true,
            .reason = kernel::TaskMessageServicePumpReason::bootstrap,
        };
        const auto bootstrap_step = session_service.step(
            rebound_bootstrap, kBudget, kBootstrapDue);
        const auto bootstrap_witness =
            kernel::task_message_session_service_witness(bootstrap_step);

        const auto open = session_dispatcher.open_session(kServiceId, kOpenPayload);
        const auto open_lookup = session_service.lookup_session(kBaseSessionHandle);
        const auto open_channel = session_service.lookup_channel(kBaseSessionHandle);
        pump_state.next_result = kernel::TaskMessageServicePumpResult{
            .progressed = true,
            .wait_armed = true,
            .reason = kernel::TaskMessageServicePumpReason::queue_empty,
            .drain = kernel::TaskMessageServiceDrainResult{
                .progressed = true,
                .served = 1u,
                .stop_reason =
                    kernel::TaskMessageServiceDrainStopReason::queue_empty,
                .last_dispatch = kernel::TaskMessageDispatchResult{
                    .accepted = true,
                    .matched = true,
                    .handler_valid = true,
                    .handled = true,
                    .replied = true,
                    .reply_value = open.trap.value,
                },
            },
        };
        const auto open_step = session_service.step(
            pump_state.receive, kBudget, kOpenDue);
        const auto open_witness =
            kernel::task_message_session_service_witness(open_step);

        const auto request = session_dispatcher.request_session(
            kBaseSessionHandle, kRequestOperation, kRequestPayload);
        pump_state.next_result = kernel::TaskMessageServicePumpResult{
            .progressed = true,
            .hold_ready = true,
            .reason = kernel::TaskMessageServicePumpReason::budget_reached,
            .drain = kernel::TaskMessageServiceDrainResult{
                .progressed = true,
                .served = 1u,
                .stop_reason =
                    kernel::TaskMessageServiceDrainStopReason::budget_reached,
                .last_dispatch = kernel::TaskMessageDispatchResult{
                    .accepted = true,
                    .matched = true,
                    .handler_valid = true,
                    .handled = true,
                    .replied = true,
                    .reply_value = request.trap.value,
                },
            },
        };
        const auto request_step = session_service.step(
            pump_state.receive, kBudget, kRequestDue);
        const auto request_witness =
            kernel::task_message_session_service_witness(request_step);

        const auto close =
            session_dispatcher.close_session(kBaseSessionHandle, kCloseReason);
        const auto closed_lookup = session_service.lookup_session(kBaseSessionHandle);
        const auto closed_channel =
            session_service.lookup_channel(kBaseSessionHandle);
        pump_state.next_result = kernel::TaskMessageServicePumpResult{
            .progressed = true,
            .wait_armed = true,
            .reason = kernel::TaskMessageServicePumpReason::queue_empty,
            .drain = kernel::TaskMessageServiceDrainResult{
                .progressed = true,
                .served = 1u,
                .stop_reason =
                    kernel::TaskMessageServiceDrainStopReason::queue_empty,
                .last_dispatch = kernel::TaskMessageDispatchResult{
                    .accepted = true,
                    .matched = true,
                    .handler_valid = true,
                    .handled = true,
                    .replied = true,
                    .reply_value = close.trap.value,
                },
            },
        };
        const auto close_step = session_service.step(
            pump_state.receive, kBudget, kCloseDue);
        const auto close_witness =
            kernel::task_message_session_service_witness(close_step);

        const auto* closed_session_slot = session_service.session(0u);
        const auto* closed_channel_slot = session_service.channel(0u);

        const bool trace_ok = inspect_service_trace(service_trace);
        const bool acceptor_ok =
            acceptor_state.accept_calls == 1u &&
            acceptor_state.last_service_id == kServiceId &&
            acceptor_state.last_session_handle == kBaseSessionHandle &&
            acceptor_state.last_open_payload == kOpenPayload &&
            acceptor_state.last_channel_slot == 0u &&
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
            acceptor_trace.size() == 3u;

        return !unbound.valid() && !unbound_wait &&
               unbound_step.reason ==
                   kernel::TaskMessageServicePumpReason::none &&
               unbound_step.active_sessions == 0u &&
               unbound_step.active_channels == 0u &&
               session_service.valid() &&
               same_text(session_service.service_name(), "echo-session"sv) &&
               session_service.bootstrap_event().id == kernel::EventId::user0 &&
               kernel::payload_u64(session_service.bootstrap_event()) == 7u &&
               wait_ok && pump_state.wait_calls == 1u &&
               pump_state.last_wait_due == 5u &&
               bootstrap_step.progressed &&
               bootstrap_step.bootstrap_consumed &&
               bootstrap_step.wait_armed &&
               bootstrap_step.reason ==
                   kernel::TaskMessageServicePumpReason::bootstrap &&
               bootstrap_step.active_sessions == 0u &&
               bootstrap_step.active_channels == 0u &&
               bootstrap_witness.verdict() ==
                   semantic::Verdict::standing &&
               bootstrap_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               bootstrap_witness.bootstrap_branch_ok() &&
               trap_result_matches(open.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open_lookup.matched && open_channel.matched &&
               open_lookup.slot == 0u && open_channel.slot == 0u &&
               open_step.progressed && !open_step.bootstrap_consumed &&
               open_step.wait_armed &&
               open_step.reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               open_step.active_sessions == 1u &&
               open_step.active_channels == 1u &&
               open_step.dispatch_accepted &&
               open_step.dispatch_handled &&
               open_step.dispatch_replied &&
               open_step.reply_value == kBaseSessionHandle &&
               open_witness.verdict() == semantic::Verdict::standing &&
               open_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               open_witness.dispatch_branch_ok() &&
               trap_result_matches(request.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + kRequestOperation +
                                       kRequestPayload) &&
               request_step.progressed && request_step.hold_ready &&
               !request_step.wait_armed &&
               request_step.reason ==
                   kernel::TaskMessageServicePumpReason::budget_reached &&
               request_step.active_sessions == 1u &&
               request_step.active_channels == 1u &&
               request_step.reply_value ==
                   kBaseSessionHandle + kRequestOperation + kRequestPayload &&
               request_witness.verdict() == semantic::Verdict::standing &&
               request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               request_witness.dispatch_branch_ok() &&
               trap_result_matches(close.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason + 1u) &&
               close_step.progressed && close_step.wait_armed &&
               close_step.reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               close_step.active_sessions == 0u &&
               close_step.active_channels == 0u &&
               close_step.reply_value == kCloseReason + 1u &&
               close_witness.verdict() == semantic::Verdict::standing &&
               close_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               close_witness.dispatch_branch_ok() &&
               !closed_lookup.matched && !closed_channel.matched &&
               closed_session_slot != nullptr && !closed_session_slot->active &&
               closed_channel_slot != nullptr && !closed_channel_slot->active &&
               pump_state.step_calls == 4u &&
               pump_state.last_event.id == kernel::EventId::message &&
               pump_state.last_budget == kBudget &&
               pump_state.last_due == kCloseDue &&
               trace_ok && acceptor_ok;
    }
}

int main()
{
    const bool facade_ok = demo::probe_session_service_facade();
    std::printf("[runtime-task-message-session-service-demo] ok=%d facade=%d\n",
                facade_ok ? 1 : 0,
                facade_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-service-witness] ok=%d collapsed=%s summary=%s\n",
        facade_ok ? 1 : 0,
        semantic::verdict_name(
            kernel::TaskMessageSessionServiceWitness{}.verdict()),
        kernel::TaskMessageSessionServiceWitness{}.summary_path().data());
    return facade_ok ? 0 : 1;
}
