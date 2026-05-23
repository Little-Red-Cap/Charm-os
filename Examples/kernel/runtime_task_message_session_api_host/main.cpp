#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.evt;
import kernel.task_message_session_api;
import semantic.core;

namespace demo {
    using Tick = std::uint64_t;
    inline constexpr auto kInvalidSyscall =
        static_cast<kernel::TaskSyscallId>(99u);
    inline constexpr kernel::TaskId kDefaultOwner{7u};
    inline constexpr kernel::TaskId kTimeoutOwner{21u};
    inline constexpr std::uint64_t kBaseToken{0xC1u};
    inline constexpr std::uint64_t kBaseSequence{0x61u};
    inline constexpr std::uint64_t kServiceId{0x51u};
    inline constexpr std::uint64_t kOpenPayload{0xAAu};
    inline constexpr std::uint64_t kRequestOperation{0x21u};
    inline constexpr std::uint64_t kRequestPayload{33u};
    inline constexpr std::uint64_t kCloseReason{0x77u};
    inline constexpr std::uint64_t kOpenedHandleBase{0x9000u};
    inline constexpr std::uint64_t kOpenedHandle{kOpenedHandleBase + kServiceId};
    inline constexpr std::uint64_t kReplyTimeoutHandle{0x9222u};
    inline constexpr std::uint64_t kTimeoutToken{0xDFu};
    inline constexpr std::uint64_t kTimeoutSequence{0x7Fu};

