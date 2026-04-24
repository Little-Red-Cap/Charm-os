#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

import kernel.evt;
import kernel.task_message_syscall_api;

namespace demo {
    using Tick = std::uint64_t;
    inline constexpr auto kInvalidSyscall =
        static_cast<kernel::TaskSyscallId>(99u);
    inline constexpr kernel::TaskId kDefaultOwner{7u};
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

    using RemoteServices = kernel::TaskMessageRuntimeServiceFacade<FakePump>;
    using TaskRuntime = kernel::TaskMessageRuntimeApi<RemoteServices>;
    using TaskSyscalls = kernel::TaskMessageSyscallApi<TaskRuntime>;

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_default_unbound_async_syscalls() noexcept
    {
        TaskSyscalls syscalls{};
        Completion completion{};
        const auto stepped = syscalls.step(make_reply_event());
        return !syscalls.valid() && !syscalls.runtime().valid() &&
               !syscalls.busy() && syscalls.pending_requests() == 0u &&
               syscalls.pending_completions() == 0u &&
               !syscalls.sys_yield(11u) &&
               !syscalls.sys_yield(kernel::TrapYieldCurrentView{}, 12u) &&
               !syscalls.sys_sleep_until(42u, 13u) &&
               !syscalls.sys_debug_write(0x33u, 14u) &&
               !syscalls.sys_capability_call(1u, 2u, 3u, 15u) &&
               !syscalls.sys_capability_call(
                   kernel::TrapCapabilityCallView{
                       .capability_id = 9u,
                       .operation = 4u,
                       .payload = 5u,
                   },
                   16u) &&
               !syscalls.kick() &&
               !syscalls.receive_completion(completion) &&
               !stepped.progressed && !stepped.issued &&
               !stepped.completion_ready && !stepped.completion_pushed;
    }

