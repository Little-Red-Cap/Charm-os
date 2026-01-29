module;

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

export module kernel.scheduler;

import kernel.capabilities;
import kernel.config;
import kernel.context;
import kernel.eda;
import kernel.evt;
import kernel.event_queue;
import kernel.event_queue_list;
import kernel.event_token;
import kernel.task_state;
import kernel.timer;
import util.core;

export namespace kernel {
    template <typename Config, typename Registry>
    using SchedulerQueueStorage = std::conditional_t<
        Config::enable_dynamic_priority,
        EventQueueList<Config::evtq_capacity, Registry::count, Config::priority_levels>,
        std::array<EventQueue<Config::evtq_capacity>, Config::priority_levels>>;

    template <typename Config, typename Registry, typename Caps, typename State>
    class Scheduler;

    template <typename Config, typename Registry, typename Caps>
    class Scheduler<Config, Registry, Caps, state::Created> {
    public:
        Scheduler(Registry& registry, Caps& caps) : registry_(&registry), caps_(&caps) {
            validate_config<Config>();
            static_assert(Capabilities<Caps>);
        }

    private:
        Registry* registry_{nullptr};
        Caps* caps_{nullptr};
        SchedulerQueueStorage<Config, Registry> queues_{};

        friend class Scheduler<Config, Registry, Caps, state::Running>;
    };

    template <typename Config, typename Registry, typename Caps>
    class Scheduler<Config, Registry, Caps, state::Running> {
    public:
        using TimeSource = typename Caps::TimeSource;
        Scheduler(Scheduler<Config, Registry, Caps, state::Created>&& created)
            : registry_(created.registry_), caps_(created.caps_), queues_(std::move(created.queues_)) {
            task_enabled_.fill(true);
            registry_->init_all();
            post_init_events();
        }

        [[nodiscard]] EventToken post_token(TaskId task, Event evt) noexcept {
            if (task.value >= Registry::count) {
                return EventToken{0};
            }
            if (!task_enabled_[task.value]) {
                return EventToken{0};
            }
            const auto prio_index = current_priorities_[task.value];
            auto guard = Caps::IrqGuard::enter();
            const auto tag = ++evt_seq_;
            bool ok = false;
            if constexpr (Config::enable_dynamic_priority) {
                ok = queues_.push(task, evt, tag, prio_index);
            } else {
                ok = queues_[prio_index].push(EventNode{task, evt, tag});
            }
            Caps::IrqGuard::leave(guard);
            if (ok) {
                Caps::SwiTrigger::trigger(prio_index);
                Caps::Wakeup::signal();
            }
            return ok ? EventToken{tag} : EventToken{0};
        }

        [[nodiscard]] bool post(TaskId task, Event evt) noexcept {
            return post_token(task, evt).value != 0;
        }

        [[nodiscard]] bool set_priority(TaskId task, Priority prio) noexcept {
            if constexpr (!Config::enable_dynamic_priority) {
                (void)task;
                (void)prio;
                return false;
            } else {
                if (task.value >= Registry::count) {
                    return false;
                }
                if (prio.value >= Config::priority_levels) {
                    return false;
                }
                current_priorities_[task.value] = static_cast<std::size_t>(prio.value);
                if constexpr (Config::enable_dynamic_priority) {
                    (void)queues_.update_ready_priority(task, static_cast<std::size_t>(prio.value));
                }
                return true;
            }
        }

        [[nodiscard]] bool enable_task(TaskId task) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            task_enabled_[task.value] = true;
            return true;
        }

