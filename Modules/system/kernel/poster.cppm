module;

#include <utility>

export module kernel.poster;

import kernel.eda;
import kernel.evt;
import kernel.event_token;

export namespace kernel {
    using PostFn = bool (*)(void* ctx, TaskId task, Event evt) noexcept;

    class PostRef {
    public:
        constexpr PostRef() noexcept = default;

        static constexpr PostRef raw(PostFn fn, void* ctx) noexcept {
            return PostRef{fn, ctx};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return fn_ != nullptr;
        }

        [[nodiscard]] constexpr PostFn fn() const noexcept {
            return fn_;
        }

        [[nodiscard]] constexpr void* ctx() const noexcept {
            return ctx_;
        }

        [[nodiscard]] constexpr bool post(TaskId task, Event evt) const noexcept {
            return fn_ && fn_(ctx_, task, evt);
        }

    private:
        constexpr PostRef(PostFn fn, void* ctx) noexcept
            : fn_(fn),
              ctx_(ctx) {}

        PostFn fn_{nullptr};
        void* ctx_{nullptr};
    };

    template <typename Scheduler>
    struct event_poster {
        Scheduler* scheduler{nullptr};
        TaskId task{};

        constexpr explicit operator bool() const noexcept {
            return scheduler != nullptr;
        }

        [[nodiscard]] constexpr bool post(const Event& evt) const
            noexcept(noexcept(std::declval<Scheduler&>().post(std::declval<TaskId>(),
                                                              std::declval<const Event&>()))) {
            return scheduler && scheduler->post(task, evt);
        }

        [[nodiscard]] constexpr EventToken post_token(const Event& evt) const
            noexcept(noexcept(std::declval<Scheduler&>().post_token(std::declval<TaskId>(),
                                                                    std::declval<const Event&>()))) {
            if (!scheduler) {
                return {};
            }
            return scheduler->post_token(task, evt);
        }
    };

    template <typename Scheduler>
    struct io_ready_poster {
        Scheduler* scheduler{nullptr};
        TaskId task{};

        constexpr explicit operator bool() const noexcept {
            return scheduler != nullptr;
        }

        [[nodiscard]] constexpr bool post(const Event& evt) const
            noexcept(noexcept(std::declval<Scheduler&>().post_io_ready(std::declval<TaskId>(),
                                                                       std::declval<const Event&>()))) {
            return scheduler && scheduler->post_io_ready(task, evt);
        }

        [[nodiscard]] constexpr EventToken post_token(const Event& evt) const
            noexcept(noexcept(std::declval<Scheduler&>().post_io_ready_token(std::declval<TaskId>(),
                                                                             std::declval<const Event&>()))) {
            if (!scheduler) {
                return {};
            }
            return scheduler->post_io_ready_token(task, evt);
        }
    };

    template <typename Scheduler>
    struct demand_poster {
        Scheduler* scheduler{nullptr};
        TaskId task{};

        constexpr explicit operator bool() const noexcept {
            return scheduler != nullptr;
        }

        [[nodiscard]] constexpr bool post(const Event& evt) const
            noexcept(noexcept(std::declval<Scheduler&>().post_demand(std::declval<TaskId>(),
                                                                     std::declval<const Event&>()))) {
            return scheduler && scheduler->post_demand(task, evt);
        }

        [[nodiscard]] constexpr EventToken post_token(const Event& evt) const
            noexcept(noexcept(std::declval<Scheduler&>().post_demand_token(std::declval<TaskId>(),
                                                                           std::declval<const Event&>()))) {
            if (!scheduler) {
                return {};
            }
            return scheduler->post_demand_token(task, evt);
        }
    };

    template <typename Scheduler>
    struct poster_set {
        TaskId task{};
        event_poster<Scheduler> event{};
        io_ready_poster<Scheduler> io_ready{};
        demand_poster<Scheduler> demand{};

        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(event)
                && static_cast<bool>(io_ready)
                && static_cast<bool>(demand);
        }

        [[nodiscard]] constexpr TaskId task_id() const noexcept {
            return task;
        }
    };

    template <typename Scheduler>
    [[nodiscard]] constexpr auto as_event_poster(Scheduler& scheduler, TaskId task) noexcept {
        return event_poster<Scheduler>{&scheduler, task};
    }

    template <typename Scheduler>
    [[nodiscard]] constexpr auto as_io_ready_poster(Scheduler& scheduler, TaskId task) noexcept {
        return io_ready_poster<Scheduler>{&scheduler, task};
    }

    template <typename Scheduler>
    [[nodiscard]] constexpr auto as_demand_poster(Scheduler& scheduler, TaskId task) noexcept {
        return demand_poster<Scheduler>{&scheduler, task};
    }

    template <typename Scheduler>
    [[nodiscard]] constexpr auto make_poster_set(Scheduler& scheduler, TaskId task) noexcept {
        return poster_set<Scheduler>{
            .task = task,
            .event = as_event_poster(scheduler, task),
            .io_ready = as_io_ready_poster(scheduler, task),
            .demand = as_demand_poster(scheduler, task),
        };
    }

    template <typename Scheduler>
    inline bool scheduler_post(void* ctx, TaskId task, Event evt) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->post(task, evt);
    }

    template <typename Scheduler>
    inline bool scheduler_post_io_ready(void* ctx, TaskId task, Event evt) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->post_io_ready(task, evt);
    }

    template <typename Scheduler>
    inline bool scheduler_post_demand(void* ctx, TaskId task, Event evt) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->post_demand(task, evt);
    }
}
