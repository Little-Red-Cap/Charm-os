module;

export module kernel.task_message_syscall_frame_caller;

export import kernel.task_message_syscall_frame;
import kernel.context;
import util.core;

export namespace kernel {
    template <typename Frame>
    struct TaskMessageSyscallFrameCallAdapter {
        void* ctx{nullptr};
        bool (*make_frame)(void* ctx,
                           TaskSyscallRequest request,
                           Frame& out) noexcept {nullptr};
        bool (*capture_result)(void* ctx,
                               const Frame& frame,
                               util::u64 reply_value,
                               TrapResult& out) noexcept {nullptr};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool task_message_syscall_frame_call_adapter_ready(
        const TaskMessageSyscallFrameCallAdapter<Frame>& adapter) noexcept
    {
        return adapter.make_frame != nullptr &&
               adapter.capture_result != nullptr;
    }

    struct TaskMessageSyscallFrameCallerState {
        TaskId owner{};
        util::u64 token{0};
        util::u64 sequence{0};
        bool pending{false};
    };

    [[nodiscard]] constexpr bool task_message_syscall_frame_caller_pending(
        const TaskMessageSyscallFrameCallerState& state) noexcept
    {
        return state.pending;
    }

    template <typename Messages,
              typename Frames,
              typename Adapter =
                  TaskMessageSyscallFrameCallAdapter<
                      typename Frames::frame_type>>
    class TaskMessageSyscallFrameCaller {
    public:
        using messages_type = Messages;
        using frames_type = Frames;
        using frame_type = typename Frames::frame_type;
        using adapter_type = Adapter;
        using tick_type = typename Messages::tick_type;
        using reply_type = typename Messages::reply_type;

        constexpr TaskMessageSyscallFrameCaller() noexcept = default;

        constexpr explicit TaskMessageSyscallFrameCaller(
            Messages& messages,
            Frames& frames,
            Adapter adapter = {}) noexcept
            : messages_(&messages), frames_(&frames), adapter_(adapter)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return messages_ != nullptr && frames_ != nullptr &&
                   messages().valid() && frames().valid() &&
                   task_message_syscall_frame_call_adapter_ready(adapter_);
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return state_.pending;
        }

        [[nodiscard]] const TaskMessageSyscallFrameCallerState& state() const
            noexcept
        {
            return state_;
        }

        [[nodiscard]] Messages& messages() noexcept
        {
            return *messages_;
        }

        [[nodiscard]] const Messages& messages() const noexcept
        {
            return *messages_;
        }

        [[nodiscard]] Frames& frames() noexcept
        {
            return *frames_;
        }

        [[nodiscard]] const Frames& frames() const noexcept
        {
            return *frames_;
        }

        [[nodiscard]] const Adapter& adapter() const noexcept
        {
            return adapter_;
        }

        [[nodiscard]] util::u64 next_token() const noexcept
        {
            return next_token_;
        }

        [[nodiscard]] util::u64 next_sequence() const noexcept
        {
            return next_sequence_;
        }

        void bind_messages(Messages& messages) noexcept
        {
            messages_ = &messages;
        }

        void bind_frames(Frames& frames) noexcept
        {
            frames_ = &frames;
        }

        void bind_adapter(Adapter adapter) noexcept
        {
            adapter_ = adapter;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            next_token_ = next_token != 0 ? next_token : 1u;
            next_sequence_ = next_sequence != 0 ? next_sequence : 1u;
        }

        [[nodiscard]] bool begin(TaskSyscallRequest request,
                                 tick_type wait_due) noexcept
        {
            if (!has_current()) {
                return false;
            }

            return begin(current_task(), request, wait_due);
        }

        [[nodiscard]] bool begin(TaskId owner,
                                 TaskSyscallRequest request,
                                 tick_type wait_due) noexcept
        {
            return begin(
                owner, consume_next_token(), consume_next_sequence(), request, wait_due);
        }

        [[nodiscard]] bool begin(TaskId owner,
                                 util::u64 token,
                                 util::u64 sequence,
                                 TaskSyscallRequest request,
                                 tick_type wait_due) noexcept
        {
            if (!valid() || busy()) {
                return false;
            }

            frame_type frame{};
            if (!adapter_.make_frame(adapter_.ctx, request, frame)) {
                return false;
            }

            if (!frames().publish(owner, token, frame)) {
                return false;
            }

            auto& mailbox = messages().mailbox();
            if (!mailbox.wait_reply_until(owner, wait_due)) {
                (void)frames().erase(owner, token);
                return false;
            }

            const auto sent = mailbox.send(
                owner,
                task_message_syscall_frame_request_label,
                token,
                sequence);
            if (!sent) {
                clear_reply_wait(owner);
                (void)frames().erase(owner, token);
                return false;
            }

            state_ = TaskMessageSyscallFrameCallerState{
                .owner = owner,
                .token = token,
                .sequence = sequence,
                .pending = true,
            };
            return true;
        }

