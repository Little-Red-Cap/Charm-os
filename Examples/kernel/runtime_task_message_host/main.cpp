#include <cstdint>
#include <cstdio>

import kernel.task_message_api;

namespace demo {
    struct FakeMailboxState {
        bool bound{true};
        kernel::TaskId server{7u};
        std::uint32_t send_calls{0};
        std::uint32_t receive_calls{0};
        std::uint32_t wait_receive_calls{0};
        std::uint32_t receive_timeout_calls{0};
        std::uint32_t reply_calls{0};
        std::uint32_t wait_reply_calls{0};
        std::uint32_t receive_reply_calls{0};
        std::uint32_t reply_timeout_calls{0};
        std::uint64_t last_send_label{0};
        std::uint64_t last_send_value{0};
        std::uint64_t last_send_sequence{0};
        std::uint64_t last_wait_receive_due{0};
        std::uint64_t last_reply_to{0};
        std::uint64_t last_reply_sequence{0};
        std::uint64_t last_reply_value{0};
        std::uint64_t last_wait_reply_due{0};
        bool receive_timeout_ready{false};
        bool reply_timeout_ready{false};
        bool request_ready{false};
        bool reply_ready{false};
        kernel::RuntimeMailboxRequest request{};
        kernel::RuntimeMailboxReply reply{};
    };

    struct FakeMailbox {
        using tick_type = std::uint64_t;
        using request_type = kernel::RuntimeMailboxRequest;
        using reply_type = kernel::RuntimeMailboxReply;

        FakeMailboxState* state{nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] kernel::TaskId server() const noexcept
        {
            return valid() ? state->server : kernel::TaskId{};
        }

        [[nodiscard]] bool send_current(
            std::uint64_t label,
            std::uint64_t value,
            std::uint64_t sequence = 0) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->send_calls;
            state->last_send_label = label;
            state->last_send_value = value;
            state->last_send_sequence = sequence;
            return true;
        }

        [[nodiscard]] bool receive_current(request_type& out) const noexcept
        {
            if (!valid() || !state->request_ready) {
                return false;
            }

            ++state->receive_calls;
            out = state->request;
            state->request_ready = false;
            return true;
        }

