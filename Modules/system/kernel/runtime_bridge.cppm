module;

#include <cstddef>

export module kernel.runtime_bridge;

import kernel.eda;
import kernel.evt;
export import kernel.runtime_glue;

export namespace kernel {
    template <typename Scheduler,
              typename TraceBuffer =
                  RuntimeTraceBuffer<typename Scheduler::TimeSource::Tick, 1>>
    class RuntimeBridge {
    public:
        using scheduler_type = Scheduler;
        using tick_type = typename Scheduler::TimeSource::Tick;
        using trace_type = TraceBuffer;

        RuntimeBridge(Scheduler& scheduler,
                      TaskId idle_task,
                      Event idle_event = make_event(EventId::user0),
                      TraceBuffer* trace = nullptr) noexcept
            : scheduler_(&scheduler), idle_task_(idle_task), idle_event_(idle_event),
              trace_(trace)
        {
        }

        [[nodiscard]] Scheduler& scheduler() noexcept
        {
            return *scheduler_;
        }

        [[nodiscard]] const Scheduler& scheduler() const noexcept
        {
            return *scheduler_;
        }

        [[nodiscard]] TaskId idle_task() const noexcept
        {
            return idle_task_;
        }

        [[nodiscard]] Event idle_event() const noexcept
        {
            return idle_event_;
        }

        [[nodiscard]] TraceBuffer* trace() noexcept
        {
            return trace_;
        }

        [[nodiscard]] const TraceBuffer* trace() const noexcept
        {
            return trace_;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void bind_idle(TaskId idle_task,
                       Event idle_event = make_event(EventId::user0)) noexcept
        {
            idle_task_ = idle_task;
            idle_event_ = idle_event;
        }

        [[nodiscard]] std::size_t advance_tick(tick_type now) noexcept
        {
            return runtime_advance_tick(*scheduler_, now, trace_);
        }

        [[nodiscard]] bool defer_from_isr(TaskId task, Event event) noexcept
        {
            return runtime_defer_from_isr(*scheduler_, task, event, trace_);
        }

        [[nodiscard]] bool bootstrap_idle() noexcept
        {
            return runtime_bootstrap_idle(*scheduler_, idle_task_, idle_event_, trace_);
        }

        [[nodiscard]] bool bootstrap_idle(Event event) noexcept
        {
            return runtime_bootstrap_idle(*scheduler_, idle_task_, event, trace_);
        }

        [[nodiscard]] bool bootstrap_worker(
            TaskId task,
            Event event = make_event(EventId::user0)) noexcept
        {
            return runtime_bootstrap_worker(*scheduler_, task, event, trace_);
        }

        [[nodiscard]] bool yield_current(
            Event event = make_event(EventId::user0)) noexcept
        {
            return runtime_yield_current(*scheduler_, event, trace_);
        }

        [[nodiscard]] bool sleep_current_until(
            tick_type due,
            Event event = make_event(EventId::tick)) noexcept
        {
            return runtime_sleep_current_until(*scheduler_, due, event, trace_);
        }

        [[nodiscard]] bool run_once_or_idle(tick_type now) noexcept
        {
            return runtime_run_once_or_idle(
                *scheduler_,
                now,
                idle_task_,
                idle_event_,
                trace_);
        }

    private:
        Scheduler* scheduler_{nullptr};
        TaskId idle_task_{};
        Event idle_event_{make_event(EventId::user0)};
        TraceBuffer* trace_{nullptr};
    };

    template <typename Tick>
    struct RuntimeLoopPort {
        void* self{nullptr};
        std::size_t (*advance_tick_fn)(void* self, Tick now) noexcept {nullptr};
        bool (*defer_from_isr_fn)(void* self, TaskId task, Event event) noexcept {
            nullptr
        };
        bool (*bootstrap_idle_default_fn)(void* self) noexcept {nullptr};
        bool (*bootstrap_idle_event_fn)(void* self, Event event) noexcept {
            nullptr
        };
        bool (*bootstrap_worker_fn)(void* self,
                                    TaskId task,
                                    Event event) noexcept {nullptr};
        bool (*run_once_or_idle_fn)(void* self, Tick now) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return self != nullptr && advance_tick_fn != nullptr &&
                   defer_from_isr_fn != nullptr &&
                   bootstrap_idle_default_fn != nullptr &&
                   bootstrap_idle_event_fn != nullptr &&
                   bootstrap_worker_fn != nullptr &&
                   run_once_or_idle_fn != nullptr;
        }

        [[nodiscard]] std::size_t advance_tick(Tick now) const noexcept
        {
            return advance_tick_fn != nullptr ? advance_tick_fn(self, now) : 0u;
        }

        [[nodiscard]] bool defer_from_isr(TaskId task, Event event) const noexcept
        {
            return defer_from_isr_fn != nullptr &&
                   defer_from_isr_fn(self, task, event);
        }

