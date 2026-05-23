#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_session_dispatch;
import kernel.task_syscall_table;
import semantic.core;

namespace demo {
    using namespace std::literals;

    inline constexpr std::uint64_t kEchoServiceId{0x51u};
    inline constexpr std::uint64_t kAuditServiceId{0x61u};
    inline constexpr std::uint64_t kGhostServiceId{0x99u};
    inline constexpr std::uint64_t kBaseSessionHandle{0x9000u};

    struct EchoServiceState {
        std::uint32_t open_calls{0};
        std::uint32_t request_calls{0};
        std::uint32_t close_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_payload{0};
        std::uint64_t last_operation{0};
        std::uint64_t last_reason{0};
    };

    struct EchoService {
        EchoServiceState* state{nullptr};

        [[nodiscard]] kernel::TrapResult open(
            kernel::TaskMessageSessionOpenDispatchView open) const noexcept
        {
            ++state->open_calls;
            state->last_service_id = open.service_id;
            state->last_session_handle = open.session_handle;
            state->last_payload = open.payload;
            return handled_result(0u);
        }

        [[nodiscard]] kernel::TrapResult request(
            kernel::TaskMessageSessionRequestDispatchView request) const noexcept
        {
            ++state->request_calls;
            state->last_service_id = request.service_id;
            state->last_session_handle = request.session_handle;
            state->last_operation = request.operation;
            state->last_payload = request.payload;
            return handled_result(request.service_id + request.operation +
                                  request.payload);
        }