        [[nodiscard]] bool disable_task(TaskId task) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            task_enabled_[task.value] = false;
            if constexpr (Config::enable_dynamic_priority) {
                (void)queues_.drop_task(task);
            } else {
                for (std::size_t i = 0; i < Config::priority_levels; ++i) {
                    (void)queues_[i].drop_task(task);
                }
            }
            return true;
        }

        [[nodiscard]] bool stop_task(TaskId task) noexcept {
            if (!disable_task(task)) {
                return false;
            }
            registry_->stop(task);
            return true;
        }

        [[nodiscard]] bool restart_task(TaskId task) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            task_enabled_[task.value] = true;
            registry_->start(task);
            return post(task, make_event(EventId::init));
        }

        [[nodiscard]] bool run_once() noexcept {
            for (std::size_t i = Config::priority_levels; i-- > 0;) {
                std::optional<EventNode> node{};
                if constexpr (Config::enable_dynamic_priority) {
                    node = queues_.pop(i);
                } else {
                    node = queues_[i].pop();
                }
                if (node.has_value()) {
                    if (!task_enabled_[node->task.value]) {
                        return true;
                    }
                    set_current(node->task, node->event);
                    registry_->dispatch(node->task, node->event);
                    clear_current();
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool cancel_event(util::u64 tag) noexcept {
            bool removed = false;
            if constexpr (Config::enable_dynamic_priority) {
                removed = queues_.cancel(tag);
            } else {
                for (std::size_t i = 0; i < Config::priority_levels; ++i) {
                    removed = queues_[i].cancel(tag) || removed;
                }
            }
            if constexpr (Config::enable_timer) {
                removed = timers_.cancel(tag) || removed;
            }
            return removed;
        }

        [[nodiscard]] bool cancel_event(EventToken token) noexcept {
            return cancel_event(token.value);
        }

        [[nodiscard]] bool tick(typename Caps::TimeSource::Tick now) noexcept {
            if constexpr (!Config::enable_timer) {
                (void)now;
                return false;
            } else {
                auto entry = timers_.pop_due(now);
                if (!entry.has_value()) {
                    return false;
                }
                (void)post_token(entry->task, entry->event);
                return true;
            }
        }

        [[nodiscard]] EventToken schedule_at_token(
            typename Caps::TimeSource::Tick due,
            TaskId task,
            Event evt) noexcept {
            if constexpr (!Config::enable_timer) {
                (void)due;
                (void)task;
                (void)evt;
                return EventToken{0};
            } else {
                const auto order = ++timer_seq_;
                const auto tag = order;
                const TimerEntry<typename Caps::TimeSource::Tick> entry{due, task, evt, order, tag};
                (void)timers_.schedule(entry);
                return EventToken{tag};
            }
        }

        [[nodiscard]] bool schedule_at(
            typename Caps::TimeSource::Tick due,
            TaskId task,
            Event evt) noexcept {
            return schedule_at_token(due, task, evt).value != 0;
        }

    private:
        Registry* registry_{nullptr};
        Caps* caps_{nullptr};
        SchedulerQueueStorage<Config, Registry> queues_{};
        using Tick = typename Caps::TimeSource::Tick;
        using TimerPolicy = typename TimerPolicySelector<Config>::type;
        using TimerStorage = std::conditional_t<
            Config::enable_timer,
            TimerQueue<Tick, Config::timer_capacity, TimerPolicy>,
            NoopTimerQueue<Tick>>;
        TimerStorage timers_{};
        util::u64 timer_seq_{0};
        util::u64 evt_seq_{0};
        static constexpr auto priority_table_ = Registry::template priority_table<Config>();
        std::array<std::size_t, Registry::count> current_priorities_{priority_table_};
        std::array<bool, Registry::count> task_enabled_{};

        void post_init_events() noexcept {
            for (std::size_t i = 0; i < Registry::count; ++i) {
                (void)post(TaskId{i}, make_event(EventId::init));
            }
        }
    };

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] Scheduler<Config, Registry, Caps, state::Created> make_scheduler(Registry& registry, Caps& caps) {
        return Scheduler<Config, Registry, Caps, state::Created>{registry, caps};
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] Scheduler<Config, Registry, Caps, state::Running> start(
        Scheduler<Config, Registry, Caps, state::Created>&& created) {
        return Scheduler<Config, Registry, Caps, state::Running>{std::move(created)};
    }
}