        [[nodiscard]] bool bootstrap_idle() const noexcept
        {
            return bootstrap_idle_default_fn != nullptr &&
                   bootstrap_idle_default_fn(self);
        }

        [[nodiscard]] bool bootstrap_idle(Event event) const noexcept
        {
            return bootstrap_idle_event_fn != nullptr &&
                   bootstrap_idle_event_fn(self, event);
        }

        [[nodiscard]] bool bootstrap_worker(
            TaskId task,
            Event event = make_event(EventId::user0)) const noexcept
        {
            return bootstrap_worker_fn != nullptr &&
                   bootstrap_worker_fn(self, task, event);
        }

        [[nodiscard]] bool run_once_or_idle(Tick now) const noexcept
        {
            return run_once_or_idle_fn != nullptr &&
                   run_once_or_idle_fn(self, now);
        }
    };

    template <typename Tick>
    struct RuntimeThreadPort {
        void* self{nullptr};
        bool (*yield_current_fn)(void* self, Event event) noexcept {nullptr};
        bool (*sleep_current_until_fn)(void* self,
                                       Tick due,
                                       Event event) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return self != nullptr && yield_current_fn != nullptr &&
                   sleep_current_until_fn != nullptr;
        }

        [[nodiscard]] bool yield_current(
            Event event = make_event(EventId::user0)) const noexcept
        {
            return yield_current_fn != nullptr && yield_current_fn(self, event);
        }

        [[nodiscard]] bool sleep_current_until(
            Tick due,
            Event event = make_event(EventId::tick)) const noexcept
        {
            return sleep_current_until_fn != nullptr &&
                   sleep_current_until_fn(self, due, event);
        }
    };

    namespace detail {
        template <typename Bridge>
        [[nodiscard]] std::size_t runtime_bridge_advance_tick_adapter(
            void* self,
            typename Bridge::tick_type now) noexcept
        {
            return static_cast<Bridge*>(self)->advance_tick(now);
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_defer_from_isr_adapter(
            void* self,
            TaskId task,
            Event event) noexcept
        {
            return static_cast<Bridge*>(self)->defer_from_isr(task, event);
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_bootstrap_idle_default_adapter(
            void* self) noexcept
        {
            return static_cast<Bridge*>(self)->bootstrap_idle();
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_bootstrap_idle_event_adapter(
            void* self,
            Event event) noexcept
        {
            return static_cast<Bridge*>(self)->bootstrap_idle(event);
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_bootstrap_worker_adapter(
            void* self,
            TaskId task,
            Event event) noexcept
        {
            return static_cast<Bridge*>(self)->bootstrap_worker(task, event);
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_run_once_or_idle_adapter(
            void* self,
            typename Bridge::tick_type now) noexcept
        {
            return static_cast<Bridge*>(self)->run_once_or_idle(now);
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_yield_current_adapter(
            void* self,
            Event event) noexcept
        {
            return static_cast<Bridge*>(self)->yield_current(event);
        }

        template <typename Bridge>
        [[nodiscard]] bool runtime_bridge_sleep_current_until_adapter(
            void* self,
            typename Bridge::tick_type due,
            Event event) noexcept
        {
            return static_cast<Bridge*>(self)->sleep_current_until(due, event);
        }
    }

    template <typename Bridge>
    [[nodiscard]] auto make_runtime_loop_port(Bridge& bridge) noexcept
        -> RuntimeLoopPort<typename Bridge::tick_type>
    {
        return RuntimeLoopPort<typename Bridge::tick_type>{
            .self = &bridge,
            .advance_tick_fn = &detail::runtime_bridge_advance_tick_adapter<Bridge>,
            .defer_from_isr_fn =
                &detail::runtime_bridge_defer_from_isr_adapter<Bridge>,
            .bootstrap_idle_default_fn =
                &detail::runtime_bridge_bootstrap_idle_default_adapter<Bridge>,
            .bootstrap_idle_event_fn =
                &detail::runtime_bridge_bootstrap_idle_event_adapter<Bridge>,
            .bootstrap_worker_fn =
                &detail::runtime_bridge_bootstrap_worker_adapter<Bridge>,
            .run_once_or_idle_fn =
                &detail::runtime_bridge_run_once_or_idle_adapter<Bridge>,
        };
    }

    template <typename Bridge>
    [[nodiscard]] auto make_runtime_thread_port(Bridge& bridge) noexcept
        -> RuntimeThreadPort<typename Bridge::tick_type>
    {
        return RuntimeThreadPort<typename Bridge::tick_type>{
            .self = &bridge,
            .yield_current_fn =
                &detail::runtime_bridge_yield_current_adapter<Bridge>,
            .sleep_current_until_fn =
                &detail::runtime_bridge_sleep_current_until_adapter<Bridge>,
        };
    }
}
