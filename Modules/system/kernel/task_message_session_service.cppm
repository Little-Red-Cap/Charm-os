module;

#include <array>
#include <cstddef>

export module kernel.task_message_session_service;

export import kernel.task_message_service_pump;
export import kernel.task_message_session_acceptor;
import util.core;

export namespace kernel {
    struct TaskMessageSessionServiceTraceEvent {
        util::u64 sequence{0};
        TaskMessageServicePumpReason reason{
            TaskMessageServicePumpReason::none};
        EventId event_id{EventId::init};
        util::u64 event_value{0};
        util::u64 due{0};
        std::size_t budget{0};
        std::size_t served{0};
        std::size_t active_sessions{0};
        std::size_t active_channels{0};
        bool progressed{false};
        bool bootstrap_consumed{false};
        bool wait_armed{false};
        bool hold_ready{false};
        bool dispatch_accepted{false};
        bool dispatch_handled{false};
        bool dispatch_replied{false};
        util::u64 reply_value{0};
    };

    template <std::size_t Capacity>
    class TaskMessageSessionServiceTraceBuffer {
    public:
        using value_type = TaskMessageSessionServiceTraceEvent;

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

    template <typename RawPumpResult>
    struct TaskMessageSessionServiceResult {
        bool progressed{false};
        bool bootstrap_consumed{false};
        bool wait_armed{false};
        bool hold_ready{false};
        TaskMessageServicePumpReason reason{
            TaskMessageServicePumpReason::none};
        std::size_t active_sessions{0};
        std::size_t active_channels{0};
        bool dispatch_accepted{false};
        bool dispatch_handled{false};
        bool dispatch_replied{false};
        util::u64 reply_value{0};
        RawPumpResult raw{};
    };

    template <typename Pump,
              typename SessionDispatcher,
              typename SessionAcceptor,
              typename TraceBuffer = TaskMessageSessionServiceTraceBuffer<1>>
    class TaskMessageSessionService {
    public:
        using pump_type = Pump;
        using session_dispatcher_type = SessionDispatcher;
        using session_acceptor_type = SessionAcceptor;
        using tick_type = typename Pump::tick_type;
        using pump_result_type = typename Pump::result_type;
        using result_type = TaskMessageSessionServiceResult<pump_result_type>;
        using trace_type = TraceBuffer;
        using session_slot_type = typename SessionDispatcher::slot_type;
        using channel_slot_type = typename SessionAcceptor::slot_type;

        constexpr TaskMessageSessionService() noexcept = default;

        constexpr explicit TaskMessageSessionService(
            Pump pump,
            SessionDispatcher& dispatcher,
            SessionAcceptor& acceptor,
            TraceBuffer* trace = nullptr) noexcept
            : pump_(pump), dispatcher_(&dispatcher), acceptor_(&acceptor),
              trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return pump_.valid() && dispatcher_ != nullptr &&
                   acceptor_ != nullptr;
        }

        [[nodiscard]] Pump& pump() noexcept
        {
            return pump_;
        }

        [[nodiscard]] const Pump& pump() const noexcept
        {
            return pump_;
        }

        [[nodiscard]] SessionDispatcher& dispatcher() noexcept
        {
            return *dispatcher_;
        }

        [[nodiscard]] const SessionDispatcher& dispatcher() const noexcept
        {
            return *dispatcher_;
        }

        [[nodiscard]] SessionAcceptor& acceptor() noexcept
        {
            return *acceptor_;
        }

        [[nodiscard]] const SessionAcceptor& acceptor() const noexcept
        {
            return *acceptor_;
        }

        void bind_pump(Pump pump) noexcept
        {
            pump_ = pump;
        }

        void bind_dispatcher(SessionDispatcher& dispatcher) noexcept
        {
            dispatcher_ = &dispatcher;
        }

        void bind_acceptor(SessionAcceptor& acceptor) noexcept
        {
            acceptor_ = &acceptor;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] const char* service_name() const noexcept
        {
            if (acceptor_ == nullptr) {
                return "session-service";
            }

            return acceptor_->service_name();
        }

        [[nodiscard]] Event bootstrap_event() const noexcept
        {
            return pump_.bootstrap_event();
        }

        void bind_bootstrap_event(Event event) noexcept
        {
            pump_.bind_bootstrap_event(event);
        }

        [[nodiscard]] Event receive_event() const noexcept
        {
            return pump_.receive_event();
        }