    [[nodiscard]] bool probe_async_syscall_named_entry_points() noexcept
    {
        FakePumpState state{};
        auto syscalls = kernel::make_task_message_syscall_api(
            kernel::make_task_message_runtime_api(
                kernel::make_task_message_runtime_service_facade(FakePump{
                    &state,
                })));
        syscalls.bind_cursors(kBaseToken, kBaseSequence);

        const bool enqueued =
            syscalls.sys_yield(11u) &&
            syscalls.sys_yield(kernel::TrapYieldCurrentView{}, 12u) &&
            syscalls.sys_sleep_until(42u, 13u) &&
            syscalls.sys_sleep_until(kernel::TrapSleepUntilView<Tick>{
                                         .due = 84u,
                                     },
                                     14u) &&
            syscalls.sys_debug_write(0xC0DEu, 15u) &&
            syscalls.sys_debug_write(kernel::TrapDebugWriteView{
                                         .value = 0x55u,
                                     },
                                     16u) &&
            syscalls.sys_capability_call(7u, 2u, 33u, 17u) &&
            syscalls.sys_capability_call(kernel::TrapCapabilityCallView{
                                             .capability_id = 9u,
                                             .operation = 4u,
                                             .payload = 5u,
                                         },
                                         18u);
        if (!enqueued || syscalls.pending_requests() != 8u ||
            !syscalls.kick() || !syscalls.busy() ||
            syscalls.pending_requests() != 7u) {
            return false;
        }

        std::array<Completion, 8> completions{};
        const auto step1 = syscalls.step(make_reply_event());
        const auto step2 = syscalls.step(make_reply_event());
        const auto step3 = syscalls.step(make_reply_event());
        const auto step4 = syscalls.step(make_reply_event());
        const auto step5 = syscalls.step(make_reply_event());
        const auto step6 = syscalls.step(make_reply_event());
        const auto step7 = syscalls.step(make_reply_event());
        const auto step8 = syscalls.step(make_reply_event());
        if (!syscalls.receive_completion(completions[0]) ||
            !syscalls.receive_completion(completions[1]) ||
            !syscalls.receive_completion(completions[2]) ||
            !syscalls.receive_completion(completions[3]) ||
            !syscalls.receive_completion(completions[4]) ||
            !syscalls.receive_completion(completions[5]) ||
            !syscalls.receive_completion(completions[6]) ||
            !syscalls.receive_completion(completions[7])) {
            return false;
        }

        return syscalls.valid() && syscalls.runtime().valid() &&
               syscalls.runtime().services().valid() &&
               state.kick_calls == 1u && state.step_calls == 8u &&
               state.receive_calls == 8u && state.issued_calls == 8u &&
               state.last_event.id == kernel::EventId::user0 &&
               kernel::payload_u64(state.last_event) == 0u &&
               step1.progressed && step1.completion_ready &&
               step1.completion_pushed && step1.issued &&
               step1.completion.owner == kDefaultOwner &&
               step1.completion.token == kBaseToken &&
               step1.completion.request_sequence == kBaseSequence &&
               trap_result_matches(step1.completion.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               step1.issued_request.request.syscall ==
                   kernel::TaskSyscallId::yield &&
               step2.issued_request.request.syscall ==
                   kernel::TaskSyscallId::sleep_until &&
               step2.issued_request.request.arg0 == 42u &&
               step3.issued_request.request.syscall ==
                   kernel::TaskSyscallId::sleep_until &&
               step3.issued_request.request.arg0 == 84u &&
               step4.issued_request.request.syscall ==
                   kernel::TaskSyscallId::debug_write &&
               step4.issued_request.request.arg0 == 0xC0DEu &&
               step5.issued_request.request.syscall ==
                   kernel::TaskSyscallId::debug_write &&
               step5.issued_request.request.arg0 == 0x55u &&
               step6.issued_request.request.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               step6.issued_request.request.arg0 == 7u &&
               step6.issued_request.request.arg1 == 2u &&
               step6.issued_request.request.arg2 == 33u &&
               step7.issued_request.request.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               step7.issued_request.request.arg0 == 9u &&
               step7.issued_request.request.arg1 == 4u &&
               step7.issued_request.request.arg2 == 5u &&
               step8.progressed && step8.completion_ready &&
               step8.completion_pushed && !step8.issued &&
               completions[0].token == kBaseToken &&
               completions[0].request_sequence == kBaseSequence &&
               completions[0].reply.value == 1u &&
               trap_result_matches(completions[0].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               completions[1].token == kBaseToken + 1u &&
               completions[1].request_sequence == kBaseSequence + 1u &&
               completions[1].reply.value == 1u &&
               trap_result_matches(completions[1].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               completions[2].token == kBaseToken + 2u &&
               completions[2].request_sequence == kBaseSequence + 2u &&
               completions[2].reply.value == 42u &&
               trap_result_matches(completions[2].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               completions[3].token == kBaseToken + 3u &&
               completions[3].request_sequence == kBaseSequence + 3u &&
               completions[3].reply.value == 84u &&
               trap_result_matches(completions[3].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   84u) &&
               completions[4].token == kBaseToken + 4u &&
               completions[4].request_sequence == kBaseSequence + 4u &&
               completions[4].reply.value == 0xC0DEu &&
               trap_result_matches(completions[4].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xC0DEu) &&
               completions[5].token == kBaseToken + 5u &&
               completions[5].request_sequence == kBaseSequence + 5u &&
               completions[5].reply.value == 0x55u &&
               trap_result_matches(completions[5].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x55u) &&
               completions[6].token == kBaseToken + 6u &&
               completions[6].request_sequence == kBaseSequence + 6u &&
               completions[6].reply.value == 42u &&
               trap_result_matches(completions[6].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               completions[7].token == kBaseToken + 7u &&
               completions[7].request_sequence == kBaseSequence + 7u &&
               completions[7].reply.value == 18u &&
               trap_result_matches(completions[7].trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   18u) &&
               syscalls.pending_requests() == 0u &&
               syscalls.pending_completions() == 0u && !syscalls.busy();
    }

    [[nodiscard]] bool probe_bind_runtime_and_escape_hatch() noexcept
    {
        FakePumpState first{};
        FakePumpState second{};
        auto syscalls = kernel::make_task_message_syscall_api(
            kernel::make_task_message_runtime_api(
                kernel::make_task_message_runtime_service_facade(FakePump{
                    &first,
                })));

        const bool queued_timeout =
            syscalls.runtime().services().enqueue(
                kTimeoutOwner,
                kTimeoutToken,
                kTimeoutSequence,
                kernel::make_task_syscall_yield_request(),
                30u);
        const bool timeout_kick = syscalls.kick();
        const auto timed = syscalls.step(make_timeout_event());
        Completion timeout_completion{};
        const bool timeout_received =
            syscalls.receive_completion(timeout_completion);

        syscalls.bind_runtime(kernel::make_task_message_runtime_api(
            kernel::make_task_message_runtime_service_facade(FakePump{})));
        const bool unbound_valid = syscalls.valid();
        const bool unbound_issue = syscalls.sys_yield(17u);

        syscalls.bind_runtime(kernel::make_task_message_runtime_api(
            kernel::make_task_message_runtime_service_facade(FakePump{
                &second,
            })));
        syscalls.bind_cursors(0xE1u, 0x81u);
        const bool rebound_queue =
            syscalls.runtime().services().enqueue(
                kernel::TaskSyscallRequest{
                    .syscall = kInvalidSyscall,
                    .arg0 = 0xAAu,
                    .arg1 = 0xBBu,
                },
                18u);
        const bool rebound_debug = syscalls.sys_debug_write(0x55u, 19u);
        const bool rebound_kick = syscalls.kick();
        const auto rebound = syscalls.step(make_reply_event());
        Completion rebound_completion{};
        const bool rebound_received =
            syscalls.receive_completion(rebound_completion);
        const auto rebound2 = syscalls.step(make_reply_event());
        Completion rebound_completion2{};
        const bool rebound_received2 =
            syscalls.receive_completion(rebound_completion2);

        return queued_timeout && timeout_kick && timed.progressed &&
               timed.completion_ready && timed.completion_pushed &&
               !timed.issued && timeout_received &&
               timeout_completion.timeout &&
               timeout_completion.owner == kTimeoutOwner &&
               timeout_completion.token == kTimeoutToken &&
               timeout_completion.request_sequence == kTimeoutSequence &&
               trap_result_matches(timeout_completion.trap,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::none) &&
               !unbound_valid && !unbound_issue &&
               rebound_queue && rebound_debug && rebound_kick &&
               rebound.progressed && rebound.completion_ready &&
               rebound.completion_pushed && rebound.issued &&
               rebound_received &&
               rebound_completion.owner == kDefaultOwner &&
               rebound_completion.token == 0xE1u &&
               rebound_completion.request_sequence == 0x81u &&
               trap_result_matches(rebound_completion.trap,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               rebound.issued_request.request.syscall ==
                   kernel::TaskSyscallId::debug_write &&
               rebound.issued_request.token == 0xE2u &&
               rebound.issued_request.request_sequence == 0x82u &&
               rebound2.progressed && rebound2.completion_ready &&
               rebound2.completion_pushed && !rebound2.issued &&
               rebound_received2 && !rebound_completion2.timeout &&
               rebound_completion2.owner == kDefaultOwner &&
               rebound_completion2.token == 0xE2u &&
               rebound_completion2.request_sequence == 0x82u &&
               rebound_completion2.reply.value == 0x55u &&
               trap_result_matches(rebound_completion2.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x55u) &&
               second.kick_calls == 1u && second.step_calls == 2u &&
               second.receive_calls == 2u && second.issued_calls == 2u &&
               second.next_token == 0xE3u && second.next_sequence == 0x83u &&
               syscalls.pending_requests() == 0u &&
               syscalls.pending_completions() == 0u && !syscalls.busy();
    }
}

int main()
{
    const bool default_unbound_ok =
        demo::probe_default_unbound_async_syscalls();
    const bool syscall_entry_points_ok =
        demo::probe_async_syscall_named_entry_points();
    const bool bind_timeout_ok =
        demo::probe_bind_runtime_and_escape_hatch();
    const bool ok =
        default_unbound_ok && syscall_entry_points_ok && bind_timeout_ok;

    std::printf(
        "[runtime-task-message-syscall-api-demo] ok=%d default_unbound=%d syscall_entry_points=%d bind_timeout=%d\n",
        ok ? 1 : 0,
        default_unbound_ok ? 1 : 0,
        syscall_entry_points_ok ? 1 : 0,
        bind_timeout_ok ? 1 : 0);
    return ok ? 0 : 1;
}