        [[nodiscard]] bool wait_receive_current_until(
            tick_type due) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->wait_receive_calls;
            state->last_wait_receive_due = due;
            return true;
        }

        [[nodiscard]] bool consume_receive_timeout_current(
            kernel::Event event) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->receive_timeout_calls;
            if (event.id != kernel::EventId::sync ||
                kernel::payload_u32(event) !=
                    kernel::runtime_mailbox_receive_timeout_code ||
                !state->receive_timeout_ready) {
                return false;
            }

            state->receive_timeout_ready = false;
            return true;
        }

        [[nodiscard]] bool reply_current(
            kernel::TaskId to,
            std::uint64_t sequence,
            std::uint64_t value) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->reply_calls;
            state->last_reply_to = to.value;
            state->last_reply_sequence = sequence;
            state->last_reply_value = value;
            return true;
        }

        [[nodiscard]] bool wait_reply_current_until(
            tick_type due) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->wait_reply_calls;
            state->last_wait_reply_due = due;
            return true;
        }

        [[nodiscard]] bool receive_reply_current(reply_type& out) const noexcept
        {
            if (!valid() || !state->reply_ready) {
                return false;
            }

            ++state->receive_reply_calls;
            out = state->reply;
            state->reply_ready = false;
            return true;
        }

        [[nodiscard]] bool consume_reply_timeout_current(
            kernel::Event event) const noexcept
        {
            if (!valid()) {
                return false;
            }

            ++state->reply_timeout_calls;
            if (event.id != kernel::EventId::sync ||
                kernel::payload_u32(event) !=
                    kernel::runtime_mailbox_reply_timeout_code ||
                !state->reply_timeout_ready) {
                return false;
            }

            state->reply_timeout_ready = false;
            return true;
        }
    };

    using TaskMessages = kernel::TaskMessageApi<FakeMailbox>;

    [[nodiscard]] bool probe_default_unbound_task_message_api() noexcept
    {
        TaskMessages messages{};
        kernel::RuntimeMailboxRequest request{};
        kernel::RuntimeMailboxReply reply{};

        return !messages.valid() && !messages.mailbox().valid() &&
               messages.server() == kernel::TaskId{} &&
               !messages.send(0x10u, 0x20u, 0x30u) &&
               !messages.send(kernel::TaskMessageSendView{
                   .label = 1u,
                   .value = 2u,
                   .sequence = 3u,
               }) &&
               !messages.receive(request) &&
               !messages.wait_receive_until(7u) &&
               !messages.consume_receive_timeout(
                   kernel::make_runtime_mailbox_receive_timeout_event()) &&
               !messages.reply(kernel::TaskId{5u}, 6u, 7u) &&
               !messages.reply(kernel::TaskMessageReplyView{
                   .to = kernel::TaskId{8u},
                   .sequence = 9u,
                   .value = 10u,
               }) &&
               !messages.reply(request, 11u) &&
               !messages.wait_reply_until(12u) &&
               !messages.receive_reply(reply) &&
               !messages.consume_reply_timeout(
                   kernel::make_runtime_mailbox_reply_timeout_event());
    }

    [[nodiscard]] bool probe_task_named_message_entry_points() noexcept
    {
        FakeMailboxState state{};
        FakeMailbox mailbox{
            .state = &state,
        };
        auto messages = kernel::make_task_message_api(mailbox);

        state.request_ready = true;
        state.request = kernel::RuntimeMailboxRequest{
            .from = kernel::TaskId{3u},
            .label = 0xCA11u,
            .value = 42u,
            .sequence = 0x55u,
        };
        state.receive_timeout_ready = true;
        state.reply_ready = true;
        state.reply = kernel::RuntimeMailboxReply{
            .from = kernel::TaskId{state.server.value},
            .to = kernel::TaskId{3u},
            .sequence = 0x55u,
            .value = 1042u,
        };
        state.reply_timeout_ready = true;

        kernel::RuntimeMailboxRequest received_request{};
        kernel::RuntimeMailboxReply received_reply{};

        const bool sent_scalar = messages.send(0x10u, 0x20u, 0x30u);
        const bool sent_view = messages.send(kernel::TaskMessageSendView{
            .label = 0x40u,
            .value = 0x50u,
            .sequence = 0x60u,
        });
        const bool wait_receive = messages.wait_receive_until(33u);
        const bool receive_timeout = messages.consume_receive_timeout(
            kernel::make_runtime_mailbox_receive_timeout_event());
        const bool received = messages.receive(received_request);
        const bool replied_request = messages.reply(received_request, 1042u);
        const bool replied_view = messages.reply(kernel::TaskMessageReplyView{
            .to = kernel::TaskId{9u},
            .sequence = 0x77u,
            .value = 0x88u,
        });
        const bool wait_reply = messages.wait_reply_until(44u);
        const bool receive_reply = messages.receive_reply(received_reply);
        const bool reply_timeout = messages.consume_reply_timeout(
            kernel::make_runtime_mailbox_reply_timeout_event());

        return messages.valid() && messages.mailbox().valid() &&
               messages.server() == state.server && sent_scalar && sent_view &&
               wait_receive && receive_timeout && received &&
               replied_request && replied_view && wait_reply &&
               receive_reply && reply_timeout && state.send_calls == 2u &&
               state.last_send_label == 0x40u &&
               state.last_send_value == 0x50u &&
               state.last_send_sequence == 0x60u &&
               state.wait_receive_calls == 1u &&
               state.last_wait_receive_due == 33u &&
               state.receive_timeout_calls == 1u &&
               state.receive_calls == 1u &&
               received_request.from == kernel::TaskId{3u} &&
               received_request.label == 0xCA11u &&
               received_request.value == 42u &&
               received_request.sequence == 0x55u &&
               state.reply_calls == 2u && state.last_reply_to == 9u &&
               state.last_reply_sequence == 0x77u &&
               state.last_reply_value == 0x88u &&
               state.wait_reply_calls == 1u &&
               state.last_wait_reply_due == 44u &&
               state.receive_reply_calls == 1u &&
               received_reply.from == state.server &&
               received_reply.to == kernel::TaskId{3u} &&
               received_reply.sequence == 0x55u &&
               received_reply.value == 1042u &&
               state.reply_timeout_calls == 1u;
    }

    [[nodiscard]] bool probe_bind_and_unbind_mailbox() noexcept
    {
        FakeMailboxState first{
            .server = kernel::TaskId{11u},
        };
        FakeMailboxState second{
            .server = kernel::TaskId{22u},
        };
        FakeMailbox mailbox{
            .state = &first,
        };
        TaskMessages messages{mailbox};

        const bool first_send = messages.send(1u, 2u, 3u);
        messages.unbind_mailbox();
        const bool unbound_valid = messages.valid();
        const bool unbound_wait = messages.wait_reply_until(99u);

        FakeMailbox rebound{
            .state = &second,
        };
        messages.bind_mailbox(rebound);
        const bool rebound_reply =
            messages.reply(kernel::TaskId{5u}, 6u, 7u);

        return first_send && !unbound_valid && !unbound_wait &&
               messages.valid() && messages.server() == second.server &&
               rebound_reply && first.send_calls == 1u &&
               second.reply_calls == 1u && second.last_reply_to == 5u &&
               second.last_reply_sequence == 6u &&
               second.last_reply_value == 7u;
    }
}

int main()
{
    const bool default_unbound_ok =
        demo::probe_default_unbound_task_message_api();
    const bool entry_points_ok =
        demo::probe_task_named_message_entry_points();
    const bool bind_ok = demo::probe_bind_and_unbind_mailbox();
    const bool ok = default_unbound_ok && entry_points_ok && bind_ok;

    std::printf(
        "[runtime-task-message-demo] ok=%d default_unbound=%d entry_points=%d bind=%d\n",
        ok ? 1 : 0,
        default_unbound_ok ? 1 : 0,
        entry_points_ok ? 1 : 0,
        bind_ok ? 1 : 0);
    return ok ? 0 : 1;
}
