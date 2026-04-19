module;

#include <array>
#include <cstddef>

export module kernel.runtime_mailbox;

import kernel.context;
import kernel.eda;
import kernel.event_token;
import kernel.evt;
import util.core;

export namespace kernel {
    inline constexpr util::u32 runtime_mailbox_receive_ready_code{1u};
    inline constexpr util::u32 runtime_mailbox_receive_timeout_code{2u};
    inline constexpr util::u32 runtime_mailbox_reply_ready_code{3u};
    inline constexpr util::u32 runtime_mailbox_reply_timeout_code{4u};

    [[nodiscard]] inline Event make_runtime_mailbox_receive_event() noexcept
    {
        return make_event(
            EventId::message,
            static_cast<util::u32>(runtime_mailbox_receive_ready_code));
    }

    [[nodiscard]] inline Event make_runtime_mailbox_receive_timeout_event()
        noexcept
    {
        return make_event(
            EventId::sync,
            static_cast<util::u32>(runtime_mailbox_receive_timeout_code));
    }

    [[nodiscard]] inline Event make_runtime_mailbox_reply_event() noexcept
    {
        return make_event(
            EventId::message,
            static_cast<util::u32>(runtime_mailbox_reply_ready_code));
    }

    [[nodiscard]] inline Event make_runtime_mailbox_reply_timeout_event()
        noexcept
    {
        return make_event(
            EventId::sync,
            static_cast<util::u32>(runtime_mailbox_reply_timeout_code));
    }

    struct RuntimeMailboxRequest {
        TaskId from{};
        util::u64 label{0};
        util::u64 value{0};
        util::u64 sequence{0};
    };

    struct RuntimeMailboxReply {
        TaskId from{};
        TaskId to{};
        util::u64 sequence{0};
        util::u64 value{0};
    };

    namespace detail {
        [[nodiscard]] constexpr bool runtime_mailbox_event_matches(
            Event lhs,
            Event rhs) noexcept
        {
            return lhs.id == rhs.id && payload_u64(lhs) == payload_u64(rhs);
        }
    }

    template <typename Scheduler,
              util::usize RequestCapacity = 4,
              util::usize ReplyCapacity = 4,
              util::usize ReplyWaitCapacity = 4>
    class RuntimeMailbox {
    public:
        using scheduler_type = Scheduler;
        using tick_type = typename Scheduler::TimeSource::Tick;
        using request_type = RuntimeMailboxRequest;
        using reply_type = RuntimeMailboxReply;

        static_assert(RequestCapacity >= 1);
        static_assert(ReplyCapacity >= 1);
        static_assert(ReplyWaitCapacity >= 1);

        constexpr RuntimeMailbox() noexcept = default;

        RuntimeMailbox(
            Scheduler& scheduler,
            TaskId server,
            Event receive_event = make_runtime_mailbox_receive_event(),
            Event receive_timeout_event =
                make_runtime_mailbox_receive_timeout_event(),
            Event reply_event = make_runtime_mailbox_reply_event(),
            Event reply_timeout_event =
                make_runtime_mailbox_reply_timeout_event()) noexcept
            : scheduler_(&scheduler), server_(server), receive_event_(receive_event),
              receive_timeout_event_(receive_timeout_event),
              reply_event_(reply_event),
              reply_timeout_event_(reply_timeout_event)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return scheduler_ != nullptr;
        }

        [[nodiscard]] Scheduler& scheduler() noexcept
        {
            return *scheduler_;
        }

        [[nodiscard]] const Scheduler& scheduler() const noexcept
        {
            return *scheduler_;
        }

        [[nodiscard]] TaskId server() const noexcept
        {
            return server_;
        }

        [[nodiscard]] Event receive_event() const noexcept
        {
            return receive_event_;
        }

        [[nodiscard]] Event receive_timeout_event() const noexcept
        {
            return receive_timeout_event_;
        }

        [[nodiscard]] Event reply_event() const noexcept
        {
            return reply_event_;
        }

        [[nodiscard]] Event reply_timeout_event() const noexcept
        {
            return reply_timeout_event_;
        }

        [[nodiscard]] util::usize pending_requests() const noexcept
        {
            return request_count_;
        }

        [[nodiscard]] util::usize pending_replies() const noexcept
        {
            return reply_count_;
        }

