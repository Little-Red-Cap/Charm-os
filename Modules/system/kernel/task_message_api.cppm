export module kernel.task_message_api;

export import kernel.eda;
export import kernel.evt;
export import kernel.runtime_mailbox;
import util.core;

export namespace kernel {
    struct TaskMessageSendView {
        util::u64 label{0};
        util::u64 value{0};
        util::u64 sequence{0};
    };

    struct TaskMessageReplyView {
        TaskId to{};
        util::u64 sequence{0};
        util::u64 value{0};
    };

    template <typename Mailbox>
    class TaskMessageApi {
    public:
        using mailbox_type = Mailbox;
        using tick_type = typename Mailbox::tick_type;
        using request_type = typename Mailbox::request_type;
        using reply_type = typename Mailbox::reply_type;

        constexpr TaskMessageApi() noexcept = default;

        constexpr explicit TaskMessageApi(Mailbox& mailbox) noexcept
            : mailbox_(&mailbox)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return mailbox().valid();
        }

        [[nodiscard]] Mailbox& mailbox() noexcept
        {
            return mailbox_ != nullptr ? *mailbox_ : storage_;
        }

        [[nodiscard]] const Mailbox& mailbox() const noexcept
        {
            return mailbox_ != nullptr ? *mailbox_ : storage_;
        }

        [[nodiscard]] TaskId server() const noexcept
        {
            return mailbox().server();
        }

        void bind_mailbox(Mailbox& mailbox) noexcept
        {
            mailbox_ = &mailbox;
        }

        void unbind_mailbox() noexcept
        {
            mailbox_ = nullptr;
        }

        [[nodiscard]] bool send(
            util::u64 label,
            util::u64 value,
            util::u64 sequence = 0) noexcept
        {
            return mailbox().send_current(label, value, sequence);
        }

        [[nodiscard]] bool send(TaskMessageSendView send_view) noexcept
        {
            return send(send_view.label, send_view.value, send_view.sequence);
        }

        [[nodiscard]] bool receive(request_type& out) noexcept
        {
            return mailbox().receive_current(out);
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            return mailbox().wait_receive_current_until(due);
        }

        [[nodiscard]] bool consume_receive_timeout(Event event) noexcept
        {
            return mailbox().consume_receive_timeout_current(event);
        }

        [[nodiscard]] bool reply(
            TaskId to,
            util::u64 sequence,
            util::u64 value) noexcept
        {
            return mailbox().reply_current(to, sequence, value);
        }

        [[nodiscard]] bool reply(TaskMessageReplyView reply_view) noexcept
        {
            return reply(reply_view.to, reply_view.sequence, reply_view.value);
        }

        [[nodiscard]] bool reply(
            const request_type& request,
            util::u64 value) noexcept
        {
            return reply(request.from, request.sequence, value);
        }

        [[nodiscard]] bool wait_reply_until(tick_type due) noexcept
        {
            return mailbox().wait_reply_current_until(due);
        }

        [[nodiscard]] bool receive_reply(reply_type& out) noexcept
        {
            return mailbox().receive_reply_current(out);
        }

        [[nodiscard]] bool consume_reply_timeout(Event event) noexcept
        {
            return mailbox().consume_reply_timeout_current(event);
        }

    private:
        Mailbox* mailbox_{nullptr};
        Mailbox storage_{};
    };

    template <typename Mailbox>
    [[nodiscard]] auto make_task_message_api(Mailbox& mailbox) noexcept
        -> TaskMessageApi<Mailbox>
    {
        return TaskMessageApi<Mailbox>{mailbox};
    }
}