        [[nodiscard]] bool yield_current(tick_type wait_due) noexcept
        {
            return begin(make_task_syscall_yield_request(), wait_due);
        }

        [[nodiscard]] bool yield_current(TaskId owner,
                                         tick_type wait_due) noexcept
        {
            return begin(owner, make_task_syscall_yield_request(), wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_until(TrapSleepUntilView<Tick> sleep,
                                       tick_type wait_due) noexcept
        {
            return begin(make_task_syscall_sleep_until_request(sleep), wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_until(TaskId owner,
                                       TrapSleepUntilView<Tick> sleep,
                                       tick_type wait_due) noexcept
        {
            return begin(
                owner, make_task_syscall_sleep_until_request(sleep), wait_due);
        }

        [[nodiscard]] bool debug_write(TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return begin(make_task_syscall_debug_write_request(write), wait_due);
        }

        [[nodiscard]] bool debug_write(TaskId owner,
                                       TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return begin(
                owner, make_task_syscall_debug_write_request(write), wait_due);
        }

        [[nodiscard]] bool capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return begin(make_task_syscall_capability_call_request(capability),
                         wait_due);
        }

        [[nodiscard]] bool capability_call(
            TaskId owner,
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return begin(owner,
                         make_task_syscall_capability_call_request(capability),
                         wait_due);
        }

        [[nodiscard]] bool receive_reply(TrapResult& out) noexcept
        {
            reply_type reply{};
            return receive_reply(reply, out);
        }

        [[nodiscard]] bool receive_reply(reply_type& reply,
                                         TrapResult& out) noexcept
        {
            if (!busy()) {
                return false;
            }

            if (!messages().mailbox().receive_reply(state_.owner, reply)) {
                return false;
            }

            return complete_reply(reply, out);
        }

        [[nodiscard]] bool complete_reply(const reply_type& reply,
                                          TrapResult& out) noexcept
        {
            if (!busy()) {
                return false;
            }

            const auto pending = state_;
            clear_pending();

            if (reply.to != pending.owner || reply.sequence != pending.sequence ||
                reply.from != messages().server()) {
                (void)frames().erase(pending.owner, pending.token);
                out = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::invalid_argument,
                    .value = reply.value,
                };
                return true;
            }

            frame_type frame{};
            if (!frames().take(pending.owner, pending.token, frame)) {
                out = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::invalid_argument,
                    .value = reply.value,
                };
                return true;
            }

            if (!adapter_.capture_result(
                    adapter_.ctx, frame, reply.value, out)) {
                out = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::writeback_failed,
                    .value = reply.value,
                };
                return true;
            }

            return true;
        }

        [[nodiscard]] bool consume_reply_timeout(Event event) noexcept
        {
            if (!busy()) {
                return false;
            }

            if (!messages().mailbox().consume_reply_timeout(state_.owner, event)) {
                return false;
            }

            (void)frames().erase(state_.owner, state_.token);
            clear_pending();
            return true;
        }

        [[nodiscard]] bool cancel_pending() noexcept
        {
            if (!busy()) {
                return false;
            }

            clear_reply_wait(state_.owner);
            (void)frames().erase(state_.owner, state_.token);
            clear_pending();
            return true;
        }

    private:
        [[nodiscard]] util::u64 consume_next_token() noexcept
        {
            const auto value = next_token_;
            advance_cursor(next_token_);
            return value;
        }

        [[nodiscard]] util::u64 consume_next_sequence() noexcept
        {
            const auto value = next_sequence_;
            advance_cursor(next_sequence_);
            return value;
        }

        static void advance_cursor(util::u64& value) noexcept
        {
            ++value;
            if (value == 0) {
                value = 1;
            }
        }

        void clear_reply_wait(TaskId owner) noexcept
        {
            auto& mailbox = messages().mailbox();
            (void)mailbox.consume_reply_timeout(owner, mailbox.reply_timeout_event());
        }

        void clear_pending() noexcept
        {
            state_ = TaskMessageSyscallFrameCallerState{};
        }

        Messages* messages_{nullptr};
        Frames* frames_{nullptr};
        Adapter adapter_{};
        TaskMessageSyscallFrameCallerState state_{};
        util::u64 next_token_{1};
        util::u64 next_sequence_{1};
    };

    template <typename Messages, typename Frames, typename Adapter>
    [[nodiscard]] auto make_task_message_syscall_frame_caller(
        Messages& messages,
        Frames& frames,
        Adapter adapter) noexcept
        -> TaskMessageSyscallFrameCaller<Messages, Frames, Adapter>
    {
        return TaskMessageSyscallFrameCaller<Messages, Frames, Adapter>{
            messages,
            frames,
            adapter,
        };
    }
}