        [[nodiscard]] bool receive_waiting() const noexcept
        {
            return receive_waiting_;
        }

        [[nodiscard]] util::usize reply_waiters() const noexcept
        {
            return reply_waiter_count_;
        }

        [[nodiscard]] bool waiting_reply(TaskId task) const noexcept
        {
            return find_reply_waiter(task) < ReplyWaitCapacity;
        }

        [[nodiscard]] bool send(request_type request) noexcept
        {
            if (!valid() || !push_request(request)) {
                return false;
            }

            if (receive_waiting_) {
                cancel_receive_wait();
            }

            return scheduler_->post_demand(server_, receive_event_);
        }

        [[nodiscard]] bool send(
            TaskId from,
            util::u64 label,
            util::u64 value,
            util::u64 sequence = 0) noexcept
        {
            return send(request_type{
                .from = from,
                .label = label,
                .value = value,
                .sequence = sequence,
            });
        }

        [[nodiscard]] bool send_current(
            util::u64 label,
            util::u64 value,
            util::u64 sequence = 0) noexcept
        {
            if (!has_current()) {
                return false;
            }

            return send(current_task(), label, value, sequence);
        }

        [[nodiscard]] bool receive(request_type& out) noexcept
        {
            return pop_request(out);
        }

        [[nodiscard]] bool receive_current(request_type& out) noexcept
        {
            return has_current() && current_task() == server_ && receive(out);
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            if (!valid() || receive_waiting_) {
                return false;
            }

            const auto token =
                scheduler_->schedule_at_token(due, server_, receive_timeout_event_);
            if (token.value == 0) {
                return false;
            }

            receive_waiting_ = true;
            receive_timeout_token_ = token;
            return true;
        }

        [[nodiscard]] bool wait_receive_current_until(tick_type due) noexcept
        {
            return has_current() && current_task() == server_ &&
                   wait_receive_until(due);
        }

        [[nodiscard]] bool consume_receive_timeout(
            TaskId task,
            Event event) noexcept
        {
            if (task != server_ || !receive_waiting_ ||
                !detail::runtime_mailbox_event_matches(
                    event, receive_timeout_event_)) {
                return false;
            }

            clear_receive_wait();
            return true;
        }

        [[nodiscard]] bool consume_receive_timeout_current(Event event) noexcept
        {
            return has_current() &&
                   consume_receive_timeout(current_task(), event);
        }

        [[nodiscard]] bool reply(reply_type reply) noexcept
        {
            if (!valid() || !push_reply(reply)) {
                return false;
            }

            (void)cancel_reply_wait(reply.to);
            return scheduler_->post_demand(reply.to, reply_event_);
        }

        [[nodiscard]] bool reply(
            TaskId from,
            TaskId to,
            util::u64 sequence,
            util::u64 value) noexcept
        {
            return reply(reply_type{
                .from = from,
                .to = to,
                .sequence = sequence,
                .value = value,
            });
        }

        [[nodiscard]] bool reply_current(
            TaskId to,
            util::u64 sequence,
            util::u64 value) noexcept
        {
            if (!has_current()) {
                return false;
            }

            return reply(current_task(), to, sequence, value);
        }

        [[nodiscard]] bool receive_reply(TaskId task, reply_type& out) noexcept
        {
            return take_reply(task, out);
        }

        [[nodiscard]] bool receive_reply_current(reply_type& out) noexcept
        {
            return has_current() && receive_reply(current_task(), out);
        }

        [[nodiscard]] bool wait_reply_until(TaskId task, tick_type due) noexcept
        {
            if (!valid() || find_reply_waiter(task) < ReplyWaitCapacity) {
                return false;
            }

            const auto token =
                scheduler_->schedule_at_token(due, task, reply_timeout_event_);
            if (token.value == 0) {
                return false;
            }

            return add_reply_waiter(task, token);
        }

        [[nodiscard]] bool wait_reply_current_until(tick_type due) noexcept
        {
            return has_current() && wait_reply_until(current_task(), due);
        }

        [[nodiscard]] bool consume_reply_timeout(
            TaskId task,
            Event event) noexcept
        {
            if (!detail::runtime_mailbox_event_matches(
                    event, reply_timeout_event_)) {
                return false;
            }

            const auto index = find_reply_waiter(task);
            if (index >= ReplyWaitCapacity) {
                return false;
            }

            erase_reply_waiter(index);
            return true;
        }