    [[nodiscard]] kernel::Event make_reply_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0, 0u);
    }

    [[nodiscard]] kernel::Event make_timeout_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user1, 1u);
    }

    struct FakeReply {
        kernel::TaskId from{};
        kernel::TaskId to{};
        std::uint64_t value{0};
    };

    using Request = kernel::TaskMessageSyscallPumpRequest<Tick>;
    using Completion = kernel::TaskMessageSyscallPumpCompletion<FakeReply>;

    struct FakeStepResult {
        bool progressed{false};
        bool issued{false};
        bool completion_ready{false};
        bool completion_pushed{false};
        bool completion_dropped{false};
        Request issued_request{};
        Completion completion{};
    };

    struct FakePumpState {
        bool bound{true};
        kernel::TaskId default_owner{kDefaultOwner};
        std::uint32_t kick_calls{0};
        std::uint32_t step_calls{0};
        std::uint32_t receive_calls{0};
        std::uint32_t issued_calls{0};
        std::uint64_t next_token{kBaseToken};
        std::uint64_t next_sequence{kBaseSequence};
        kernel::Event last_event{};
    };

    struct FakePump {
        using tick_type = Tick;
        using reply_type = FakeReply;
        using request_type = Request;
        using completion_type = Completion;
        using result_type = FakeStepResult;

        constexpr FakePump() noexcept = default;

        constexpr explicit FakePump(FakePumpState* bound_state) noexcept
            : state(bound_state)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return current_valid_;
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return request_count_;
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return completion_count_;
        }

        void bind_cursors(std::uint64_t next_token,
                          std::uint64_t next_sequence) noexcept
        {
            if (!valid()) {
                return;
            }

            state->next_token = next_token;
            state->next_sequence = next_sequence;
        }

        [[nodiscard]] bool enqueue(request_type request) noexcept
        {
            if (!valid() || request_count_ >= requests_.size()) {
                return false;
            }

            requests_[request_tail_] = request;
            request_tail_ = (request_tail_ + 1u) % requests_.size();
            ++request_count_;
            return true;
        }

        [[nodiscard]] bool enqueue(kernel::TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return enqueue(kernel::make_task_message_syscall_pump_request(
                request, wait_due));
        }

        [[nodiscard]] bool enqueue(kernel::TaskId owner,
                                   kernel::TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return enqueue(kernel::make_task_message_syscall_pump_request(
                owner, request, wait_due));
        }

        [[nodiscard]] bool enqueue(kernel::TaskId owner,
                                   std::uint64_t token,
                                   std::uint64_t request_sequence,
                                   kernel::TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return enqueue(kernel::make_task_message_syscall_pump_request(
                owner, token, request_sequence, request, wait_due));
        }

        [[nodiscard]] bool kick() noexcept
        {
            if (!valid() || busy() || request_count_ == 0u) {
                return false;
            }

            ++state->kick_calls;
            request_type issued{};
            return issue_next(issued);
        }

        [[nodiscard]] result_type step(kernel::Event event) noexcept
        {
            auto result = result_type{};
            if (!valid()) {
                return result;
            }

            ++state->step_calls;
            state->last_event = event;
            if (!busy()) {
                return result;
            }

            result.progressed = true;
            result.completion_ready = true;
            result.completion = make_completion(event);
            result.completion_pushed = push_completion(result.completion);
            result.completion_dropped = !result.completion_pushed;
            current_valid_ = false;

            if (request_count_ > 0u) {
                result.issued = issue_next(result.issued_request);
            }

            return result;
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            if (!valid() || completion_count_ == 0u) {
                return false;
            }

            ++state->receive_calls;
            out = completions_[completion_head_];
            completion_head_ = (completion_head_ + 1u) % completions_.size();
            --completion_count_;
            return true;
        }

    private:
        [[nodiscard]] bool issue_next(request_type& issued_request) noexcept
        {
            if (request_count_ == 0u) {
                return false;
            }

            current_request_ = requests_[request_head_];
            request_head_ = (request_head_ + 1u) % requests_.size();
            --request_count_;

            if (!current_request_.explicit_owner) {
                current_request_.owner = state->default_owner;
            }

            if (!current_request_.explicit_ids) {
                current_request_.token = state->next_token++;
                current_request_.request_sequence = state->next_sequence++;
            }

            current_valid_ = true;
            ++state->issued_calls;
            issued_request = current_request_;
            return true;
        }

        [[nodiscard]] completion_type make_completion(
            kernel::Event event) const noexcept
        {
            const bool timeout = event.id == kernel::EventId::user1 &&
                                 kernel::payload_u64(event) == 1u;
            const auto trap =
                timeout ? timeout_result() : reply_result(current_request_);
            return completion_type{
                .timeout = timeout,
                .owner = current_request_.owner,
                .token = current_request_.token,
                .request_sequence = current_request_.request_sequence,
                .reply = FakeReply{
                    .from = kernel::TaskId{1u},
                    .to = current_request_.owner,
                    .value = trap.value,
                },
                .trap = trap,
            };
        }

        [[nodiscard]] bool push_completion(completion_type completion) noexcept
        {
            if (completion_count_ >= completions_.size()) {
                return false;
            }

            completions_[completion_tail_] = completion;
            completion_tail_ =
                (completion_tail_ + 1u) % completions_.size();
            ++completion_count_;
            return true;
        }

        [[nodiscard]] static constexpr kernel::TrapResult timeout_result()
            noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::none,
                .value = 0,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult reply_result(
            const request_type& request) noexcept
        {
            if (request.request.syscall == kernel::TaskSyscallId::capability_call &&
                request.request.arg1 ==
                    kernel::task_message_session_open_operation) {
                return handled_result(kOpenedHandleBase + request.request.arg0);
            }

            if (request.request.syscall == kernel::TaskSyscallId::capability_call &&
                request.request.arg1 ==
                    kernel::task_message_session_close_operation) {
                return handled_result(request.request.arg2);
            }

            switch (request.request.syscall) {
            case kernel::TaskSyscallId::yield:
                return handled_result(1u);
            case kernel::TaskSyscallId::sleep_until:
                return handled_result(request.request.arg0);
            case kernel::TaskSyscallId::debug_write:
                return handled_result(request.request.arg0);
            case kernel::TaskSyscallId::capability_call:
                return handled_result(request.request.arg1 +
                                      request.request.arg2);
            default:
                return kernel::TrapResult{
                    .disposition = kernel::TrapDisposition::unsupported,
                    .error = kernel::TrapError::unsupported_service,
                    .value = 0,
                };
            }
        }

        [[nodiscard]] static constexpr kernel::TrapResult handled_result(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        std::array<request_type, 8> requests_{};
        std::size_t request_head_{0};
        std::size_t request_tail_{0};
        std::size_t request_count_{0};
        std::array<completion_type, 8> completions_{};
        std::size_t completion_head_{0};
        std::size_t completion_tail_{0};
        std::size_t completion_count_{0};
        request_type current_request_{};
        bool current_valid_{false};
        FakePumpState* state{nullptr};
    };

    using RemoteServices = kernel::TaskMessageRuntimeServiceFacade<FakePump>;
    using TaskRuntime = kernel::TaskMessageRuntimeApi<RemoteServices>;
    using TaskSyscalls = kernel::TaskMessageSyscallApi<TaskRuntime>;
    using TaskSession = kernel::TaskMessageSessionApi<TaskSyscalls>;

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] constexpr kernel::TaskMessageSyscallApiWitness
    make_syscall_reply_witness(const Completion& completion) noexcept
    {
        return kernel::TaskMessageSyscallApiWitness{
            .ready = true,
            .kind = completion.timeout
                        ? kernel::TaskMessageSyscallApiWitnessKind::timeout
                        : kernel::TaskMessageSyscallApiWitnessKind::reply,
            .owner = completion.owner,
            .token = completion.token,
            .request_sequence = completion.request_sequence,
            .completion_ready = true,
            .completion_pushed = true,
            .timeout = completion.timeout,
            .reply_value = completion.trap.value,
            .disposition = completion.trap.disposition,
            .error = completion.trap.error,
        };
    }

    [[nodiscard]] bool probe_default_unbound_session_api() noexcept
    {
        TaskSession session{};
        TaskSession::completion_type completion{};
        const auto stepped = session.step(make_reply_event());

        return !session.valid() && !session.syscalls().valid() &&
               !session.busy() && !session.opened() && !session.faulted() &&
               session.phase() == kernel::TaskMessageSessionPhase::idle &&
               session.pending_requests() == 0u &&
               session.pending_completions() == 0u &&
               session.service_id() == 0u && session.session_handle() == 0u &&
               !session.open(kServiceId, 11u) &&
               !session.request(kRequestOperation, kRequestPayload, 12u) &&
               !session.close(kCloseReason, 13u) &&
               !session.kick() &&
               !session.receive_completion(completion) &&
               !stepped.progressed && !stepped.issued &&
               !stepped.completion_ready && !stepped.completion_pushed &&
               session.reset();
    }

    [[nodiscard]] bool probe_open_request_close_roundtrip() noexcept
    {
        FakePumpState state{};
        auto session = kernel::make_task_message_session_api(
            kernel::make_task_message_syscall_api(
                kernel::make_task_message_runtime_api(
                    kernel::make_task_message_runtime_service_facade(FakePump{
                        &state,
                    }))));
        session.bind_cursors(kBaseToken, kBaseSequence);

        const bool opened_issue = session.open(
            kernel::TaskMessageSessionOpenView{
                .service_id = kServiceId,
                .payload = kOpenPayload,
            },
            11u);
        const bool open_busy_reject =
            !session.request(kRequestOperation, kRequestPayload, 12u);
        const bool open_kick = session.kick();
        const auto open_step = session.step(make_reply_event());
        TaskSession::completion_type open_completion{};
        const bool open_received =
            session.receive_completion(open_completion);
        const auto open_witness =
            kernel::task_message_session_api_witness(
                session,
                open_completion,
                make_syscall_reply_witness(open_completion.raw));
        const bool open_state_ok =
            session.opened() && !session.faulted() &&
            session.phase() == kernel::TaskMessageSessionPhase::open &&
            session.service_id() == kServiceId &&
            session.session_handle() == kOpenedHandle;

        const bool request_issue =
            session.request(kRequestOperation, kRequestPayload, 12u);
        const bool reserved_request_reject =
            !session.request(kernel::task_message_session_close_operation,
                             0u,
                             13u);
        const bool request_kick = session.kick();
        const auto request_step = session.step(make_reply_event());
        TaskSession::completion_type request_completion{};
        const bool request_received =
            session.receive_completion(request_completion);
        const auto request_witness =
            kernel::task_message_session_api_witness(
                session,
                request_completion,
                make_syscall_reply_witness(request_completion.raw));
        const bool request_state_ok =
            session.opened() && !session.faulted() &&
            session.phase() == kernel::TaskMessageSessionPhase::open &&
            session.service_id() == kServiceId &&
            session.session_handle() == kOpenedHandle;

        const bool close_issue = session.close(kCloseReason, 13u);
        const bool close_kick = session.kick();
        const auto close_step = session.step(make_reply_event());
        TaskSession::completion_type close_completion{};
        const bool close_received =
            session.receive_completion(close_completion);
        const auto close_witness =
            kernel::task_message_session_api_witness(
                session,
                close_completion,
                make_syscall_reply_witness(close_completion.raw));
        const auto close_handoff =
            kernel::task_message_session_api_witness_handoff_target(
                close_witness);
        const bool close_state_ok =
            !session.opened() && !session.faulted() &&
            session.phase() == kernel::TaskMessageSessionPhase::idle &&
            session.service_id() == 0u && session.session_handle() == 0u &&
            session.pending_requests() == 0u &&
            session.pending_completions() == 0u && !session.busy();

        const bool open_ok =
            opened_issue && open_busy_reject && open_kick &&
               open_step.progressed && open_step.completion_ready &&
               open_step.completion_pushed && !open_step.issued &&
               open_received &&
               open_step.completion.owner == kDefaultOwner &&
               open_step.completion.token == kBaseToken &&
               open_step.completion.request_sequence == kBaseSequence &&
               open_step.completion.reply.value == kOpenedHandle &&
               open_step.completion.trap.value == kOpenedHandle &&
               open_step.completion.trap.disposition ==
                   kernel::TrapDisposition::handled &&
               kernel::task_message_session_api_witness_ready(open_witness) &&
               open_witness.verdict() == semantic::Verdict::standing &&
               open_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               open_witness.has_lower_provenance &&
               open_witness.action ==
                   kernel::TaskMessageSessionActionKind::open &&
               open_witness.phase_before ==
                   kernel::TaskMessageSessionPhase::opening &&
               open_witness.phase_after ==
                   kernel::TaskMessageSessionPhase::open &&
               open_witness.session_opened &&
               open_witness.opened &&
               !open_witness.faulted &&
               open_witness.service_id == kServiceId &&
               open_witness.session_handle == kOpenedHandle &&
               open_completion.action ==
                   kernel::TaskMessageSessionActionKind::open &&
               open_completion.phase_before ==
                   kernel::TaskMessageSessionPhase::opening &&
               open_completion.phase_after ==
                   kernel::TaskMessageSessionPhase::open &&
               open_completion.session_opened &&
               !open_completion.session_closed &&
               !open_completion.session_faulted &&
               !open_completion.timeout &&
               open_completion.service_id == kServiceId &&
               open_completion.session_handle == kOpenedHandle &&
               open_completion.operation ==
                   kernel::task_message_session_open_operation &&
               open_completion.payload == kOpenPayload &&
               open_completion.reply_value == kOpenedHandle &&
               trap_result_matches(open_completion.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kOpenedHandle) &&
               open_state_ok;
        const bool request_ok =
            request_issue && reserved_request_reject && request_kick &&
               request_step.progressed && request_step.completion_ready &&
               request_step.completion_pushed && !request_step.issued &&
               request_received &&
               request_step.completion.token == kBaseToken + 1u &&
               request_step.completion.request_sequence ==
                   kBaseSequence + 1u &&
               request_step.completion.reply.value ==
                   kRequestOperation + kRequestPayload &&
               request_witness.verdict() == semantic::Verdict::standing &&
               request_witness.action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request_witness.phase_before ==
                   kernel::TaskMessageSessionPhase::requesting &&
               request_witness.phase_after ==
                   kernel::TaskMessageSessionPhase::open &&
               request_witness.service_id == kServiceId &&
               request_witness.session_handle == kOpenedHandle &&
               request_completion.action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request_completion.phase_before ==
                   kernel::TaskMessageSessionPhase::requesting &&
               request_completion.phase_after ==
                   kernel::TaskMessageSessionPhase::open &&
               !request_completion.session_opened &&
               !request_completion.session_closed &&
               !request_completion.session_faulted &&
               !request_completion.timeout &&
               request_completion.service_id == kServiceId &&
               request_completion.session_handle == kOpenedHandle &&
               request_completion.operation == kRequestOperation &&
               request_completion.payload == kRequestPayload &&
               request_completion.reply_value ==
                   kRequestOperation + kRequestPayload &&
               trap_result_matches(request_completion.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kRequestOperation + kRequestPayload) &&
               request_state_ok;
        const bool close_ok =
            close_issue && close_kick && close_step.progressed &&
               close_step.completion_ready && close_step.completion_pushed &&
               !close_step.issued && close_received &&
               close_step.completion.token == kBaseToken + 2u &&
               close_step.completion.request_sequence ==
                   kBaseSequence + 2u &&
               close_step.completion.reply.value == kCloseReason &&
               close_witness.verdict() == semantic::Verdict::standing &&
               close_witness.action ==
                   kernel::TaskMessageSessionActionKind::close &&
               close_witness.phase_before ==
                   kernel::TaskMessageSessionPhase::closing &&
               close_witness.phase_after ==
                   kernel::TaskMessageSessionPhase::idle &&
               close_witness.session_closed &&
               std::string_view{close_handoff.entry_name()} ==
                   "task-message-session-api-witness" &&
               std::string_view{close_handoff.selected_summary_path()} ==
                   "task-message-session-api-witness.summary" &&
               close_completion.action ==
                   kernel::TaskMessageSessionActionKind::close &&
               close_completion.phase_before ==
                   kernel::TaskMessageSessionPhase::closing &&
               close_completion.phase_after ==
                   kernel::TaskMessageSessionPhase::idle &&
               !close_completion.timeout &&
               !close_completion.session_opened &&
               close_completion.session_closed &&
               !close_completion.session_faulted &&
               close_completion.service_id == kServiceId &&
               close_completion.session_handle == kOpenedHandle &&
               close_completion.operation ==
                   kernel::task_message_session_close_operation &&
               close_completion.payload == kCloseReason &&
               close_completion.reply_value == kCloseReason &&
               trap_result_matches(close_completion.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kCloseReason) &&
               close_state_ok &&
               state.kick_calls == 3u && state.step_calls == 3u &&
               state.receive_calls == 3u && state.issued_calls == 3u;
        const bool ok = open_ok && request_ok && close_ok;

        return ok;
    }

    [[nodiscard]] bool probe_close_timeout_reset_and_rebind() noexcept
    {
        FakePumpState first{};
        FakePumpState second{};
        auto session = kernel::make_task_message_session_api(
            kernel::make_task_message_syscall_api(
                kernel::make_task_message_runtime_api(
                    kernel::make_task_message_runtime_service_facade(FakePump{
                        &first,
                    }))));
        session.bind_cursors(kBaseToken, kBaseSequence);

        const bool opened_issue =
            session.open(kernel::TaskMessageSessionOpenView{
                             .service_id = kServiceId,
                             .payload = 1u,
                         },
                         21u);
        const bool open_kick = session.kick();
        const auto open_step = session.step(make_reply_event());
        TaskSession::completion_type open_completion{};
        const bool open_received =
            session.receive_completion(open_completion);

        const bool close_issue = session.close(0x99u, 22u);
        const bool close_kick = session.kick();
        const auto close_step = session.step(make_timeout_event());
        TaskSession::completion_type close_completion{};
        const bool close_received =
            session.receive_completion(close_completion);
        const auto faulted_witness =
            kernel::task_message_session_api_witness(
                session,
                close_completion,
                make_syscall_reply_witness(close_completion.raw));
        const bool faulted_state_ok =
            !session.opened() && session.faulted() &&
            session.phase() == kernel::TaskMessageSessionPhase::faulted &&
            session.service_id() == kServiceId &&
            session.session_handle() == kOpenedHandle;
        const bool blocked_while_faulted =
            !session.open(kServiceId, 23u) &&
            !session.request(kRequestOperation, kRequestPayload, 24u) &&
            !session.close(kCloseReason, 25u);
        const bool reset_ok = session.reset();
        const bool reset_state_ok =
            session.phase() == kernel::TaskMessageSessionPhase::idle &&
            session.service_id() == 0u && session.session_handle() == 0u;

        session.bind_syscalls(kernel::make_task_message_syscall_api(
            kernel::make_task_message_runtime_api(
                kernel::make_task_message_runtime_service_facade(FakePump{}))));
        const bool rebound_unbound = !session.valid() && !session.open(kServiceId, 26u);

        session.bind_syscalls(kernel::make_task_message_syscall_api(
            kernel::make_task_message_runtime_api(
                kernel::make_task_message_runtime_service_facade(FakePump{
                    &second,
                }))));
        session.bind_cursors(0xE1u, 0x81u);
        const bool rebound_open = session.open(kServiceId + 1u, 27u);
        const bool rebound_kick = session.kick();
        const auto rebound_step = session.step(make_reply_event());
        TaskSession::completion_type rebound_completion{};
        const bool rebound_received =
            session.receive_completion(rebound_completion);
        const bool rebound_state_ok =
            session.opened() && !session.faulted() &&
            session.service_id() == kServiceId + 1u &&
            session.session_handle() ==
                kOpenedHandleBase + kServiceId + 1u;

        const bool raw_enqueue =
            session.syscalls().runtime().services().enqueue(
            kTimeoutOwner,
            kTimeoutToken,
            kTimeoutSequence,
            kernel::TaskSyscallRequest{
                .syscall = kInvalidSyscall,
                .arg0 = 0xAAu,
            },
            28u);
        const bool raw_kick = session.kick();
        const auto raw_step = session.step(make_reply_event());
        kernel::TaskMessageSessionApi<TaskSyscalls>::syscall_completion_type
            raw_completion{};
        const bool raw_received =
            session.syscalls().receive_completion(raw_completion);

        const bool faulted_ok =
            opened_issue && open_kick && open_step.progressed &&
               open_received && open_completion.session_opened &&
               close_issue && close_kick && close_step.progressed &&
               close_step.completion_ready && close_step.completion_pushed &&
               close_received &&
               faulted_witness.verdict() == semantic::Verdict::standing &&
               faulted_witness.action ==
                   kernel::TaskMessageSessionActionKind::close &&
               faulted_witness.phase_after ==
                   kernel::TaskMessageSessionPhase::faulted &&
               faulted_witness.timeout &&
               faulted_witness.session_faulted &&
               close_completion.action ==
                   kernel::TaskMessageSessionActionKind::close &&
               close_completion.phase_before ==
                   kernel::TaskMessageSessionPhase::closing &&
               close_completion.phase_after ==
                   kernel::TaskMessageSessionPhase::faulted &&
               close_completion.timeout &&
               !close_completion.session_closed &&
               close_completion.session_faulted &&
               close_completion.session_handle == kOpenedHandle &&
               trap_result_matches(close_completion.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::none) &&
               faulted_state_ok &&
               blocked_while_faulted && reset_ok && reset_state_ok;
        const bool rebound_ok =
            rebound_unbound && rebound_open && rebound_kick &&
            rebound_step.progressed && rebound_received &&
            rebound_completion.session_opened &&
            rebound_completion.session_handle ==
                kOpenedHandleBase + kServiceId + 1u &&
            rebound_state_ok;
        const bool raw_ok =
            raw_enqueue && raw_kick && raw_step.progressed && raw_received &&
            raw_completion.owner == kTimeoutOwner &&
            raw_completion.token == kTimeoutToken &&
            raw_completion.request_sequence == kTimeoutSequence &&
            trap_result_matches(raw_completion.trap,
                                kernel::TrapDisposition::unsupported,
                                kernel::TrapError::unsupported_service) &&
            second.kick_calls == 2u && second.step_calls == 2u &&
            second.receive_calls == 2u && second.issued_calls == 2u &&
            second.next_token == 0xE2u && second.next_sequence == 0x82u;
        const bool ok = faulted_ok && rebound_ok && raw_ok;

        return ok;
    }
}

int main()
{
    const bool default_unbound_ok =
        demo::probe_default_unbound_session_api();
    const bool roundtrip_ok =
        demo::probe_open_request_close_roundtrip();
    const bool timeout_rebind_ok =
        demo::probe_close_timeout_reset_and_rebind();
    const bool ok =
        default_unbound_ok && roundtrip_ok && timeout_rebind_ok;

    std::printf(
        "[runtime-task-message-session-api-demo] ok=%d default_unbound=%d roundtrip=%d timeout_rebind=%d\n",
        ok ? 1 : 0,
        default_unbound_ok ? 1 : 0,
        roundtrip_ok ? 1 : 0,
        timeout_rebind_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-api-witness] ok=%d collapsed=%s summary=%s\n",
        ok ? 1 : 0,
        semantic::verdict_name(
            kernel::TaskMessageSessionApiWitness{}.verdict()),
        kernel::TaskMessageSessionApiWitness{}.summary_path().data());
    return ok ? 0 : 1;
}
