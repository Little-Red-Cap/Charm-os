#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.evt;
import kernel.task_message_runtime_service;
import semantic.core;

namespace demo {
    using Tick = std::uint64_t;
    inline constexpr auto kInvalidSyscall =
        static_cast<kernel::TaskSyscallId>(99u);
    inline constexpr kernel::TaskId kDefaultOwner{7u};
    inline constexpr kernel::TaskId kExplicitOwner{11u};
    inline constexpr kernel::TaskId kTimeoutOwner{21u};
    inline constexpr std::uint64_t kBaseToken{0xC1u};
    inline constexpr std::uint64_t kBaseSequence{0x61u};
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
            switch (request.request.syscall) {
            case kernel::TaskSyscallId::yield:
                return handled_result(1u);
            case kernel::TaskSyscallId::sleep_until:
                return handled_result(request.request.arg0);
            case kernel::TaskSyscallId::debug_write:
                return handled_result(request.request.arg0);
            case kernel::TaskSyscallId::capability_call:
                return handled_result(request.request.arg0 +
                                      request.request.arg1 +
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

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] constexpr kernel::TaskMessageSyscallPumpWitness
    make_pump_witness_from_step(const FakeStepResult& result) noexcept
    {
        auto witness = kernel::TaskMessageSyscallPumpWitness{};
        if (result.completion_dropped) {
            witness.ready = true;
            witness.kind = kernel::TaskMessageSyscallPumpTraceKind::completion_drop;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
            return witness;
        }

        if (result.completion_ready) {
            witness.ready = true;
            witness.has_trace = true;
            witness.terminal_ok = result.completion_pushed;
            witness.kind = result.completion.timeout
                               ? kernel::TaskMessageSyscallPumpTraceKind::timeout
                               : kernel::TaskMessageSyscallPumpTraceKind::reply;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
            return witness;
        }

        if (result.issued) {
            witness.ready = true;
            witness.has_trace = true;
            witness.terminal_ok = true;
            witness.kind = kernel::TaskMessageSyscallPumpTraceKind::issue;
            witness.owner = result.issued_request.owner;
            witness.token = result.issued_request.token;
            witness.request_sequence = result.issued_request.request_sequence;
            witness.wait_due = result.issued_request.wait_due;
        }

        return witness;
    }

    [[nodiscard]] constexpr FakeStepResult
    make_issue_only_step(const Request& request) noexcept
    {
        return FakeStepResult{
            .issued = true,
            .issued_request = request,
        };
    }

    [[nodiscard]] bool probe_default_unbound_remote_service() noexcept
    {
        kernel::TaskMessageRuntimeServiceFacade<FakePump> facade{};
        Completion completion{};
        return !facade.valid() && !facade.busy() &&
               facade.pending_requests() == 0u &&
               facade.pending_completions() == 0u &&
               !facade.yield_current(11u) && !facade.debug_write(0x44u, 12u) &&
               !facade.capability_call(1u, 2u, 3u, 13u) && !facade.kick() &&
               !facade.receive_completion(completion);
    }

    [[nodiscard]] bool probe_remote_service_queue_and_reply() noexcept
    {
        FakePumpState state{};
        auto facade = kernel::make_task_message_runtime_service_facade(
            FakePump{&state});
        facade.bind_cursors(kBaseToken, kBaseSequence);

        const bool enqueued =
            facade.yield_current(11u) &&
            facade.sleep_current_until(kExplicitOwner,
                                       kernel::TrapSleepUntilView<Tick>{
                                           .due = 42u,
                                       },
                                       12u) &&
            facade.debug_write(0xC0DEu, 13u) &&
            facade.capability_call(7u, 2u, 33u, 14u) &&
            facade.enqueue(
                kernel::TaskSyscallRequest{
                    .syscall = kInvalidSyscall,
                    .arg0 = 0xAAu,
                    .arg1 = 0xBBu,
                },
                15u);
        if (!enqueued || facade.pending_requests() != 5u || !facade.kick() ||
            !facade.busy() || facade.pending_requests() != 4u) {
            return false;
        }

        std::array<Completion, 5> completions{};
        auto reply_result = facade.step(make_reply_event());
        const auto reply_lower = make_pump_witness_from_step(reply_result);
        const auto reply_witness =
            kernel::task_message_runtime_service_witness(
                facade, reply_result, reply_lower);
        if (!reply_result.progressed || !reply_result.completion_ready ||
            !reply_result.completion_pushed || !reply_result.issued ||
            !facade.receive_completion(completions[0])) {
            return false;
        }

        auto step2 = facade.step(make_reply_event());
        auto step3 = facade.step(make_reply_event());
        auto step4 = facade.step(make_reply_event());
        auto step5 = facade.step(make_reply_event());
        const auto issued_only = make_issue_only_step(step4.issued_request);
        const auto issued_witness =
            kernel::task_message_runtime_service_witness(facade, issued_only);
        const auto issued_handoff =
            kernel::task_message_runtime_service_witness_handoff_target(
                issued_witness);
        if (!facade.receive_completion(completions[1]) ||
            !facade.receive_completion(completions[2]) ||
            !facade.receive_completion(completions[3]) ||
            !facade.receive_completion(completions[4])) {
            return false;
        }

        return state.kick_calls == 1u && state.step_calls == 5u &&
               state.receive_calls == 5u && state.issued_calls == 5u &&
               state.next_token == kBaseToken + 5u &&
               state.next_sequence == kBaseSequence + 5u &&
               state.last_event.id == kernel::EventId::user0 &&
               kernel::payload_u64(state.last_event) == 0u &&
               reply_result.completion.owner == kDefaultOwner &&
               reply_result.completion.token == kBaseToken &&
               reply_result.completion.request_sequence == kBaseSequence &&
               trap_result_matches(reply_result.completion.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               reply_result.issued_request.owner == kExplicitOwner &&
               reply_result.issued_request.token == kBaseToken + 1u &&
               reply_result.issued_request.request_sequence ==
                   kBaseSequence + 1u &&
               kernel::task_message_runtime_service_witness_ready(
                   reply_witness) &&
               reply_witness.verdict() == semantic::Verdict::standing &&
               reply_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               reply_witness.has_lower_provenance &&
               reply_witness.kind ==
                   kernel::TaskMessageRuntimeServiceWitnessKind::reply &&
               reply_witness.owner == kDefaultOwner &&
               reply_witness.token == kBaseToken &&
               reply_witness.request_sequence == kBaseSequence &&
               reply_witness.reply_value == 1u &&
               issued_witness.verdict() == semantic::Verdict::standing &&
               issued_witness.kind ==
                   kernel::TaskMessageRuntimeServiceWitnessKind::issue &&
               std::string_view{issued_handoff.entry_name()} ==
                   "task-message-runtime-service-witness" &&
               std::string_view{issued_handoff.selected_summary_path()} ==
                   "task-message-runtime-service-witness.summary" &&
               reply_result.issued_request.request.syscall ==
                   kernel::TaskSyscallId::sleep_until &&
               reply_result.issued_request.request.arg0 == 42u &&
               step2.issued_request.request.syscall ==
                   kernel::TaskSyscallId::debug_write &&
               step2.issued_request.request.arg0 == 0xC0DEu &&
               step3.issued_request.request.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               step3.issued_request.request.arg0 == 7u &&
               step3.issued_request.request.arg1 == 2u &&
               step3.issued_request.request.arg2 == 33u &&
               step4.issued_request.request.syscall == kInvalidSyscall &&
               step4.issued_request.request.arg0 == 0xAAu &&
               step4.issued_request.request.arg1 == 0xBBu &&
               !step5.issued &&
               completions[0].owner == kDefaultOwner &&
               completions[0].token == kBaseToken &&
               completions[0].request_sequence == kBaseSequence &&
               completions[0].reply.value == 1u &&
               trap_result_matches(completions[0].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               completions[1].owner == kExplicitOwner &&
               completions[1].token == kBaseToken + 1u &&
               completions[1].request_sequence == kBaseSequence + 1u &&
               completions[1].reply.value == 42u &&
               trap_result_matches(completions[1].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               completions[2].owner == kDefaultOwner &&
               completions[2].token == kBaseToken + 2u &&
               completions[2].request_sequence == kBaseSequence + 2u &&
               completions[2].reply.value == 0xC0DEu &&
               trap_result_matches(completions[2].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xC0DEu) &&
               completions[3].owner == kDefaultOwner &&
               completions[3].token == kBaseToken + 3u &&
               completions[3].request_sequence == kBaseSequence + 3u &&
               completions[3].reply.value == 42u &&
               trap_result_matches(completions[3].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               completions[4].owner == kDefaultOwner &&
               completions[4].token == kBaseToken + 4u &&
               completions[4].request_sequence == kBaseSequence + 4u &&
               completions[4].reply.value == 0u &&
               trap_result_matches(completions[4].trap,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               facade.pending_requests() == 0u &&
               facade.pending_completions() == 0u && !facade.busy();
    }

    [[nodiscard]] bool probe_explicit_timeout_and_rebind() noexcept
    {
        FakePumpState first{};
        FakePumpState second{};
        auto facade = kernel::make_task_message_runtime_service_facade(
            FakePump{&first});

        const bool first_queue = facade.enqueue(
            kTimeoutOwner,
            kTimeoutToken,
            kTimeoutSequence,
            kernel::TaskSyscallRequest{
                .syscall = kernel::TaskSyscallId::yield,
            },
            30u);
        const bool kicked = facade.kick();
        const auto timed = facade.step(make_timeout_event());
        const auto timed_lower = make_pump_witness_from_step(timed);
        const auto timed_witness =
            kernel::task_message_runtime_service_witness(
                facade, timed, timed_lower);
        Completion completion{};
        const bool received = facade.receive_completion(completion);

        facade.bind_pump(FakePump{});
        const bool unbound_ok =
            !facade.valid() && !facade.yield_current(31u) && !facade.kick();

        facade.bind_pump(FakePump{&second});
        facade.bind_cursors(0xE1u, 0x81u);
        const bool rebound_enqueue = facade.debug_write(kExplicitOwner, 0x55u, 22u);
        const bool rebound_kick = facade.kick();
        const auto rebound = facade.step(make_reply_event());
        const auto rebound_lower = make_pump_witness_from_step(rebound);
        const auto rebound_witness =
            kernel::task_message_runtime_service_witness(
                facade, rebound, rebound_lower);
        Completion rebound_completion{};
        const bool rebound_receive =
            facade.receive_completion(rebound_completion);

        return first_queue && kicked && timed.progressed &&
               timed.completion_ready && timed.completion_pushed &&
               !timed.issued && received && completion.timeout &&
               completion.owner == kTimeoutOwner &&
               completion.token == kTimeoutToken &&
               completion.request_sequence == kTimeoutSequence &&
               timed_witness.verdict() == semantic::Verdict::standing &&
               timed_witness.kind ==
                   kernel::TaskMessageRuntimeServiceWitnessKind::timeout &&
               timed_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               trap_result_matches(completion.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::none) &&
               unbound_ok && rebound_enqueue && rebound_kick &&
               rebound.progressed && rebound.completion_ready &&
               rebound.completion_pushed && rebound_receive &&
               rebound_witness.verdict() == semantic::Verdict::standing &&
               rebound_witness.kind ==
                   kernel::TaskMessageRuntimeServiceWitnessKind::reply &&
               rebound_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               !rebound_completion.timeout &&
               rebound_completion.owner == kExplicitOwner &&
               rebound_completion.token == 0xE1u &&
               rebound_completion.request_sequence == 0x81u &&
               rebound_completion.reply.value == 0x55u &&
               trap_result_matches(rebound_completion.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x55u) &&
               second.next_token == 0xE2u && second.next_sequence == 0x82u &&
               second.kick_calls == 1u && second.step_calls == 1u &&
               second.receive_calls == 1u;
    }
}

int main()
{
    const bool default_unbound_ok =
        demo::probe_default_unbound_remote_service();
    const bool queue_reply_ok =
        demo::probe_remote_service_queue_and_reply();
    const bool timeout_rebind_ok =
        demo::probe_explicit_timeout_and_rebind();
    const bool ok =
        default_unbound_ok && queue_reply_ok && timeout_rebind_ok;

    std::printf(
        "[runtime-task-message-runtime-service-demo] ok=%d default_unbound=%d queue_reply=%d timeout_rebind=%d\n",
        ok ? 1 : 0,
        default_unbound_ok ? 1 : 0,
        queue_reply_ok ? 1 : 0,
        timeout_rebind_ok ? 1 : 0);
    return ok ? 0 : 1;
}
