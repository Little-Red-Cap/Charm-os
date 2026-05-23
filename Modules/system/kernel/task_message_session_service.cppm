module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_service;

export import kernel.task_message_service_pump;
export import kernel.task_message_session_acceptor;
import semantic.core;
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

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionServiceTraceEvent>(
            std::array<std::string_view, 17>{
                "sequence",
                "reason",
                "event_id",
                "event_value",
                "due",
                "budget",
                "served",
                "active_sessions",
                "active_channels",
                "progressed",
                "bootstrap_consumed",
                "wait_armed",
                "hold_ready",
                "dispatch_accepted",
                "dispatch_handled",
                "dispatch_replied",
                "reply_value",
            }));

    struct TaskMessageSessionServiceWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
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

        [[nodiscard]] constexpr bool bootstrap_branch_ok() const noexcept
        {
            return reason == TaskMessageServicePumpReason::bootstrap &&
                   progressed && bootstrap_consumed && served == 0u &&
                   !dispatch_accepted && !dispatch_handled &&
                   !dispatch_replied;
        }

        [[nodiscard]] constexpr bool dispatch_branch_ok() const noexcept
        {
            return (reason == TaskMessageServicePumpReason::queue_empty ||
                    reason == TaskMessageServicePumpReason::budget_reached ||
                    reason == TaskMessageServicePumpReason::timeout) &&
                   progressed && !bootstrap_consumed && served > 0u &&
                   dispatch_accepted;
        }

        [[nodiscard]] constexpr bool idle_branch_ok() const noexcept
        {
            return reason == TaskMessageServicePumpReason::none &&
                   !progressed && !bootstrap_consumed && served == 0u &&
                   !dispatch_accepted && !dispatch_handled &&
                   !dispatch_replied;
        }

        [[nodiscard]] constexpr bool flow_flags_ok() const noexcept
        {
            if (bootstrap_consumed && dispatch_accepted) {
                return false;
            }

            if (dispatch_replied && !dispatch_handled) {
                return false;
            }

            if (dispatch_handled && !dispatch_accepted) {
                return false;
            }

            if (hold_ready && wait_armed) {
                return false;
            }

            if (!progressed &&
                (bootstrap_consumed || dispatch_accepted ||
                 dispatch_handled || dispatch_replied || served != 0u)) {
                return false;
            }

            return true;
        }

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return verdict() == semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr semantic::Result result() const noexcept
        {
            return verdict() == semantic::Verdict::standing
                       ? semantic::Result::ok
                       : semantic::Result::failed;
        }

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (!flow_flags_ok()) {
                return semantic::Verdict::drifted;
            }

            if (bootstrap_branch_ok() || dispatch_branch_ok() ||
                idle_branch_ok()) {
                return semantic::Verdict::standing;
            }

            return semantic::Verdict::drifted;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            if (!ready) {
                return semantic::FailureDomain::input;
            }

            if (verdict() == semantic::Verdict::standing) {
                return semantic::FailureDomain::none;
            }

            if (!flow_flags_ok()) {
                return semantic::FailureDomain::handoff;
            }

            if (reason == TaskMessageServicePumpReason::none) {
                return semantic::FailureDomain::selection;
            }

            return semantic::FailureDomain::route;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-service-witness.summary";
        }
    };

    struct TaskMessageSessionServiceWitnessHandoffTarget {
        const TaskMessageSessionServiceWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-service-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-service-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionServiceWitness>(
            std::array<std::string_view, 19>{
                "sequence",
                "ready",
                "has_trace",
                "reason",
                "event_id",
                "event_value",
                "due",
                "budget",
                "served",
                "active_sessions",
                "active_channels",
                "progressed",
                "bootstrap_consumed",
                "wait_armed",
                "hold_ready",
                "dispatch_accepted",
                "dispatch_handled",
                "dispatch_replied",
                "reply_value",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSessionServiceWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSessionServiceWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionServiceWitness
    task_message_session_service_witness(
        const TaskMessageSessionServiceTraceEvent& event) noexcept
    {
        return TaskMessageSessionServiceWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .reason = event.reason,
            .event_id = event.event_id,
            .event_value = event.event_value,
            .due = event.due,
            .budget = event.budget,
            .served = event.served,
            .active_sessions = event.active_sessions,
            .active_channels = event.active_channels,
            .progressed = event.progressed,
            .bootstrap_consumed = event.bootstrap_consumed,
            .wait_armed = event.wait_armed,
            .hold_ready = event.hold_ready,
            .dispatch_accepted = event.dispatch_accepted,
            .dispatch_handled = event.dispatch_handled,
            .dispatch_replied = event.dispatch_replied,
            .reply_value = event.reply_value,
        };
    }

    [[nodiscard]] constexpr bool task_message_session_service_witness_ready(
        const TaskMessageSessionServiceWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionServiceWitnessHandoffTarget
    task_message_session_service_witness_handoff_target(
        const TaskMessageSessionServiceWitness& witness) noexcept
    {
        return TaskMessageSessionServiceWitnessHandoffTarget{
            .witness = &witness,
        };
    }

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

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSessionServiceWitness
    task_message_session_service_witness(
        const TaskMessageSessionServiceTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSessionServiceWitness{};
        }

        return task_message_session_service_witness(*terminal);
    }

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

    template <typename RawPumpResult>
    [[nodiscard]] constexpr TaskMessageSessionServiceWitness
    task_message_session_service_witness(
        const TaskMessageSessionServiceResult<RawPumpResult>& result) noexcept
    {
        return TaskMessageSessionServiceWitness{
            .ready = true,
            .reason = result.reason,
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
        };
    }

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