        [[nodiscard]] kernel::TrapResult close(
            kernel::TaskMessageSessionCloseDispatchView close) const noexcept
        {
            ++state->close_calls;
            state->last_service_id = close.service_id;
            state->last_session_handle = close.session_handle;
            state->last_reason = close.reason;
            return handled_result(close.reason);
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

    struct AuditServiceState {
        std::uint32_t open_calls{0};
        std::uint32_t request_calls{0};
        std::uint32_t close_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_payload{0};
        std::uint64_t last_operation{0};
        std::uint64_t last_reason{0};
    };

    struct AuditService {
        AuditServiceState* state{nullptr};

        [[nodiscard]] kernel::TrapResult open(
            kernel::TaskMessageSessionOpenDispatchView open) const noexcept
        {
            ++state->open_calls;
            state->last_service_id = open.service_id;
            state->last_session_handle = open.session_handle;
            state->last_payload = open.payload;
            return handled_result(open.payload + 1u);
        }

        [[nodiscard]] kernel::TrapResult request(
            kernel::TaskMessageSessionRequestDispatchView request) const noexcept
        {
            ++state->request_calls;
            state->last_service_id = request.service_id;
            state->last_session_handle = request.session_handle;
            state->last_operation = request.operation;
            state->last_payload = request.payload;
            return handled_result(request.session_handle ^ request.payload);
        }

        [[nodiscard]] kernel::TrapResult close(
            kernel::TaskMessageSessionCloseDispatchView close) const noexcept
        {
            ++state->close_calls;
            state->last_service_id = close.service_id;
            state->last_session_handle = close.session_handle;
            state->last_reason = close.reason;
            return handled_result(close.reason + 1u);
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

    using SessionTrace = kernel::TaskMessageSessionDispatchTraceBuffer<16>;
    using SessionDispatcher = kernel::TaskMessageSessionDispatcher<2, 2, SessionTrace>;
    using SyscallTableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
    using SyscallTable = kernel::TaskSyscallTable<1, SyscallTableTrace>;

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

    [[nodiscard]] bool probe_direct_session_dispatch() noexcept
    {
        EchoServiceState echo_state{};
        AuditServiceState audit_state{};
        EchoService echo{
            .state = &echo_state,
        };
        AuditService audit{
            .state = &audit_state,
        };
        SessionTrace trace{};
        auto dispatcher = kernel::make_task_message_session_dispatcher<2, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 2>{
                kernel::task_message_session_handler_entry(
                    kEchoServiceId,
                    "echo-session",
                    kernel::make_task_message_session_handler(echo)),
                kernel::task_message_session_handler_entry(
                    kAuditServiceId,
                    "audit-session",
                    kernel::make_task_message_session_handler(audit)),
            },
            &trace);
        dispatcher.bind_next_session_handle(kBaseSessionHandle);

        const auto open_echo =
            dispatcher.open_session(kEchoServiceId, 0xAAu);
        const auto open_audit =
            dispatcher.open_session(kAuditServiceId, 0xBBu);
        const auto open_full =
            dispatcher.open_session(kEchoServiceId, 0xCCu);
        const auto request_echo = dispatcher.request_session(
            kBaseSessionHandle, 0x21u, 33u);
        const auto close_echo =
            dispatcher.close_session(kBaseSessionHandle, 0x77u);
        const auto reopen_echo =
            dispatcher.open_session(kEchoServiceId, 0xDDu);

        const auto* slot0 = dispatcher.session(0u);
        const auto* slot1 = dispatcher.session(1u);
        const auto* first = trace.at(0u);
        const auto* third = trace.at(2u);
        const auto* fifth = trace.at(4u);
        const auto* sixth = trace.at(5u);
        if (slot0 == nullptr || slot1 == nullptr || first == nullptr ||
            third == nullptr || fifth == nullptr || sixth == nullptr) {
            return false;
        }

        const auto first_witness =
            kernel::task_message_session_dispatch_witness(*first);
        const auto third_witness =
            kernel::task_message_session_dispatch_witness(*third);
        const auto request_witness =
            kernel::task_message_session_dispatch_witness(request_echo);
        const auto fifth_witness =
            kernel::task_message_session_dispatch_witness(*fifth);
        const auto terminal_witness =
            kernel::task_message_session_dispatch_witness(trace);
        const auto handoff =
            kernel::task_message_session_dispatch_witness_handoff_target(
                terminal_witness);

        return dispatcher.valid() &&
               trap_result_matches(open_echo.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               open_echo.action == kernel::TaskMessageSessionActionKind::open &&
               open_echo.matched && open_echo.handler_valid &&
               open_echo.session_allocated &&
               open_echo.service_slot == 0u &&
               open_echo.session_slot == 0u &&
               open_echo.service_id == kEchoServiceId &&
               open_echo.session_handle == kBaseSessionHandle &&
               trap_result_matches(open_audit.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + 1u) &&
               open_audit.session_allocated &&
               open_audit.service_slot == 1u &&
               open_audit.session_slot == 1u &&
               trap_result_matches(open_full.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument) &&
               !open_full.session_allocated &&
               open_full.service_slot == 0u &&
               open_full.session_slot ==
                   kernel::task_message_session_unmapped_slot &&
               trap_result_matches(request_echo.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kEchoServiceId + 0x21u + 33u) &&
               request_echo.action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request_echo.session_found &&
               request_echo.session_slot == 0u &&
               request_echo.service_slot == 0u &&
               trap_result_matches(close_echo.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x77u) &&
               close_echo.action ==
                   kernel::TaskMessageSessionActionKind::close &&
               close_echo.session_found && close_echo.session_closed &&
               close_echo.session_slot == 0u &&
               trap_result_matches(reopen_echo.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle + 2u) &&
               reopen_echo.session_allocated &&
               reopen_echo.session_slot == 0u &&
               dispatcher.active_sessions() == 2u &&
               slot0->active && slot1->active &&
               slot0->service_id == kEchoServiceId &&
               slot0->session_handle == kBaseSessionHandle + 2u &&
               slot1->service_id == kAuditServiceId &&
               slot1->session_handle == kBaseSessionHandle + 1u &&
               echo_state.open_calls == 2u &&
               echo_state.request_calls == 1u &&
               echo_state.close_calls == 1u &&
               echo_state.last_service_id == kEchoServiceId &&
               echo_state.last_session_handle == kBaseSessionHandle + 2u &&
               echo_state.last_payload == 0xDDu &&
               echo_state.last_operation == 0x21u &&
               echo_state.last_reason == 0x77u &&
               audit_state.open_calls == 1u &&
               audit_state.request_calls == 0u &&
               audit_state.close_calls == 0u &&
               audit_state.last_service_id == kAuditServiceId &&
               audit_state.last_session_handle == kBaseSessionHandle + 1u &&
               audit_state.last_payload == 0xBBu &&
               trace.size() == 6u &&
               first->sequence == 1u &&
               first->action == kernel::TaskMessageSessionActionKind::open &&
               first->service_id == kEchoServiceId &&
               same_text(first->service_name, "echo-session"sv) &&
               first->session_handle == kBaseSessionHandle &&
               first->session_allocated &&
               third->sequence == 3u &&
               third->action == kernel::TaskMessageSessionActionKind::open &&
               third->service_id == kEchoServiceId &&
               third->matched && third->handler_valid &&
               !third->session_allocated &&
               third->error == kernel::TrapError::invalid_argument &&
               fifth->sequence == 5u &&
               fifth->action == kernel::TaskMessageSessionActionKind::close &&
               fifth->session_found && fifth->session_closed &&
               fifth->session_handle == kBaseSessionHandle &&
               sixth->sequence == 6u &&
               sixth->session_handle == kBaseSessionHandle + 2u &&
               sixth->session_slot == 0u &&
               same_text(
                   kernel::task_message_session_action_kind_name(sixth->action),
                   "open"sv) &&
               kernel::task_message_session_dispatch_witness_ready(
                   first_witness) &&
               first_witness.verdict() == semantic::Verdict::standing &&
               first_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               first_witness.open_branch_ok() &&
               third_witness.verdict() == semantic::Verdict::standing &&
               third_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               third_witness.open_branch_ok() &&
               request_witness.verdict() == semantic::Verdict::standing &&
               request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               request_witness.request_branch_ok() &&
               fifth_witness.verdict() == semantic::Verdict::standing &&
               fifth_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               fifth_witness.close_branch_ok() &&
               terminal_witness.verdict() == semantic::Verdict::standing &&
               terminal_witness.open_branch_ok() &&
               std::string_view{handoff.entry_name()} ==
                   "task-message-session-dispatch-witness"sv &&
               std::string_view{handoff.selected_summary_path()} ==
                   "task-message-session-dispatch-witness.summary"sv;
    }

    [[nodiscard]] bool probe_table_integration_and_errors() noexcept
    {
        EchoServiceState echo_state{};
        EchoService echo{
            .state = &echo_state,
        };
        SessionTrace session_trace{};
        SyscallTableTrace table_trace{};
        auto dispatcher = kernel::make_task_message_session_dispatcher<2, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 2>{
                kernel::task_message_session_handler_entry(
                    kEchoServiceId,
                    "echo-session",
                    kernel::make_task_message_session_handler(echo)),
                kernel::task_message_session_handler_entry(
                    kAuditServiceId,
                    "audit-session"),
            },
            &session_trace);
        dispatcher.bind_next_session_handle(kBaseSessionHandle);
        auto table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 1>{
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::capability_call,
                    kernel::make_task_syscall_handler(dispatcher)),
            },
            &table_trace);

        const auto unsupported_open = table.capability_call(
            kGhostServiceId,
            kernel::task_message_session_open_operation,
            1u);
        const auto unbound_open = table.capability_call(
            kAuditServiceId,
            kernel::task_message_session_open_operation,
            2u);
        const auto opened = table.capability_call(
            kEchoServiceId,
            kernel::task_message_session_open_operation,
            3u);
        const auto invalid_request = table.capability_call(0xDEADu, 0x31u, 4u);
        const auto valid_request =
            table.capability_call(kBaseSessionHandle, 0x31u, 4u);
        const auto invalid_close = table.capability_call(
            0xBEEFu,
            kernel::task_message_session_close_operation,
            5u);
        const auto valid_close = table.capability_call(
            kBaseSessionHandle,
            kernel::task_message_session_close_operation,
            6u);
        const auto unsupported_syscall = table.dispatch(kernel::TaskSyscallRequest{
            .syscall = kernel::TaskSyscallId::debug_write,
            .arg0 = 0x44u,
        });

        const auto* session_first = session_trace.at(0u);
        const auto* session_second = session_trace.at(1u);
        const auto* session_third = session_trace.at(2u);
        const auto* session_fourth = session_trace.at(3u);
        const auto* session_fifth = session_trace.at(4u);
        const auto* table_first = table_trace.at(0u);
        const auto* table_last = table_trace.at(7u);
        if (session_first == nullptr || session_second == nullptr ||
            session_third == nullptr || session_fourth == nullptr ||
            session_fifth == nullptr || table_first == nullptr ||
            table_last == nullptr) {
            return false;
        }

        const auto missing_service_witness =
            kernel::task_message_session_dispatch_witness(*session_first);
        const auto unbound_service_witness =
            kernel::task_message_session_dispatch_witness(*session_second);
        const auto opened_witness =
            kernel::task_message_session_dispatch_witness(*session_third);
        const auto invalid_request_witness =
            kernel::task_message_session_dispatch_witness(*session_fourth);
        const auto valid_request_witness =
            kernel::task_message_session_dispatch_witness(*session_fifth);
        const auto unsupported_syscall_witness =
            kernel::task_message_session_dispatch_witness(
                kernel::TaskMessageSessionDispatchTraceEvent{
                    .sequence = table_last->sequence,
                    .syscall = table_last->syscall,
                    .disposition = table_last->disposition,
                    .error = table_last->error,
                    .value = table_last->value,
                });

        return trap_result_matches(unsupported_open,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               trap_result_matches(unbound_open,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               trap_result_matches(opened,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               trap_result_matches(invalid_request,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument) &&
               trap_result_matches(valid_request,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kEchoServiceId + 0x31u + 4u) &&
               trap_result_matches(invalid_close,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::invalid_argument) &&
               trap_result_matches(valid_close,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   6u) &&
               trap_result_matches(unsupported_syscall,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               dispatcher.active_sessions() == 0u &&
               echo_state.open_calls == 1u &&
               echo_state.request_calls == 1u &&
               echo_state.close_calls == 1u &&
               session_trace.size() == 7u &&
               session_first->sequence == 1u &&
               session_first->action ==
                   kernel::TaskMessageSessionActionKind::open &&
               session_first->service_id == kGhostServiceId &&
               !session_first->matched &&
               session_first->error ==
                   kernel::TrapError::unsupported_service &&
               session_second->sequence == 2u &&
               session_second->matched &&
               !session_second->handler_valid &&
               session_second->error == kernel::TrapError::unbound_adapter &&
               session_third->sequence == 3u &&
               session_third->session_allocated &&
               session_third->session_handle == kBaseSessionHandle &&
               session_fourth->sequence == 4u &&
               session_fourth->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               !session_fourth->session_found &&
               session_fourth->error == kernel::TrapError::invalid_argument &&
               session_fifth->sequence == 5u &&
               session_fifth->session_found &&
               session_fifth->session_handle == kBaseSessionHandle &&
               session_fifth->value == kEchoServiceId + 0x31u + 4u &&
               table_trace.size() == 8u &&
               table_first->sequence == 1u &&
               table_first->syscall ==
                   kernel::TaskSyscallId::capability_call &&
               table_first->slot == 0u &&
               table_first->matched &&
               table_first->handler_valid &&
               table_first->error ==
                   kernel::TrapError::unsupported_service &&
               table_last->sequence == 8u &&
               table_last->syscall == kernel::TaskSyscallId::debug_write &&
               table_last->slot == kernel::task_syscall_table_unmapped_slot &&
               !table_last->matched &&
               !table_last->handler_valid &&
               table_last->error ==
                   kernel::TrapError::unsupported_service &&
               missing_service_witness.verdict() ==
                   semantic::Verdict::standing &&
               missing_service_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               missing_service_witness.service_missing_branch_ok() &&
               unbound_service_witness.verdict() ==
                   semantic::Verdict::standing &&
               unbound_service_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               unbound_service_witness.service_unbound_branch_ok() &&
               opened_witness.verdict() == semantic::Verdict::standing &&
               opened_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               opened_witness.open_branch_ok() &&
               invalid_request_witness.verdict() ==
                   semantic::Verdict::standing &&
               invalid_request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               invalid_request_witness.session_missing_branch_ok() &&
               valid_request_witness.verdict() ==
                   semantic::Verdict::standing &&
               valid_request_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               valid_request_witness.request_branch_ok() &&
               unsupported_syscall_witness.verdict() ==
                   semantic::Verdict::standing &&
               unsupported_syscall_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               unsupported_syscall_witness.unsupported_syscall_branch_ok();
    }
}

int main()
{
    const bool direct_ok = demo::probe_direct_session_dispatch();
    const bool table_ok = demo::probe_table_integration_and_errors();
    const bool ok = direct_ok && table_ok;

    std::printf(
        "[runtime-task-message-session-dispatch-demo] ok=%d direct=%d table=%d\n",
        ok ? 1 : 0,
        direct_ok ? 1 : 0,
        table_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-dispatch-witness] ok=%d collapsed=%s summary=%s\n",
        ok ? 1 : 0,
        semantic::verdict_name(
            kernel::TaskMessageSessionDispatchWitness{}.verdict()),
        kernel::TaskMessageSessionDispatchWitness{}.summary_path().data());
    return ok ? 0 : 1;
}