        [[nodiscard]] bool consume_reply_timeout_current(Event event) noexcept
        {
            return has_current() && consume_reply_timeout(current_task(), event);
        }

    private:
        struct ReplySlot {
            bool engaged{false};
            reply_type reply{};
        };

        struct ReplyWaiter {
            bool engaged{false};
            TaskId task{};
            EventToken timeout{};
        };

        [[nodiscard]] bool push_request(request_type request) noexcept
        {
            if (request_count_ >= RequestCapacity) {
                return false;
            }

            requests_[request_tail_] = request;
            request_tail_ = (request_tail_ + 1) % RequestCapacity;
            ++request_count_;
            return true;
        }

        [[nodiscard]] bool pop_request(request_type& out) noexcept
        {
            if (request_count_ == 0) {
                return false;
            }

            out = requests_[request_head_];
            request_head_ = (request_head_ + 1) % RequestCapacity;
            --request_count_;
            return true;
        }

        [[nodiscard]] bool push_reply(reply_type reply) noexcept
        {
            for (auto& slot : replies_) {
                if (slot.engaged) {
                    continue;
                }

                slot.engaged = true;
                slot.reply = reply;
                ++reply_count_;
                return true;
            }

            return false;
        }

        [[nodiscard]] bool take_reply(TaskId task, reply_type& out) noexcept
        {
            for (auto& slot : replies_) {
                if (!slot.engaged || slot.reply.to != task) {
                    continue;
                }

                out = slot.reply;
                slot.engaged = false;
                --reply_count_;
                return true;
            }

            return false;
        }

        [[nodiscard]] util::usize find_reply_waiter(TaskId task) const noexcept
        {
            for (util::usize i = 0; i < ReplyWaitCapacity; ++i) {
                if (reply_waiters_[i].engaged &&
                    reply_waiters_[i].task == task) {
                    return i;
                }
            }

            return ReplyWaitCapacity;
        }

        [[nodiscard]] bool add_reply_waiter(
            TaskId task,
            EventToken timeout) noexcept
        {
            for (auto& waiter : reply_waiters_) {
                if (waiter.engaged) {
                    continue;
                }

                waiter.engaged = true;
                waiter.task = task;
                waiter.timeout = timeout;
                ++reply_waiter_count_;
                return true;
            }

            return false;
        }

        void erase_reply_waiter(util::usize index) noexcept
        {
            if (index >= ReplyWaitCapacity || !reply_waiters_[index].engaged) {
                return;
            }

            reply_waiters_[index] = ReplyWaiter{};
            --reply_waiter_count_;
        }

        [[nodiscard]] bool cancel_reply_wait(TaskId task) noexcept
        {
            const auto index = find_reply_waiter(task);
            if (index >= ReplyWaitCapacity) {
                return false;
            }

            const auto timeout = reply_waiters_[index].timeout;
            erase_reply_waiter(index);
            if (timeout.value != 0) {
                (void)scheduler_->cancel_event(timeout);
            }
            return true;
        }

        void clear_receive_wait() noexcept
        {
            receive_waiting_ = false;
            receive_timeout_token_ = EventToken{};
        }

        void cancel_receive_wait() noexcept
        {
            const auto timeout = receive_timeout_token_;
            clear_receive_wait();
            if (timeout.value != 0) {
                (void)scheduler_->cancel_event(timeout);
            }
        }

        Scheduler* scheduler_{nullptr};
        TaskId server_{};
        Event receive_event_{make_runtime_mailbox_receive_event()};
        Event receive_timeout_event_{make_runtime_mailbox_receive_timeout_event()};
        Event reply_event_{make_runtime_mailbox_reply_event()};
        Event reply_timeout_event_{make_runtime_mailbox_reply_timeout_event()};
        std::array<request_type, RequestCapacity> requests_{};
        util::usize request_head_{0};
        util::usize request_tail_{0};
        util::usize request_count_{0};
        std::array<ReplySlot, ReplyCapacity> replies_{};
        util::usize reply_count_{0};
        bool receive_waiting_{false};
        EventToken receive_timeout_token_{};
        std::array<ReplyWaiter, ReplyWaitCapacity> reply_waiters_{};
        util::usize reply_waiter_count_{0};
    };
}
