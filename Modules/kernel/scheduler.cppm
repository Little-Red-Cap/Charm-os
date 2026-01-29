module;

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

export module kernel.scheduler;

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.evt_queue;
import kernel.task_state;
import kernel.timer;
import util.core;

export namespace kernel {
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
        std::array<EventQueue<Config::evtq_capacity>, Config::priority_levels> queues_{};

        friend class Scheduler<Config, Registry, Caps, state::Running>;
    };

    template <typename Config, typename Registry, typename Caps>
    class Scheduler<Config, Registry, Caps, state::Running> {
    public:
        using TimeSource = typename Caps::TimeSource;
        Scheduler(Scheduler<Config, Registry, Caps, state::Created>&& created)
            : registry_(created.registry_), caps_(created.caps_), queues_(std::move(created.queues_)) {
            post_init_events();
        }

        [[nodiscard]] bool post(TaskId task, Event evt) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            const auto prio_index = current_priorities_[task.value];
            auto guard = Caps::IrqGuard::enter();
            const bool ok = queues_[prio_index].push(EventNode{task, evt});
            Caps::IrqGuard::leave(guard);
            if (ok) {
                Caps::SwiTrigger::trigger(prio_index);
                Caps::Wakeup::signal();
            }
            return ok;
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
                return true;
            }
        }

        [[nodiscard]] bool run_once() noexcept {
            for (std::size_t i = Config::priority_levels; i-- > 0;) {
                auto node = queues_[i].pop();
                if (node.has_value()) {
                    registry_->dispatch(node->task, node->event);
                    return true;
                }
            }
            return false;
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
                (void)post(entry->task, entry->event);
                return true;
            }
        }

        [[nodiscard]] bool schedule_at(
            typename Caps::TimeSource::Tick due,
            TaskId task,
            Event evt) noexcept {
            if constexpr (!Config::enable_timer) {
                (void)due;
                (void)task;
                (void)evt;
                return false;
            } else {
                const TimerEntry<typename Caps::TimeSource::Tick> entry{due, task, evt, timer_seq_++};
                return timers_.schedule(entry);
            }
        }

    private:
        Registry* registry_{nullptr};
        Caps* caps_{nullptr};
        std::array<EventQueue<Config::evtq_capacity>, Config::priority_levels> queues_{};
        using Tick = typename Caps::TimeSource::Tick;
        using TimerPolicy = typename TimerPolicySelector<Config>::type;
        using TimerStorage = std::conditional_t<
            Config::enable_timer,
            TimerQueue<Tick, Config::timer_capacity, TimerPolicy>,
            NoopTimerQueue<Tick>>;
        TimerStorage timers_{};
        util::u64 timer_seq_{0};
        static constexpr auto priority_table_ = Registry::template priority_table<Config>();
        std::array<std::size_t, Registry::count> current_priorities_{priority_table_};

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
