module;

#include <array>
#include <cstddef>
#include <cstdint>

export module kernel.runtime_glue;

import kernel.context;
import kernel.eda;
import kernel.evt;
import util.core;

export namespace kernel {
    enum class RuntimeTraceKind : util::u8 {
        tick = 0,
        isr_defer,
        idle_bootstrap,
        worker_bootstrap,
        yield,
        sleep,
    };

    [[nodiscard]] constexpr const char* runtime_trace_kind_name(
        RuntimeTraceKind kind) noexcept
    {
        switch (kind) {
        case RuntimeTraceKind::tick:
            return "tick";
        case RuntimeTraceKind::isr_defer:
            return "isr-defer";
        case RuntimeTraceKind::idle_bootstrap:
            return "idle-bootstrap";
        case RuntimeTraceKind::worker_bootstrap:
            return "worker-bootstrap";
        case RuntimeTraceKind::yield:
            return "yield";
        case RuntimeTraceKind::sleep:
            return "sleep";
        }
        return "unknown";
    }

    template <typename Tick>
    struct RuntimeTraceEvent {
        Tick stamp{};
        RuntimeTraceKind kind{RuntimeTraceKind::tick};
        TaskId task{};
        bool task_valid{false};
        EventId event_id{EventId::init};
        util::u64 value{0};
        bool ok{false};
    };

    template <typename Tick, std::size_t Capacity>
    class RuntimeTraceBuffer {
    public:
        using tick_type = Tick;
        using value_type = RuntimeTraceEvent<Tick>;

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

    template <typename TraceBuffer>
    inline void runtime_trace_push(TraceBuffer* trace,
                                   typename TraceBuffer::tick_type stamp,
                                   RuntimeTraceKind kind,
                                   TaskId task,
                                   bool task_valid,
                                   Event event,
                                   util::u64 value,
                                   bool ok) noexcept
    {
        if (trace == nullptr) {
            return;
        }

        (void)trace->push(typename TraceBuffer::value_type{
            .stamp = stamp,
            .kind = kind,
            .task = task,
            .task_valid = task_valid,
            .event_id = event.id,
            .value = value,
            .ok = ok,
        });
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] std::size_t runtime_advance_tick(
        Scheduler& scheduler,
        typename Scheduler::TimeSource::Tick now,
        TraceBuffer* trace = nullptr) noexcept
    {
        std::size_t count = 0;
        while (scheduler.tick(now)) {
            ++count;
        }

        if (count != 0) {
            runtime_trace_push(trace,
                               now,
                               RuntimeTraceKind::tick,
                               TaskId{},
                               false,
                               make_event(EventId::tick),
                               static_cast<util::u64>(count),
                               true);
        }

        return count;
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] bool runtime_defer_from_isr(
        Scheduler& scheduler,
        TaskId task,
        Event event,
        TraceBuffer* trace = nullptr) noexcept
    {
        const auto now = Scheduler::TimeSource::now();
        const auto ok = scheduler.post_demand(task, event);
        runtime_trace_push(trace,
                           now,
                           RuntimeTraceKind::isr_defer,
                           task,
                           true,
                           event,
                           payload_u64(event),
                           ok);
        return ok;
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] bool runtime_bootstrap_idle(
        Scheduler& scheduler,
        TaskId task,
        Event event = make_event(EventId::user0),
        TraceBuffer* trace = nullptr) noexcept
    {
        const auto now = Scheduler::TimeSource::now();
        const auto ok = scheduler.post(task, event);
        runtime_trace_push(trace,
                           now,
                           RuntimeTraceKind::idle_bootstrap,
                           task,
                           true,
                           event,
                           payload_u64(event),
                           ok);
        return ok;
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] bool runtime_bootstrap_worker(
        Scheduler& scheduler,
        TaskId task,
        Event event = make_event(EventId::user0),
        TraceBuffer* trace = nullptr) noexcept
    {
        const auto now = Scheduler::TimeSource::now();
        const auto ok = scheduler.post(task, event);
        runtime_trace_push(trace,
                           now,
                           RuntimeTraceKind::worker_bootstrap,
                           task,
                           true,
                           event,
                           payload_u64(event),
                           ok);
        return ok;
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] bool runtime_yield_current(
        Scheduler& scheduler,
        Event event = make_event(EventId::user0),
        TraceBuffer* trace = nullptr) noexcept
    {
        const auto now = Scheduler::TimeSource::now();
        if (!has_current()) {
            runtime_trace_push(trace,
                               now,
                               RuntimeTraceKind::yield,
                               TaskId{},
                               false,
                               event,
                               payload_u64(event),
                               false);
            return false;
        }

        const auto task = current_task();
        const auto ok = scheduler.post(task, event);
        runtime_trace_push(trace,
                           now,
                           RuntimeTraceKind::yield,
                           task,
                           true,
                           event,
                           payload_u64(event),
                           ok);
        return ok;
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] bool runtime_sleep_current_until(
        Scheduler& scheduler,
        typename Scheduler::TimeSource::Tick due,
        Event event = make_event(EventId::tick),
        TraceBuffer* trace = nullptr) noexcept
    {
        const auto now = Scheduler::TimeSource::now();
        if (!has_current()) {
            runtime_trace_push(trace,
                               now,
                               RuntimeTraceKind::sleep,
                               TaskId{},
                               false,
                               event,
                               static_cast<util::u64>(due),
                               false);
            return false;
        }

        const auto task = current_task();
        const auto ok = scheduler.schedule_at(due, task, event);
        runtime_trace_push(trace,
                           now,
                           RuntimeTraceKind::sleep,
                           task,
                           true,
                           event,
                           static_cast<util::u64>(due),
                           ok);
        return ok;
    }

    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    [[nodiscard]] bool runtime_run_once_or_idle(
        Scheduler& scheduler,
        typename Scheduler::TimeSource::Tick now,
        TaskId idle_task,
        Event idle_event = make_event(EventId::user0),
        TraceBuffer* trace = nullptr) noexcept
    {
        const auto ticked = runtime_advance_tick(scheduler, now, trace);
        if (scheduler.run_once()) {
            return true;
        }
        if (ticked != 0) {
            return true;
        }

        const auto bootstrapped =
            runtime_bootstrap_idle(scheduler, idle_task, idle_event, trace);
        if (!bootstrapped) {
            return false;
        }

        return scheduler.run_once() || bootstrapped;
    }
}
