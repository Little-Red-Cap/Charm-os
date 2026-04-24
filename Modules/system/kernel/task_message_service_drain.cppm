module;

#include <array>
#include <cstddef>

export module kernel.task_message_service_drain;

export import kernel.task_message_service_loop;
import util.core;

export namespace kernel {
    enum class TaskMessageServiceDrainTraceKind : util::u8 {
        timeout = 0,
        dispatch,
        stop,
    };

    [[nodiscard]] constexpr const char* task_message_service_drain_trace_kind_name(
        TaskMessageServiceDrainTraceKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageServiceDrainTraceKind::timeout:
            return "timeout";
        case TaskMessageServiceDrainTraceKind::dispatch:
            return "dispatch";
        case TaskMessageServiceDrainTraceKind::stop:
            return "stop";
        }
        return "unknown";
    }

    enum class TaskMessageServiceDrainStopReason : util::u8 {
        none = 0,
        timeout,
        queue_empty,
        budget_reached,
    };

    [[nodiscard]] constexpr const char* task_message_service_drain_stop_reason_name(
        TaskMessageServiceDrainStopReason reason) noexcept
    {
        switch (reason) {
        case TaskMessageServiceDrainStopReason::none:
            return "none";
        case TaskMessageServiceDrainStopReason::timeout:
            return "timeout";
        case TaskMessageServiceDrainStopReason::queue_empty:
            return "queue-empty";
        case TaskMessageServiceDrainStopReason::budget_reached:
            return "budget-reached";
        }
        return "unknown";
    }

    struct TaskMessageServiceDrainResult {
        bool progressed{false};
        bool wait_armed{false};
        bool timeout_consumed{false};
        std::size_t served{0};
        TaskMessageServiceDrainStopReason stop_reason{
            TaskMessageServiceDrainStopReason::none};
        TaskMessageDispatchResult last_dispatch{};
    };

    struct TaskMessageServiceDrainTraceEvent {
        util::u64 sequence{0};
        TaskMessageServiceDrainTraceKind kind{
            TaskMessageServiceDrainTraceKind::timeout};
        TaskMessageServiceDrainStopReason stop_reason{
            TaskMessageServiceDrainStopReason::none};
        std::size_t served{0};
        std::size_t budget{0};
        bool ok{false};
        bool matched{false};
        bool handler_valid{false};
        bool handled{false};
        bool replied{false};
        RuntimeMailboxRequest request{};
        bool request_valid{false};
        util::u64 reply_value{0};
    };

    template <std::size_t Capacity>
    class TaskMessageServiceDrainTraceBuffer {
    public:
        using value_type = TaskMessageServiceDrainTraceEvent;

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

    template <typename ServiceLoop,
              typename TraceBuffer = TaskMessageServiceDrainTraceBuffer<1>>
    class TaskMessageServiceDrain {
    public:
        using service_loop_type = ServiceLoop;
        using dispatcher_type = typename ServiceLoop::dispatcher_type;
        using result_type = TaskMessageServiceDrainResult;
        using trace_type = TraceBuffer;
        using tick_type = typename ServiceLoop::tick_type;

        constexpr TaskMessageServiceDrain() noexcept = default;

        constexpr explicit TaskMessageServiceDrain(
            ServiceLoop service_loop,
            TraceBuffer* trace = nullptr) noexcept
            : service_loop_(service_loop), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return service_loop_.valid();
        }

        [[nodiscard]] ServiceLoop& service_loop() noexcept
        {
            return service_loop_;
        }

        [[nodiscard]] const ServiceLoop& service_loop() const noexcept
        {
            return service_loop_;
        }

        void bind_service_loop(ServiceLoop service_loop) noexcept
        {
            service_loop_ = service_loop;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] Event receive_event() const noexcept
        {
            return service_loop_.receive_event();
        }

        [[nodiscard]] Event receive_timeout_event() const noexcept
        {
            return service_loop_.receive_timeout_event();
        }

        [[nodiscard]] bool wait_receive_until(tick_type due) noexcept
        {
            return service_loop_.wait_receive_until(due);
        }

        [[nodiscard]] result_type step(Event event, std::size_t budget) noexcept
        {
            if (!valid() || budget == 0u) {
                return result_type{};
            }

            auto base = service_loop_.step(event);
            if (base.timeout_consumed) {
                trace_timeout(budget);
                return result_type{
                    .progressed = true,
                    .timeout_consumed = true,
                    .stop_reason = TaskMessageServiceDrainStopReason::timeout,
                    .last_dispatch = base.dispatch,
                };
            }

            auto result = result_type{
                .progressed = base.progressed,
                .wait_armed = base.wait_armed,
                .timeout_consumed = base.timeout_consumed,
                .last_dispatch = base.dispatch,
            };

            if (!base.dispatch.accepted) {
                if (event_matches(event, receive_event())) {
                    result.stop_reason =
                        TaskMessageServiceDrainStopReason::queue_empty;
                    trace_stop(result.stop_reason, result.served, budget);
                }
                return result;
            }

            result.progressed = true;
            result.served = 1u;
            result.last_dispatch = base.dispatch;
            trace_dispatch(base.dispatch, result.served, budget);

            while (result.served < budget) {
                const auto dispatch = service_loop_.dispatcher().serve_once();
                if (!dispatch.accepted) {
                    result.stop_reason =
                        TaskMessageServiceDrainStopReason::queue_empty;
                    trace_stop(result.stop_reason, result.served, budget);
                    return result;
                }

                ++result.served;
                result.last_dispatch = dispatch;
                trace_dispatch(dispatch, result.served, budget);
            }

            result.stop_reason = TaskMessageServiceDrainStopReason::budget_reached;
            trace_stop(result.stop_reason, result.served, budget);
            return result;
        }

        [[nodiscard]] result_type step_and_wait_until(Event event,
                                                      std::size_t budget,
                                                      tick_type due) noexcept
        {
            auto result = step(event, budget);
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

        void trace_timeout(std::size_t budget) noexcept
        {
            trace_push(TaskMessageServiceDrainTraceEvent{
                .kind = TaskMessageServiceDrainTraceKind::timeout,
                .stop_reason = TaskMessageServiceDrainStopReason::timeout,
                .budget = budget,
                .ok = true,
            });
        }

        void trace_dispatch(const TaskMessageDispatchResult& dispatch,
                            std::size_t served,
                            std::size_t budget) noexcept
        {
            trace_push(TaskMessageServiceDrainTraceEvent{
                .kind = TaskMessageServiceDrainTraceKind::dispatch,
                .served = served,
                .budget = budget,
                .ok = dispatch.accepted,
                .matched = dispatch.matched,
                .handler_valid = dispatch.handler_valid,
                .handled = dispatch.handled,
                .replied = dispatch.replied,
                .request = dispatch.request,
                .request_valid = dispatch.accepted,
                .reply_value = dispatch.reply_value,
            });
        }

        void trace_stop(TaskMessageServiceDrainStopReason stop_reason,
                        std::size_t served,
                        std::size_t budget) noexcept
        {
            trace_push(TaskMessageServiceDrainTraceEvent{
                .kind = TaskMessageServiceDrainTraceKind::stop,
                .stop_reason = stop_reason,
                .served = served,
                .budget = budget,
                .ok = stop_reason != TaskMessageServiceDrainStopReason::none,
            });
        }

        void trace_push(const TaskMessageServiceDrainTraceEvent& event) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            auto traced = event;
            traced.sequence = ++sequence_;
            (void)trace_->push(traced);
        }

        ServiceLoop service_loop_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename ServiceLoop>
    [[nodiscard]] auto make_task_message_service_drain(
        ServiceLoop service_loop) noexcept
        -> TaskMessageServiceDrain<ServiceLoop>
    {
        return TaskMessageServiceDrain<ServiceLoop>{service_loop};
    }

    template <typename ServiceLoop, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_service_drain(
        ServiceLoop service_loop,
        TraceBuffer* trace) noexcept
        -> TaskMessageServiceDrain<ServiceLoop, TraceBuffer>
    {
        return TaskMessageServiceDrain<ServiceLoop, TraceBuffer>{
            service_loop,
            trace,
        };
    }
}
