#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_dispatch;

namespace demo {
    using namespace std::literals;

    inline constexpr std::uint64_t kEchoLabel{0xCA11u};
    inline constexpr std::uint64_t kSequenceLabel{0xD00Du};

    struct FakeMessagesState {
        bool bound{true};
        bool request_ready{false};
        std::uint32_t receive_calls{0};
        std::uint32_t reply_calls{0};
        kernel::RuntimeMailboxRequest queued_request{};
        std::uint64_t last_reply_to{0};
        std::uint64_t last_reply_sequence{0};
        std::uint64_t last_reply_value{0};
    };

    struct FakeMessages {
        using request_type = kernel::RuntimeMailboxRequest;

        FakeMessagesState* state{nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] bool receive(request_type& out) const noexcept
        {
            if (!valid() || !state->request_ready) {
                return false;
            }

            ++state->receive_calls;
            out = state->queued_request;
            state->request_ready = false;
            return true;
        }

        [[nodiscard]] bool reply(const request_type& request,
                                 std::uint64_t value) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->reply_calls;
            state->last_reply_to = request.from.value;
            state->last_reply_sequence = request.sequence;
            state->last_reply_value = value;
            return true;
        }
    };

    struct EchoHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_from{0};
        std::uint64_t last_value{0};
        std::uint64_t last_sequence{0};
    };

    struct EchoHandler {
        EchoHandlerState* state{nullptr};

        [[nodiscard]] kernel::TaskMessageHandleResult dispatch(
            const kernel::RuntimeMailboxRequest& request) const noexcept
        {
            if (state == nullptr) {
                return kernel::unhandled_task_message();
            }

            ++state->calls;
            state->last_from = request.from.value;
            state->last_value = request.value;
            state->last_sequence = request.sequence;
            return kernel::handled_task_message(request.value + 1000u);
        }
    };

    struct SequenceHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_label{0};
        std::uint64_t last_sequence{0};
    };

    struct SequenceHandler {
        SequenceHandlerState* state{nullptr};

        [[nodiscard]] kernel::TaskMessageHandleResult dispatch(
            const kernel::RuntimeMailboxRequest& request) const noexcept
        {
            if (state == nullptr) {
                return kernel::unhandled_task_message();
            }

            ++state->calls;
            state->last_label = request.label;
            state->last_sequence = request.sequence;
            return kernel::handled_task_message(request.sequence);
        }
    };

    using DispatchTrace = kernel::TaskMessageDispatchTraceBuffer<8>;

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] bool probe_direct_dispatch_and_serve_once() noexcept
    {
        FakeMessagesState state{};
        EchoHandlerState echo_state{};
        SequenceHandlerState sequence_state{};
        DispatchTrace trace{};
        EchoHandler echo_handler{
            .state = &echo_state,
        };
        SequenceHandler sequence_handler{
            .state = &sequence_state,
        };
        auto table = kernel::make_task_message_table(
            std::array<kernel::TaskMessageHandlerEntry, 2>{
                kernel::task_message_handler_entry(
                    kEchoLabel,
                    "echo",
                    kernel::make_task_message_handler(echo_handler)),
                kernel::task_message_handler_entry(
                    kSequenceLabel,
                    "sequence",
                    kernel::make_task_message_handler(sequence_handler)),
            });
        auto dispatcher = kernel::make_task_message_dispatcher(
            FakeMessages{
                .state = &state,
            },
            table,
            &trace);

        const auto direct = dispatcher.dispatch_request(
            kernel::RuntimeMailboxRequest{
                .from = kernel::TaskId{2u},
                .label = kSequenceLabel,
                .value = 7u,
                .sequence = 0x55u,
            });

        state.request_ready = true;
        state.queued_request = kernel::RuntimeMailboxRequest{
            .from = kernel::TaskId{3u},
            .label = kEchoLabel,
            .value = 42u,
            .sequence = 0x77u,
        };
        const auto served = dispatcher.serve_once();

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        if (first == nullptr || second == nullptr) {
            return false;
        }

        const auto first_request =
            kernel::task_message_request_from_trace_event(*first);

        return dispatcher.valid() && direct.accepted && direct.matched &&
               direct.handler_valid && direct.handled && direct.replied &&
               direct.reply_value == 0x55u && served.accepted &&
               served.matched && served.handler_valid && served.handled &&
               served.replied && served.reply_value == 1042u &&
               state.receive_calls == 1u && state.reply_calls == 2u &&
               state.last_reply_to == 3u &&
               state.last_reply_sequence == 0x77u &&
               state.last_reply_value == 1042u && echo_state.calls == 1u &&
               echo_state.last_from == 3u && echo_state.last_value == 42u &&
               echo_state.last_sequence == 0x77u &&
               sequence_state.calls == 1u &&
               sequence_state.last_label == kSequenceLabel &&
               sequence_state.last_sequence == 0x55u && trace.size() == 2u &&
               first->sequence == 1u &&
               first->from == kernel::TaskId{2u} &&
               first->label == kSequenceLabel &&
               same_text(first->label_name, "sequence"sv) &&
               first->accepted && first->matched && first->handler_valid &&
               first->handled && first->replied &&
               first->reply_value == 0x55u &&
               first_request.from == kernel::TaskId{2u} &&
               first_request.label == kSequenceLabel &&
               first_request.value == 7u &&
               first_request.sequence == 0x55u &&
               second->sequence == 2u &&
               second->from == kernel::TaskId{3u} &&
               second->label == kEchoLabel &&
               same_text(second->label_name, "echo"sv) &&
               second->accepted && second->matched &&
               second->handler_valid && second->handled &&
               second->replied && second->reply_value == 1042u;
    }

    [[nodiscard]] bool probe_unbound_and_missing() noexcept
    {
        FakeMessagesState unbound_state{
            .bound = false,
        };
        FakeMessagesState missing_state{
            .request_ready = true,
            .queued_request =
                kernel::RuntimeMailboxRequest{
                    .from = kernel::TaskId{5u},
                    .label = 0xBEEFu,
                    .value = 12u,
                    .sequence = 2u,
                },
        };
        EchoHandlerState echo_state{};
        DispatchTrace trace{};
        EchoHandler echo_handler{
            .state = &echo_state,
        };
        auto table = kernel::make_task_message_table(
            std::array<kernel::TaskMessageHandlerEntry, 2>{
                kernel::task_message_handler_entry(
                    kEchoLabel,
                    "echo",
                    kernel::make_task_message_handler(echo_handler)),
                kernel::task_message_handler_entry(kSequenceLabel, "sequence"),
            });
        auto unbound_dispatcher = kernel::make_task_message_dispatcher(
            FakeMessages{
                .state = &unbound_state,
            },
            table,
            &trace);

        const auto unbound = unbound_dispatcher.dispatch_request(
            kernel::RuntimeMailboxRequest{
                .from = kernel::TaskId{4u},
                .label = kEchoLabel,
                .value = 99u,
                .sequence = 1u,
            });

        unbound_dispatcher.bind_messages(
            FakeMessages{
                .state = &missing_state,
            });
        const auto missing = unbound_dispatcher.serve_once();
        const auto idle = decltype(unbound_dispatcher){}.serve_once();

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        if (first == nullptr || second == nullptr) {
            return false;
        }

        return !kernel::TaskMessageDispatcher<
                   FakeMessages,
                   decltype(table),
                   DispatchTrace>{}
                    .valid() &&
               unbound.accepted &&
               unbound.matched && unbound.handler_valid &&
               unbound.handled && !unbound.replied &&
               unbound.reply_value == 1099u && echo_state.calls == 1u &&
               echo_state.last_from == 4u && echo_state.last_value == 99u &&
               echo_state.last_sequence == 1u &&
               unbound_state.reply_calls == 0u && missing.accepted &&
               !missing.matched && !missing.handler_valid &&
               !missing.handled && !missing.replied &&
               missing.reply_value == 0u && missing_state.receive_calls == 1u &&
               missing_state.reply_calls == 0u && !idle.accepted &&
               trace.size() == 2u && first->sequence == 1u &&
               same_text(first->label_name, "echo"sv) &&
               first->accepted && first->matched && first->handler_valid &&
               first->handled && !first->replied &&
               first->reply_value == 1099u && second->sequence == 2u &&
               same_text(second->label_name, "unmapped"sv) &&
               second->accepted && !second->matched &&
               !second->handler_valid && !second->handled &&
               !second->replied && second->reply_value == 0u;
    }
}

int main()
{
    const bool direct_ok = demo::probe_direct_dispatch_and_serve_once();
    const bool error_ok = demo::probe_unbound_and_missing();
    const bool ok = direct_ok && error_ok;

    std::printf(
        "[runtime-task-message-dispatch-demo] ok=%d direct=%d error=%d\n",
        ok ? 1 : 0,
        direct_ok ? 1 : 0,
        error_ok ? 1 : 0);
    return ok ? 0 : 1;
}
