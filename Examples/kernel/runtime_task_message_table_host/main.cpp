#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_table;

namespace demo {
    using namespace std::literals;

    inline constexpr std::uint64_t kEchoLabel{0xCA11u};
    inline constexpr std::uint64_t kSequenceLabel{0xD00Du};

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

    using TableTrace = kernel::TaskMessageTableTraceBuffer<8>;
    using StaticTable = kernel::TaskMessageTable<2, TableTrace>;

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] bool probe_lookup_and_dispatch() noexcept
    {
        EchoHandlerState echo_state{};
        SequenceHandlerState sequence_state{};
        TableTrace trace{};
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
            },
            &trace);

        const auto echo_lookup = table.lookup(kEchoLabel);
        const auto sequence_lookup = table.lookup(kSequenceLabel);
        const auto echoed =
            table.dispatch(kernel::TaskId{2u}, kEchoLabel, 42u, 0x55u);
        const auto sequenced = table.dispatch(
            kernel::RuntimeMailboxRequest{
                .from = kernel::TaskId{3u},
                .label = kSequenceLabel,
                .value = 7u,
                .sequence = 0x77u,
            });

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        if (first == nullptr || second == nullptr) {
            return false;
        }

        const auto first_request =
            kernel::task_message_request_from_trace_event(*first);

        return echo_lookup.matched && echo_lookup.slot == 0u &&
               echo_lookup.entry != nullptr &&
               sequence_lookup.matched && sequence_lookup.slot == 1u &&
               sequence_lookup.entry != nullptr && echoed.handled &&
               echoed.reply_value == 1042u && sequenced.handled &&
               sequenced.reply_value == 0x77u && echo_state.calls == 1u &&
               echo_state.last_from == 2u && echo_state.last_value == 42u &&
               echo_state.last_sequence == 0x55u &&
               sequence_state.calls == 1u &&
               sequence_state.last_label == kSequenceLabel &&
               sequence_state.last_sequence == 0x77u && trace.size() == 2u &&
               first->sequence == 1u && first->from == kernel::TaskId{2u} &&
               first->label == kEchoLabel &&
               same_text(first->label_name, "echo"sv) &&
               first->slot == 0u && first->matched && first->handler_valid &&
               first->handled && first->reply_value == 1042u &&
               first_request.from == kernel::TaskId{2u} &&
               first_request.label == kEchoLabel &&
               first_request.value == 42u &&
               first_request.sequence == 0x55u && second->sequence == 2u &&
               second->from == kernel::TaskId{3u} &&
               second->label == kSequenceLabel &&
               same_text(second->label_name, "sequence"sv) &&
               second->slot == 1u && second->matched &&
               second->handler_valid && second->handled &&
               second->reply_value == 0x77u;
    }

    [[nodiscard]] bool probe_bind_and_missing_entry() noexcept
    {
        EchoHandlerState echo_state{};
        TableTrace trace{};
        EchoHandler echo_handler{
            .state = &echo_state,
        };
        StaticTable table{
            std::array<kernel::TaskMessageHandlerEntry, 2>{
                kernel::task_message_handler_entry(kEchoLabel, "echo"),
                kernel::task_message_handler_entry(kSequenceLabel, "sequence"),
            },
            &trace,
        };

        const auto unbound = table.dispatch(
            kernel::RuntimeMailboxRequest{
                .from = kernel::TaskId{4u},
                .label = kEchoLabel,
                .value = 99u,
                .sequence = 1u,
            });

        table.bind_entry(
            0u,
            kernel::task_message_handler_entry(
                kEchoLabel,
                "echo",
                kernel::make_task_message_handler(echo_handler)));
        const auto rebound =
            table.dispatch(kernel::TaskId{5u}, kEchoLabel, 11u, 2u);
        const auto missing =
            table.dispatch(kernel::TaskId{6u}, 0xBEEFu, 12u, 3u);

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        const auto* third = trace.at(2u);
        if (first == nullptr || second == nullptr || third == nullptr) {
            return false;
        }

        return !unbound.handled && rebound.handled &&
               rebound.reply_value == 1011u && !missing.handled &&
               echo_state.calls == 1u && echo_state.last_from == 5u &&
               echo_state.last_value == 11u && echo_state.last_sequence == 2u &&
               trace.size() == 3u && first->sequence == 1u &&
               first->matched && !first->handler_valid && !first->handled &&
               same_text(first->label_name, "echo"sv) &&
               second->sequence == 2u && second->matched &&
               second->handler_valid && second->handled &&
               second->reply_value == 1011u &&
               same_text(second->label_name, "echo"sv) &&
               third->sequence == 3u && !third->matched &&
               !third->handler_valid && !third->handled &&
               third->slot == kernel::task_message_table_unmapped_slot &&
               same_text(third->label_name, "unmapped"sv);
    }
}

int main()
{
    const bool lookup_ok = demo::probe_lookup_and_dispatch();
    const bool error_ok = demo::probe_bind_and_missing_entry();
    const bool ok = lookup_ok && error_ok;

    std::printf(
        "[runtime-task-message-table-demo] ok=%d lookup=%d error=%d\n",
        ok ? 1 : 0,
        lookup_ok ? 1 : 0,
        error_ok ? 1 : 0);
    return ok ? 0 : 1;
}
