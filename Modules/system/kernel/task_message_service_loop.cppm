module;

#include <array>
#include <cstddef>

export module kernel.task_message_service_loop;

export import kernel.task_message_dispatch;
import util.core;

export namespace kernel {
    enum class TaskMessageServiceLoopTraceKind : util::u8 {
        wait = 0,
        timeout,
        dispatch,
    };

    [[nodiscard]] constexpr const char* task_message_service_loop_trace_kind_name(
        TaskMessageServiceLoopTraceKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageServiceLoopTraceKind::wait:
            return "wait";
        case TaskMessageServiceLoopTraceKind::timeout:
            return "timeout";
        case TaskMessageServiceLoopTraceKind::dispatch:
            return "dispatch";
        }
        return "unknown";
    }

    struct TaskMessageServiceLoopResult {
        bool progressed{false};
        bool wait_armed{false};
        bool timeout_consumed{false};
        TaskMessageDispatchResult dispatch{};
    };

    struct TaskMessageServiceLoopTraceEvent {
        util::u64 sequence{0};
        TaskMessageServiceLoopTraceKind kind{
            TaskMessageServiceLoopTraceKind::wait};
        EventId event_id{EventId::init};
        util::u64 value{0};
        bool ok{false};
        bool dispatch_accepted{false};
        bool matched{false};
        bool handler_valid{false};
        bool handled{false};
        bool replied{false};
        RuntimeMailboxRequest request{};
        bool request_valid{false};
    };

    template <std::size_t Capacity>
    class TaskMessageServiceLoopTraceBuffer {
    public:
        using value_type = TaskMessageServiceLoopTraceEvent;

        static_assert(Capacity > 0);

        [[nodiscard]] bool push(const value_type& event) noexcept
        {
            events_[head_] = event;
            head_ = (head_ + 1u) % Capacity;
            if (size_ < Capacity) {
                ++size_;
            }
            return true;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] const value_type* at(std::size_t index) const noexcept
        {
            if (index >= size_) {
                return nullptr;
            }

            const auto first = (head_ + Capacity - size_) % Capacity;
            return &events_[(first + index) % Capacity];
        }

    private:
        std::array<value_type, Capacity> events_{};
        std::size_t head_{0};
        std::size_t size_{0};
    };

    template <typename Dispatcher,
              typename TraceBuffer = TaskMessageServiceLoopTraceBuffer<1>>
    class TaskMessageServiceLoop {
    public:
        using dispatcher_type = Dispatcher;
        using messages_type = typename Dispatcher::message_type;
        using tick_type = typename messages_type::tick_type;
        using result_type = TaskMessageServiceLoopResult;
        using trace_type = TraceBuffer;

        constexpr TaskMessageServiceLoop() noexcept = default;

        constexpr explicit TaskMessageServiceLoop(Dispatcher dispatcher,
                                                  TraceBuffer* trace = nullptr) noexcept
            : dispatcher_(dispatcher), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return dispatcher_.valid();
        }

        [[nodiscard]] Dispatcher& dispatcher() noexcept
        {
            return dispatcher_;
        }

        [[nodiscard]] const Dispatcher& dispatcher() const noexcept
        {
            return dispatcher_;
        }

        void bind_dispatcher(Dispatcher dispatcher) noexcept
        {
            dispatcher_ = dispatcher;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] Event receive_event() const noexcept
        {
            if constexpr (requires(const messages_type& messages) {
                              messages.mailbox().receive_event();
                          }) {
                return dispatcher_.messages().mailbox().receive_event();
            } else {
                return make_runtime_mailbox_receive_event();
            }
        }

        [[nodiscard]] Event receive_timeout_event() const noexcept
        {
            if constexpr (requires(const messages_type& messages) {
                              messages.mailbox().receive_timeout_event();
                          }) {
                return dispatcher_.messages().mailbox().receive_timeout_event();
            } else {
                return make_runtime_mailbox_receive_timeout_event();
            }
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            const auto ok = dispatcher_.messages().wait_receive_until(due);
            trace_wait(due, ok);
            return ok;
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            if (!valid()) {
                return result_type{};
            }

            if (dispatcher_.messages().consume_receive_timeout(event)) {
                trace_timeout(event, true);
                return result_type{
                    .progressed = true,
                    .timeout_consumed = true,
                };
            }

            if (!event_matches(event, receive_event())) {
                return result_type{};
            }

            const auto dispatch = dispatcher_.serve_once();
            trace_dispatch(event, dispatch);
            return result_type{
                .progressed = dispatch.accepted,
                .dispatch = dispatch,
            };
        }

        [[nodiscard]] result_type step_and_wait_until(Event event,
                                                      tick_type due) noexcept
        {
            auto result = step(event);
            if (result.timeout_consumed) {
                result.wait_armed = wait_receive_until(due);
                result.progressed = true;
            }
            return result;
        }

    private:
        [[nodiscard]] static constexpr bool event_matches(Event lhs,
                                                          Event rhs) noexcept
        {
            return lhs.id == rhs.id && payload_u64(lhs) == payload_u64(rhs);
        }

        void trace_wait(tick_type due, bool ok) noexcept
        {
            trace_push(TaskMessageServiceLoopTraceEvent{
                .kind = TaskMessageServiceLoopTraceKind::wait,
                .event_id = receive_event().id,
                .value = static_cast<util::u64>(due),
                .ok = ok,
            });
        }

        void trace_timeout(Event event, bool ok) noexcept
        {
            trace_push(TaskMessageServiceLoopTraceEvent{
                .kind = TaskMessageServiceLoopTraceKind::timeout,
                .event_id = event.id,
                .value = payload_u64(event),
                .ok = ok,
            });
        }

        void trace_dispatch(Event event,
                            const TaskMessageDispatchResult& dispatch) noexcept
        {
            trace_push(TaskMessageServiceLoopTraceEvent{
                .kind = TaskMessageServiceLoopTraceKind::dispatch,
                .event_id = event.id,
                .value = dispatch.reply_value,
                .ok = dispatch.accepted,
                .dispatch_accepted = dispatch.accepted,
                .matched = dispatch.matched,
                .handler_valid = dispatch.handler_valid,
                .handled = dispatch.handled,
                .replied = dispatch.replied,
                .request = dispatch.request,
                .request_valid = dispatch.accepted,
            });
        }

        void trace_push(const TaskMessageServiceLoopTraceEvent& event) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            auto traced = event;
            traced.sequence = ++sequence_;
            (void)trace_->push(traced);
        }

        Dispatcher dispatcher_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Dispatcher>
    [[nodiscard]] auto make_task_message_service_loop(
        Dispatcher dispatcher) noexcept -> TaskMessageServiceLoop<Dispatcher>
    {
        return TaskMessageServiceLoop<Dispatcher>{dispatcher};
    }

    template <typename Dispatcher, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_service_loop(
        Dispatcher dispatcher,
        TraceBuffer* trace) noexcept
        -> TaskMessageServiceLoop<Dispatcher, TraceBuffer>
    {
        return TaskMessageServiceLoop<Dispatcher, TraceBuffer>{
            dispatcher,
            trace,
        };
    }
}
