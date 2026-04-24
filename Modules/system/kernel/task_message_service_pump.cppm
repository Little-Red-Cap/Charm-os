module;

#include <array>
#include <cstddef>

export module kernel.task_message_service_pump;

export import kernel.task_message_service_drain;
import util.core;

export namespace kernel {
    enum class TaskMessageServicePumpTraceKind : util::u8 {
        bootstrap = 0,
        rearm,
        hold,
    };

    [[nodiscard]] constexpr const char* task_message_service_pump_trace_kind_name(
        TaskMessageServicePumpTraceKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageServicePumpTraceKind::bootstrap:
            return "bootstrap";
        case TaskMessageServicePumpTraceKind::rearm:
            return "rearm";
        case TaskMessageServicePumpTraceKind::hold:
            return "hold";
        }
        return "unknown";
    }

    enum class TaskMessageServicePumpReason : util::u8 {
        none = 0,
        bootstrap,
        timeout,
        queue_empty,
        budget_reached,
    };

    [[nodiscard]] constexpr const char* task_message_service_pump_reason_name(
        TaskMessageServicePumpReason reason) noexcept
    {
        switch (reason) {
        case TaskMessageServicePumpReason::none:
            return "none";
        case TaskMessageServicePumpReason::bootstrap:
            return "bootstrap";
        case TaskMessageServicePumpReason::timeout:
            return "timeout";
        case TaskMessageServicePumpReason::queue_empty:
            return "queue-empty";
        case TaskMessageServicePumpReason::budget_reached:
            return "budget-reached";
        }
        return "unknown";
    }

    struct TaskMessageServicePumpResult {
        bool progressed{false};
        bool bootstrap_consumed{false};
        bool wait_armed{false};
        bool hold_ready{false};
        TaskMessageServicePumpReason reason{
            TaskMessageServicePumpReason::none};
        TaskMessageServiceDrainResult drain{};
    };

    struct TaskMessageServicePumpTraceEvent {
        util::u64 sequence{0};
        TaskMessageServicePumpTraceKind kind{
            TaskMessageServicePumpTraceKind::bootstrap};
        TaskMessageServicePumpReason reason{
            TaskMessageServicePumpReason::none};
        EventId event_id{EventId::init};
        util::u64 value{0};
        util::u64 due{0};
        std::size_t budget{0};
        std::size_t served{0};
        bool ok{false};
    };

    template <std::size_t Capacity>
    class TaskMessageServicePumpTraceBuffer {
    public:
        using value_type = TaskMessageServicePumpTraceEvent;

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

    template <typename ServiceDrain,
              typename TraceBuffer = TaskMessageServicePumpTraceBuffer<1>>
    class TaskMessageServicePump {
    public:
        using service_drain_type = ServiceDrain;
        using result_type = TaskMessageServicePumpResult;
        using trace_type = TraceBuffer;
        using tick_type = typename ServiceDrain::tick_type;

        constexpr TaskMessageServicePump() noexcept = default;

        constexpr explicit TaskMessageServicePump(
            ServiceDrain service_drain,
            Event bootstrap_event = make_event(EventId::user0),
            TraceBuffer* trace = nullptr) noexcept
            : service_drain_(service_drain), bootstrap_event_(bootstrap_event),
              trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return service_drain_.valid();
        }

        [[nodiscard]] ServiceDrain& service_drain() noexcept
        {
            return service_drain_;
        }

        [[nodiscard]] const ServiceDrain& service_drain() const noexcept
        {
            return service_drain_;
        }

        void bind_service_drain(ServiceDrain service_drain) noexcept
        {
            service_drain_ = service_drain;
        }

        [[nodiscard]] Event bootstrap_event() const noexcept
        {
            return bootstrap_event_;
        }

        void bind_bootstrap_event(Event event) noexcept
        {
            bootstrap_event_ = event;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] Event receive_event() const noexcept
        {
            return service_drain_.receive_event();
        }

        [[nodiscard]] Event receive_timeout_event() const noexcept
        {
            return service_drain_.receive_timeout_event();
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            return service_drain_.wait_receive_until(due);
        }

        [[nodiscard]] result_type step(Event event,
                                       std::size_t budget,
                                       tick_type due) noexcept
        {
            if (!valid()) {
                return result_type{};
            }

            if (event_matches(event, bootstrap_event_)) {
                const auto armed = wait_receive_until(due);
                trace_bootstrap(event, budget, due, armed);
                return result_type{
                    .progressed = armed,
                    .bootstrap_consumed = true,
                    .wait_armed = armed,
                    .reason = TaskMessageServicePumpReason::bootstrap,
                };
            }

            if (budget == 0u) {
                return result_type{};
            }

            const auto drained = service_drain_.step(event, budget);
            auto result = result_type{
                .progressed = drained.progressed,
                .reason = TaskMessageServicePumpReason::none,
                .drain = drained,
            };

            if (drained.timeout_consumed) {
                result.reason = TaskMessageServicePumpReason::timeout;
                result.wait_armed = wait_receive_until(due);
                result.progressed = true;
                trace_rearm(
                    event, result.reason, drained.served, budget, due, result.wait_armed);
                return result;
            }

            if (drained.stop_reason ==
                TaskMessageServiceDrainStopReason::queue_empty) {
                result.reason = TaskMessageServicePumpReason::queue_empty;
                result.wait_armed = wait_receive_until(due);
                result.progressed = drained.progressed || result.wait_armed;
                trace_rearm(
                    event, result.reason, drained.served, budget, due, result.wait_armed);
                return result;
            }

            if (drained.stop_reason ==
                TaskMessageServiceDrainStopReason::budget_reached) {
                result.reason = TaskMessageServicePumpReason::budget_reached;
                result.hold_ready = true;
                trace_hold(
                    event, result.reason, drained.served, budget, due, true);
                return result;
            }

            return result;
        }

    private:
        [[nodiscard]] static constexpr bool event_matches(Event lhs,
                                                          Event rhs) noexcept
        {
            return lhs.id == rhs.id && payload_u64(lhs) == payload_u64(rhs);
        }

        void trace_bootstrap(Event event,
                             std::size_t budget,
                             tick_type due,
                             bool ok) noexcept
        {
            trace_push(TaskMessageServicePumpTraceEvent{
                .kind = TaskMessageServicePumpTraceKind::bootstrap,
                .reason = TaskMessageServicePumpReason::bootstrap,
                .event_id = event.id,
                .value = payload_u64(event),
                .due = static_cast<util::u64>(due),
                .budget = budget,
                .served = 0u,
                .ok = ok,
            });
        }

        void trace_rearm(Event event,
                         TaskMessageServicePumpReason reason,
                         std::size_t served,
                         std::size_t budget,
                         tick_type due,
                         bool ok) noexcept
        {
            trace_push(TaskMessageServicePumpTraceEvent{
                .kind = TaskMessageServicePumpTraceKind::rearm,
                .reason = reason,
                .event_id = event.id,
                .value = payload_u64(event),
                .due = static_cast<util::u64>(due),
                .budget = budget,
                .served = served,
                .ok = ok,
            });
        }

        void trace_hold(Event event,
                        TaskMessageServicePumpReason reason,
                        std::size_t served,
                        std::size_t budget,
                        tick_type due,
                        bool ok) noexcept
        {
            trace_push(TaskMessageServicePumpTraceEvent{
                .kind = TaskMessageServicePumpTraceKind::hold,
                .reason = reason,
                .event_id = event.id,
                .value = payload_u64(event),
                .due = static_cast<util::u64>(due),
                .budget = budget,
                .served = served,
                .ok = ok,
            });
        }

        void trace_push(const TaskMessageServicePumpTraceEvent& event) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            auto traced = event;
            traced.sequence = ++sequence_;
            (void)trace_->push(traced);
        }

        ServiceDrain service_drain_{};
        Event bootstrap_event_{make_event(EventId::user0)};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename ServiceDrain>
    [[nodiscard]] auto make_task_message_service_pump(
        ServiceDrain service_drain,
        Event bootstrap_event = make_event(EventId::user0)) noexcept
        -> TaskMessageServicePump<ServiceDrain>
    {
        return TaskMessageServicePump<ServiceDrain>{
            service_drain,
            bootstrap_event,
        };
    }

    template <typename ServiceDrain, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_service_pump(
        ServiceDrain service_drain,
        Event bootstrap_event,
        TraceBuffer* trace) noexcept
        -> TaskMessageServicePump<ServiceDrain, TraceBuffer>
    {
        return TaskMessageServicePump<ServiceDrain, TraceBuffer>{
            service_drain,
            bootstrap_event,
            trace,
        };
    }
}