        [[nodiscard]] Event receive_timeout_event() const noexcept
        {
            return pump_.receive_timeout_event();
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            return pump_.wait_receive_until(due);
        }

        [[nodiscard]] result_type step(Event event,
                                       std::size_t budget,
                                       tick_type due) noexcept
        {
            const auto raw = pump_.step(event, budget, due);
            const auto& last_dispatch = raw.drain.last_dispatch;
            auto result = result_type{
                .progressed = raw.progressed,
                .bootstrap_consumed = raw.bootstrap_consumed,
                .wait_armed = raw.wait_armed,
                .hold_ready = raw.hold_ready,
                .reason = raw.reason,
                .active_sessions = active_sessions(),
                .active_channels = active_channels(),
                .dispatch_accepted = last_dispatch.accepted,
                .dispatch_handled = last_dispatch.handled,
                .dispatch_replied = last_dispatch.replied,
                .reply_value = last_dispatch.reply_value,
                .raw = raw,
            };
            trace_push(event, budget, due, result);
            return result;
        }

        [[nodiscard]] std::size_t active_sessions() const noexcept
        {
            return dispatcher_ != nullptr ? dispatcher_->active_sessions() : 0u;
        }

        [[nodiscard]] const session_slot_type* session(
            std::size_t index) const noexcept
        {
            return dispatcher_ != nullptr ? dispatcher_->session(index) : nullptr;
        }

        [[nodiscard]] TaskMessageSessionSlotLookup lookup_session(
            util::u64 session_handle) const noexcept
        {
            return dispatcher_ != nullptr
                       ? dispatcher_->lookup_session(session_handle)
                       : TaskMessageSessionSlotLookup{};
        }

        [[nodiscard]] std::size_t active_channels() const noexcept
        {
            return acceptor_ != nullptr ? acceptor_->active_channels() : 0u;
        }

        [[nodiscard]] const channel_slot_type* channel(
            std::size_t index) const noexcept
        {
            return acceptor_ != nullptr ? acceptor_->channel(index) : nullptr;
        }

        [[nodiscard]] TaskMessageSessionChannelLookup lookup_channel(
            util::u64 session_handle) const noexcept
        {
            return acceptor_ != nullptr
                       ? acceptor_->lookup_channel(session_handle)
                       : TaskMessageSessionChannelLookup{};
        }

    private:
        void trace_push(const Event& event,
                        std::size_t budget,
                        tick_type due,
                        const result_type& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = ++sequence_,
                .reason = result.reason,
                .event_id = event.id,
                .event_value = payload_u64(event),
                .due = static_cast<util::u64>(due),
                .budget = budget,
                .served = result.raw.drain.served,
                .active_sessions = result.active_sessions,
                .active_channels = result.active_channels,
                .progressed = result.progressed,
                .bootstrap_consumed = result.bootstrap_consumed,
                .wait_armed = result.wait_armed,
                .hold_ready = result.hold_ready,
                .dispatch_accepted = result.dispatch_accepted,
                .dispatch_handled = result.dispatch_handled,
                .dispatch_replied = result.dispatch_replied,
                .reply_value = result.reply_value,
            });
        }

        Pump pump_{};
        SessionDispatcher* dispatcher_{nullptr};
        SessionAcceptor* acceptor_{nullptr};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Pump, typename SessionDispatcher, typename SessionAcceptor>
    [[nodiscard]] auto make_task_message_session_service(
        Pump pump,
        SessionDispatcher& dispatcher,
        SessionAcceptor& acceptor) noexcept
        -> TaskMessageSessionService<Pump, SessionDispatcher, SessionAcceptor>
    {
        return TaskMessageSessionService<Pump,
                                         SessionDispatcher,
                                         SessionAcceptor>{
            pump,
            dispatcher,
            acceptor,
        };
    }

    template <typename Pump,
              typename SessionDispatcher,
              typename SessionAcceptor,
              typename TraceBuffer>
    [[nodiscard]] auto make_task_message_session_service(
        Pump pump,
        SessionDispatcher& dispatcher,
        SessionAcceptor& acceptor,
        TraceBuffer* trace) noexcept
        -> TaskMessageSessionService<Pump,
                                     SessionDispatcher,
                                     SessionAcceptor,
                                     TraceBuffer>
    {
        return TaskMessageSessionService<Pump,
                                         SessionDispatcher,
                                         SessionAcceptor,
                                         TraceBuffer>{
            pump,
            dispatcher,
            acceptor,
            trace,
        };
    }
}
