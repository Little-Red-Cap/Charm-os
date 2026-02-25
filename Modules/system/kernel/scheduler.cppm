module;

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <cstdio>

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
import kernel.trace;
import util.core;

export namespace kernel {
    template <typename Config, typename Registry>
    using SchedulerQueueStorage = std::conditional_t<
        use_list_queue<Config>,
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
        struct Stats {
            util::u64 posted{0};
            util::u64 dropped{0};
            util::u64 dispatched{0};
            util::u64 filtered{0};
            util::u64 budget_limited{0};
            std::size_t max_queue{0};
            std::size_t max_timer{0};
            std::array<util::u64, Config::priority_levels> posted_prio{};
            std::array<util::u64, Config::priority_levels> dispatched_prio{};
            std::array<util::u64, event_id_count> event_posted{};
            std::array<util::u64, event_id_count> event_dispatched{};
            util::u64 dedup_filtered{0};
            util::u64 debounce_filtered{0};
            util::u64 coalesce_hit{0};
            util::u64 source_post{0};
            util::u64 source_timer{0};
            util::u64 source_replay{0};
            util::u64 idle_rounds{0};
        };
        enum class AlertType : unsigned char {
            queue,
            timer,
            filtered
        };
        enum class AlertLevel : unsigned char {
            warning,
            error
        };
        using AlertHook = void (*)(AlertType, AlertLevel, std::size_t);
        struct PostItem {
            TaskId task{};
            Event event{};
        };
        Scheduler(Scheduler<Config, Registry, Caps, state::Created>&& created)
            : registry_(created.registry_), caps_(created.caps_), queues_(std::move(created.queues_)) {
            task_enabled_.fill(true);
            task_states_.fill(TaskState::ready);
            registry_->template fill_priorities<Config>(current_priorities_);
            registry_->init_all();
            post_init_events();
        }

        [[nodiscard]] EventToken post_token(TaskId task, Event evt) noexcept {
            ++stats_.source_post;
            if (task.value >= Registry::count) {
                ++stats_.dropped;
                return EventToken{0};
            }
            if (!task_enabled_[task.value] && evt.id != EventId::terminate) {
                ++stats_.dropped;
                return EventToken{0};
            }
            if (!registry_->template is_active<Config>(task)) {
                ++stats_.dropped;
                return EventToken{0};
            }
            // Filter chain order: dedup -> debounce -> rate_limit -> coalesce.
            if constexpr (Config::enable_event_dedup) {
                if (evt.id != EventId::terminate) {
                    const auto sig = event_signature(evt);
                    if (event_sig_[task.value] == sig) {
                        ++stats_.filtered;
                        ++stats_.dedup_filtered;
                        return EventToken{0};
                    }
                }
            }
            if constexpr (Config::enable_event_debounce) {
                if (evt.id != EventId::terminate) {
                    const auto idx = static_cast<std::size_t>(evt.id);
                    const auto now = Caps::TimeSource::now();
                    const auto last = event_time_[task.value][idx];
                    if (now >= last && (now - last) <= static_cast<typename Caps::TimeSource::Tick>(Config::debounce_window)) {
                        ++stats_.filtered;
                        ++stats_.debounce_filtered;
                        return EventToken{0};
                    }
                }
            }
            if constexpr (Config::enable_rate_limit) {
                if (evt.id != EventId::terminate) {
                    const auto idx = static_cast<std::size_t>(evt.id);
                    const auto now = Caps::TimeSource::now();
                    const auto last = event_rate_time_[task.value][idx];
                    const auto gap = event_rate_gap_[task.value][idx];
                    if (gap > 0 && now >= last && (now - last) < static_cast<typename Caps::TimeSource::Tick>(gap)) {
                        ++stats_.filtered;
                        return EventToken{0};
                    }
                }
            }
            auto prio_index = current_priorities_[task.value];
            if constexpr (Config::enable_event_boost) {
                if ((event_mask(evt.id) & Config::boost_mask) != 0) {
                    prio_index = Config::priority_levels - 1;
                }
            }
            if constexpr (Config::enable_task_boost) {
                if (task_boost_remaining_[task.value] > 0
                    && (task_boost_mask_[task.value] & event_mask(evt.id)) != 0) {
                    prio_index = task_boost_prio_[task.value];
                    --task_boost_remaining_[task.value];
                }
            }
            auto guard = Caps::IrqGuard::enter();
            const auto tag = ++evt_seq_;
            bool ok = false;
            if constexpr (use_list_queue<Config>) {
                if constexpr (Config::enable_event_coalesce) {
                    ok = queues_.coalesce(task, evt.id, evt, tag);
                    if (!ok) {
                        ok = queues_.push(task, evt, tag, prio_index, drop_policy());
                    } else {
                        ++stats_.coalesce_hit;
                    }
                } else {
                    ok = queues_.push(task, evt, tag, prio_index, drop_policy());
                }
            } else {
                if constexpr (Config::enable_event_coalesce) {
                    ok = queues_[prio_index].coalesce(task, evt.id, evt, tag);
                    if (!ok) {
                        ok = queues_[prio_index].push(EventNode{task, evt, tag}, drop_policy());
                    } else {
                        ++stats_.coalesce_hit;
                    }
                } else {
                    ok = queues_[prio_index].push(EventNode{task, evt, tag}, drop_policy());
                }
            }
            Caps::IrqGuard::leave(guard);
            if (ok) {
                ++stats_.posted;
                ++stats_.event_posted[static_cast<std::size_t>(evt.id)];
                if constexpr (Config::enable_prio_stats) {
                    ++stats_.posted_prio[prio_index];
                }
                if constexpr (Config::wakeup_batch <= 1) {
                    Caps::SwiTrigger::trigger(prio_index);
                    Caps::Wakeup::signal();
                } else {
                    ++wakeup_counter_;
                    if ((wakeup_counter_ % Config::wakeup_batch) == 0) {
                        Caps::SwiTrigger::trigger(prio_index);
                        Caps::Wakeup::signal();
                    }
                }
                if constexpr (use_list_queue<Config>) {
                    const auto depth = queues_.size();
                    if (depth > stats_.max_queue) stats_.max_queue = depth;
                } else {
                    const auto depth = queues_[prio_index].size();
                    if (depth > stats_.max_queue) stats_.max_queue = depth;
                }
                check_alerts();
                if constexpr (Config::enable_event_dedup) {
                    if (evt.id != EventId::terminate) {
                        event_sig_[task.value] = event_signature(evt);
                    }
                }
                if constexpr (Config::enable_event_debounce) {
                    if (evt.id != EventId::terminate) {
                        const auto idx = static_cast<std::size_t>(evt.id);
                        event_time_[task.value][idx] = Caps::TimeSource::now();
                    }
                }
                if constexpr (Config::enable_rate_limit) {
                    if (evt.id != EventId::terminate) {
                        const auto idx = static_cast<std::size_t>(evt.id);
                        event_rate_time_[task.value][idx] = Caps::TimeSource::now();
                    }
                }
            } else {
                ++stats_.dropped;
            }
            return ok ? EventToken{tag} : EventToken{0};
        }

        [[nodiscard]] bool post(TaskId task, Event evt) noexcept {
            return post_token(task, evt).value != 0;
        }

        [[nodiscard]] std::size_t post_many(const PostItem* items, std::size_t count) noexcept {
            std::size_t posted = 0;
            for (std::size_t i = 0; i < count; ++i) {
                if (post(items[i].task, items[i].event)) {
                    ++posted;
                }
            }
            return posted;
        }

        [[nodiscard]] std::size_t post_many_dedup(const PostItem* items, std::size_t count) noexcept {
            std::size_t posted = 0;
            if (count == 0) {
                return posted;
            }
            // Dedup is per (task, event.id) using a fixed bitmap.
            constexpr std::size_t dedup_slots = Registry::count * event_id_count;
            static_assert(dedup_slots > 0);
            std::array<bool, dedup_slots> seen{};
            for (std::size_t i = count; i-- > 0;) {
                const auto task = items[i].task.value;
                if (task >= Registry::count) {
                    if (post(items[i].task, items[i].event)) {
                        ++posted;
                    }
                    continue;
                }
                const auto evt = static_cast<std::size_t>(items[i].event.id);
                const auto idx = task * event_id_count + evt;
                if (seen[idx]) {
                    continue;
                }
                seen[idx] = true;
                if (post(items[i].task, items[i].event)) {
                    ++posted;
                }
            }
            return posted;
        }

        void set_alert_hook(AlertHook hook) noexcept {
            alert_hook_ = hook;
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
            task_states_[task.value] = TaskState::ready;
            return true;
        }

        [[nodiscard]] bool activate_task(TaskId task, Priority prio) noexcept {
            if (task.value >= Registry::count || prio.value >= Config::priority_levels) {
                return false;
            }
            task_enabled_[task.value] = true;
            task_states_[task.value] = TaskState::ready;
            current_priorities_[task.value] = static_cast<std::size_t>(prio.value);
            registry_->start(task);
            return post(task, make_event(EventId::init));
        }

        [[nodiscard]] bool disable_task(TaskId task) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            task_enabled_[task.value] = false;
            task_states_[task.value] = TaskState::stopped;
            if constexpr (use_list_queue<Config>) {
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
            task_states_[task.value] = TaskState::ready;
            registry_->start(task);
            return post(task, make_event(EventId::init));
        }

        [[nodiscard]] bool terminate_task(TaskId task) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            task_enabled_[task.value] = false;
            task_states_[task.value] = TaskState::terminated;
            if constexpr (use_list_queue<Config>) {
                (void)queues_.drop_task(task);
            } else {
                for (std::size_t i = 0; i < Config::priority_levels; ++i) {
                    (void)queues_[i].drop_task(task);
                }
            }
            if constexpr (Config::enable_timer) {
                (void)timers_.cancel_task(task);
            }
            registry_->stop(task);
            return post(task, make_event(EventId::terminate));
        }

        [[nodiscard]] TaskState state_of(TaskId task) const noexcept {
            if (task.value >= Registry::count) {
                return TaskState::stopped;
            }
            return task_states_[task.value];
        }

        [[nodiscard]] bool is_enabled(TaskId task) const noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            return task_enabled_[task.value];
        }

        [[nodiscard]] bool run_once() noexcept {
            return run_budget(1);
        }

        [[nodiscard]] bool run_budget(std::size_t budget) noexcept {
            if (budget == 0) {
                return false;
            }
            for (std::size_t i = 0; i < task_counts_.size(); ++i) {
                task_counts_[i] = 0;
            }
            for (std::size_t i = 0; i < task_event_counts_.size(); ++i) {
                for (std::size_t j = 0; j < event_id_count; ++j) {
                    task_event_counts_[i][j] = 0;
                }
            }
            std::size_t dispatched = 0;
            const std::size_t top = Config::priority_levels - 1;
            std::size_t level = top;
            if (Config::priority_levels > 1 && starve_.stall[top] >= 4) {
                level = top - 1;
            }
            for (std::size_t iter = 0; iter < Config::priority_levels; ++iter) {
                const std::size_t i = (level + Config::priority_levels - iter) % Config::priority_levels;
                std::optional<EventNode> node{};
                if constexpr (use_list_queue<Config>) {
                    node = queues_.pop(i);
                } else {
                    node = queues_[i].pop();
                }
                if (node.has_value()) {
                    if (!task_enabled_[node->task.value] && node->event.id != EventId::terminate) {
                        ++stats_.filtered;
                        continue;
                    }
                    if (!registry_->template is_active<Config>(node->task)) {
                        ++stats_.filtered;
                        continue;
                    }
                    if (task_caps_[node->task.value] != 0 && task_counts_[node->task.value] >= task_caps_[node->task.value]) {
                        ++stats_.filtered;
                        continue;
                    }
                    const auto eid = static_cast<std::size_t>(node->event.id);
                    if (task_event_caps_[node->task.value][eid] != 0
                        && task_event_counts_[node->task.value][eid] >= task_event_caps_[node->task.value][eid]) {
                        ++stats_.filtered;
                        continue;
                    }
                    task_states_[node->task.value] = TaskState::running;
                    set_current(node->task, node->event);
                    registry_->dispatch(node->task, node->event);
                    clear_current();
                    ++stats_.dispatched;
                    ++task_counts_[node->task.value];
                    ++task_event_counts_[node->task.value][eid];
                    ++stats_.event_dispatched[static_cast<std::size_t>(node->event.id)];
                    if constexpr (Config::enable_trace) {
                        const TraceRecord<Tick> rec{
                            Caps::TimeSource::now(),
                            node->task,
                            node->event.id,
                            payload_u64(node->event),
                            1,
                            TraceKind::event
                        };
                        if (!trace_.try_merge_last(rec)) {
                            trace_.push(rec);
                        }
                    }
                    if constexpr (Config::enable_prio_stats) {
                        ++stats_.dispatched_prio[i];
                    }
                    if (i == top) {
                        starve_.stall[top]++;
                    } else {
                        starve_.stall[top] = 0;
                    }
                    if (node->event.id == EventId::terminate) {
                        task_states_[node->task.value] = TaskState::terminated;
                        if constexpr (requires(Registry& r, TaskId t) { r.unregister(t); }) {
                            registry_->unregister(node->task);
                        }
                    } else {
                        task_states_[node->task.value] = TaskState::ready;
                    }
                    if (++dispatched >= budget) {
                        ++stats_.budget_limited;
                        return true;
                    }
                }
            }
            if (dispatched == 0) {
                ++idle_rounds_;
            }
            return false;
        }

        [[nodiscard]] const Stats& stats() const noexcept {
            return stats_;
        }

        [[nodiscard]] std::size_t queue_depth() const noexcept {
            std::size_t total = 0;
            if constexpr (use_list_queue<Config>) {
                return queues_.size();
            } else {
                for (std::size_t i = 0; i < Config::priority_levels; ++i) {
                    total += queues_[i].size();
                }
                return total;
            }
        }

        [[nodiscard]] std::size_t timer_depth() const noexcept {
            if constexpr (Config::enable_timer) {
                return timers_.size();
            } else {
                return 0;
            }
        }

        [[nodiscard]] std::size_t format_snapshot_json(char* out, std::size_t max) const noexcept {
            const auto snap = snapshot();
            return static_cast<std::size_t>(std::snprintf(
                out,
                max,
                "{\"posted\":%llu,\"dropped\":%llu,\"dispatched\":%llu,\"filtered\":%llu,\"budget\":%llu,"
                "\"maxQ\":%llu,\"maxT\":%llu,\"queue\":%llu,\"timers\":%llu,\"active\":%llu,"
                "\"dedup\":%llu,\"debounce\":%llu,\"coalesce\":%llu,\"idle\":%llu}",
                static_cast<unsigned long long>(snap.stats.posted),
                static_cast<unsigned long long>(snap.stats.dropped),
                static_cast<unsigned long long>(snap.stats.dispatched),
                static_cast<unsigned long long>(snap.stats.filtered),
                static_cast<unsigned long long>(snap.stats.budget_limited),
                static_cast<unsigned long long>(snap.stats.max_queue),
                static_cast<unsigned long long>(snap.stats.max_timer),
                static_cast<unsigned long long>(snap.queue_depth),
                static_cast<unsigned long long>(snap.timer_depth),
                static_cast<unsigned long long>(snap.active_tasks),
                static_cast<unsigned long long>(snap.stats.dedup_filtered),
                static_cast<unsigned long long>(snap.stats.debounce_filtered),
                static_cast<unsigned long long>(snap.stats.coalesce_hit),
                static_cast<unsigned long long>(snap.stats.idle_rounds)));
        }

        [[nodiscard]] std::size_t format_event_stats_json(char* out, std::size_t max) const noexcept {
            std::size_t offset = 0;
            offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "["));
            for (std::size_t i = 0; i < event_id_count; ++i) {
                offset += static_cast<std::size_t>(std::snprintf(
                    out + offset,
                    max - offset,
                    "%s{\"id\":%llu,\"posted\":%llu,\"dispatched\":%llu}",
                    i == 0 ? "" : ",",
                    static_cast<unsigned long long>(i),
                    static_cast<unsigned long long>(stats_.event_posted[i]),
                    static_cast<unsigned long long>(stats_.event_dispatched[i])));
            }
            offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "]"));
            return offset;
        }

        [[nodiscard]] std::size_t format_event_source_json(char* out, std::size_t max) const noexcept {
            std::size_t offset = 0;
            offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "{\"post\":%llu,\"timer\":%llu,\"replay\":%llu}",
                static_cast<unsigned long long>(stats_.source_post),
                static_cast<unsigned long long>(stats_.source_timer),
                static_cast<unsigned long long>(stats_.source_replay)));
            return offset;
        }

        [[nodiscard]] std::size_t replay_recent(std::size_t count, EventMask mask = 0xFFFF'FFFFu) noexcept {
            if constexpr (!Config::enable_trace) {
                return 0;
            } else {
                const auto total = trace_.size();
                if (total == 0) {
                    return 0;
                }
                const auto n = count < total ? count : total;
                std::size_t replayed = 0;
                for (std::size_t i = total - n; i < total; ++i) {
                    const auto idx = (trace_.head() + Config::trace_capacity - trace_.size() + i) % Config::trace_capacity;
                    const auto& rec = trace_.data()[idx];
                    if ((mask & event_mask(rec.id)) == 0) {
                        continue;
                    }
                    Event evt{rec.id, rec.payload};
                    ++stats_.source_replay;
                    if (post(rec.task, evt)) {
                        ++replayed;
                    }
                }
                return replayed;
            }
        }

        [[nodiscard]] std::size_t format_tasks_json(char* out, std::size_t max) const noexcept {
            std::size_t offset = 0;
            offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "["));
            auto tasks = task_snapshot();
            for (std::size_t i = 0; i < tasks.size(); ++i) {
                const auto& t = tasks[i];
                offset += static_cast<std::size_t>(std::snprintf(
                    out + offset,
                    max - offset,
                    "%s{\"id\":%llu,\"state\":%u,\"enabled\":%u,\"prio\":%u,\"active\":%u}",
                    i == 0 ? "" : ",",
                    static_cast<unsigned long long>(t.id.value),
                    static_cast<unsigned>(t.state),
                    t.enabled ? 1u : 0u,
                    static_cast<unsigned>(t.priority),
                    t.active ? 1u : 0u));
            }
            offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "]"));
            return offset;
        }

        [[nodiscard]] std::size_t format_snapshot_diff(char* out, std::size_t max) noexcept {
            const auto current = snapshot();
            const auto prev = last_snapshot_;
            last_snapshot_ = current;
            return static_cast<std::size_t>(std::snprintf(
                out,
                max,
                "posted=%lld dropped=%lld dispatched=%lld filtered=%lld budget=%lld",
                static_cast<long long>(current.stats.posted - prev.stats.posted),
                static_cast<long long>(current.stats.dropped - prev.stats.dropped),
                static_cast<long long>(current.stats.dispatched - prev.stats.dispatched),
                static_cast<long long>(current.stats.filtered - prev.stats.filtered),
                static_cast<long long>(current.stats.budget_limited - prev.stats.budget_limited)));
        }

        [[nodiscard]] std::size_t format_trace_json(char* out, std::size_t max) const noexcept {
            if constexpr (!Config::enable_trace) {
                return static_cast<std::size_t>(std::snprintf(out, max, "[]"));
            } else {
                std::size_t offset = 0;
                offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "["));
                const auto& data = trace_.data();
                for (std::size_t i = 0; i < trace_.size(); ++i) {
                    const auto idx = (trace_.head() + Config::trace_capacity - trace_.size() + i) % Config::trace_capacity;
                    const auto& rec = data[idx];
                        offset += static_cast<std::size_t>(std::snprintf(
                        out + offset,
                        max - offset,
                        "%s{\"t\":%llu,\"task\":%llu,\"id\":%u,\"payload\":%llu,\"count\":%u,\"kind\":%u}",
                        i == 0 ? "" : ",",
                        static_cast<unsigned long long>(rec.time),
                        static_cast<unsigned long long>(rec.task.value),
                        static_cast<unsigned>(rec.id),
                        static_cast<unsigned long long>(rec.payload),
                        static_cast<unsigned>(rec.count),
                        static_cast<unsigned>(rec.kind)));
                }
                offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "]"));
                return offset;
            }
        }

        [[nodiscard]] std::size_t format_trace_csv(char* out, std::size_t max) const noexcept {
            std::size_t offset = 0;
            offset += static_cast<std::size_t>(std::snprintf(
                out + offset,
                max - offset,
                "trace_v1,t,task,id,payload,count,kind\n"));
            if constexpr (!Config::enable_trace) {
                return offset;
            } else {
                const auto& data = trace_.data();
                for (std::size_t i = 0; i < trace_.size(); ++i) {
                    const auto idx = (trace_.head() + Config::trace_capacity - trace_.size() + i) % Config::trace_capacity;
                    const auto& rec = data[idx];
                    offset += static_cast<std::size_t>(std::snprintf(
                        out + offset,
                        max - offset,
                        "%llu,%llu,%u,%llu,%u,%u\n",
                        static_cast<unsigned long long>(rec.time),
                        static_cast<unsigned long long>(rec.task.value),
                        static_cast<unsigned>(rec.id),
                        static_cast<unsigned long long>(rec.payload),
                        static_cast<unsigned>(rec.count),
                        static_cast<unsigned>(rec.kind)));
                    if (offset >= max) {
                        break;
                    }
                }
                return offset;
            }
        }

        struct Snapshot {
            Stats stats{};
            std::size_t queue_depth{0};
            std::size_t timer_depth{0};
            std::size_t active_tasks{0};
        };

        [[nodiscard]] Snapshot snapshot() const noexcept {
            Snapshot out{};
            out.stats = stats_;
            out.queue_depth = queue_depth();
            out.timer_depth = timer_depth();
            if constexpr (requires(const Registry& r) { r.active_count(); }) {
                out.active_tasks = registry_->active_count();
            } else {
                out.active_tasks = Registry::count;
            }
            out.stats.idle_rounds = idle_rounds_;
            return out;
        }

        [[nodiscard]] std::size_t format_snapshot(char* out, std::size_t max) const noexcept {
            const auto snap = snapshot();
            return static_cast<std::size_t>(std::snprintf(
                out,
                max,
                "posted=%llu dropped=%llu dispatched=%llu filtered=%llu budget=%llu maxQ=%llu maxT=%llu queue=%llu timers=%llu active=%llu dedup=%llu debounce=%llu coalesce=%llu idle=%llu",
                static_cast<unsigned long long>(snap.stats.posted),
                static_cast<unsigned long long>(snap.stats.dropped),
                static_cast<unsigned long long>(snap.stats.dispatched),
                static_cast<unsigned long long>(snap.stats.filtered),
                static_cast<unsigned long long>(snap.stats.budget_limited),
                static_cast<unsigned long long>(snap.stats.max_queue),
                static_cast<unsigned long long>(snap.stats.max_timer),
                static_cast<unsigned long long>(snap.queue_depth),
                static_cast<unsigned long long>(snap.timer_depth),
                static_cast<unsigned long long>(snap.active_tasks),
                static_cast<unsigned long long>(snap.stats.dedup_filtered),
                static_cast<unsigned long long>(snap.stats.debounce_filtered),
                static_cast<unsigned long long>(snap.stats.coalesce_hit),
                static_cast<unsigned long long>(snap.stats.idle_rounds)));
        }

        struct TaskSnapshot {
            TaskId id{};
            TaskState state{};
            bool enabled{false};
            std::size_t priority{0};
            bool active{false};
        };

        [[nodiscard]] bool set_rate_limit(TaskId task, EventId id, util::u32 gap) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            const auto idx = static_cast<std::size_t>(id);
            event_rate_gap_[task.value][idx] = gap;
            return true;
        }

        [[nodiscard]] std::array<TaskSnapshot, Registry::count> task_snapshot() const noexcept {
            std::array<TaskSnapshot, Registry::count> out{};
            for (std::size_t i = 0; i < Registry::count; ++i) {
                const TaskId id{i};
                out[i] = TaskSnapshot{
                    id,
                    task_states_[i],
                    task_enabled_[i],
                    current_priorities_[i],
                    registry_->template is_active<Config>(id)
                };
            }
            return out;
        }

        [[nodiscard]] bool run_auto() noexcept {
            if constexpr (Config::dispatch_budget == 0) {
                return run_once();
            } else {
                return run_budget(Config::dispatch_budget);
            }
        }

        [[nodiscard]] bool run_idle(typename Caps::TimeSource::Tick now, std::size_t budget) noexcept {
            if (run_budget(budget)) {
                return true;
            }
            if (tick(now)) {
                return true;
            }
            ++idle_rounds_;
            if constexpr (requires { Caps::Wakeup::wait(); }) {
                Caps::Wakeup::wait();
            }
            return false;
        }

        [[nodiscard]] bool run_idle(typename Caps::TimeSource::Tick now) noexcept {
            if constexpr (Config::dispatch_budget == 0) {
                return run_idle(now, 1);
            } else {
                return run_idle(now, Config::dispatch_budget);
            }
        }

        [[nodiscard]] std::size_t dispatch_batch(std::size_t budget) noexcept {
            std::size_t count = 0;
            for (std::size_t i = 0; i < budget; ++i) {
                if (!run_once()) {
                    break;
                }
                ++count;
            }
            return count;
        }

        [[nodiscard]] bool set_task_cap(TaskId task, std::size_t cap) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            task_caps_[task.value] = cap;
            return true;
        }

        [[nodiscard]] bool set_task_event_cap(TaskId task, EventId id, std::size_t cap) noexcept {
            if (task.value >= Registry::count) {
                return false;
            }
            const auto idx = static_cast<std::size_t>(id);
            task_event_caps_[task.value][idx] = cap;
            return true;
        }

        [[nodiscard]] bool set_task_boost(TaskId task, EventMask mask, std::size_t prio, std::size_t count) noexcept {
            if (task.value >= Registry::count || prio >= Config::priority_levels) {
                return false;
            }
            task_boost_mask_[task.value] = mask;
            task_boost_prio_[task.value] = prio;
            task_boost_remaining_[task.value] = count;
            return true;
        }

        [[nodiscard]] bool cancel_event(util::u64 tag) noexcept {
            bool removed = false;
            if constexpr (use_list_queue<Config>) {
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

        [[nodiscard]] bool cancel_event(TaskId task, util::u64 tag) noexcept {
            bool removed = false;
            if constexpr (use_list_queue<Config>) {
                removed = queues_.drop_task_with_tag(task, tag);
            } else {
                for (std::size_t i = 0; i < Config::priority_levels; ++i) {
                    removed = queues_[i].drop_task_with_tag(task, tag) || removed;
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
                ++stats_.source_timer;
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
                if constexpr (Config::enable_timer_merge) {
                    const auto idx = static_cast<std::size_t>(evt.id);
                    const auto old_tag = last_timer_tag_[task.value][idx];
                    if (old_tag != 0) {
                        (void)cancel_event(task, old_tag);
                    }
                }
                const auto order = ++timer_seq_;
                const auto tag = order;
                const TimerEntry<typename Caps::TimeSource::Tick> entry{due, task, evt, order, tag};
                if (!timers_.schedule(entry)) {
                    ++stats_.dropped;
                    if constexpr (Config::enable_alert) {
                        if (alert_hook_) {
                            alert_hook_(AlertType::timer, AlertLevel::error, timers_.size());
                        }
                    }
                    return EventToken{0};
                }
                if constexpr (Config::enable_timer) {
                    const auto depth = timers_.size();
                    if (depth > stats_.max_timer) {
                        stats_.max_timer = depth;
                    }
                }
                if constexpr (Config::enable_timer_merge) {
                    const auto idx = static_cast<std::size_t>(evt.id);
                    last_timer_tag_[task.value][idx] = tag;
                }
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
        std::array<std::size_t, Registry::count> current_priorities_{};
        std::array<bool, Registry::count> task_enabled_{};
        std::array<TaskState, Registry::count> task_states_{};
        Stats stats_{};
        std::array<std::array<util::u64, event_id_count>, Registry::count> last_timer_tag_{};
        std::array<util::u64, Registry::count> event_sig_{};
        util::u64 wakeup_counter_{0};
        std::array<std::array<typename Caps::TimeSource::Tick, event_id_count>, Registry::count> event_time_{};
        std::conditional_t<Config::enable_trace,
            TraceBuffer<typename Caps::TimeSource::Tick, Config::trace_capacity>,
            TraceBuffer<typename Caps::TimeSource::Tick, 1>> trace_{};
        std::array<std::size_t, Registry::count> task_caps_{};
        std::array<std::size_t, Registry::count> task_counts_{};
        std::array<std::array<std::size_t, event_id_count>, Registry::count> task_event_caps_{};
        std::array<std::array<std::size_t, event_id_count>, Registry::count> task_event_counts_{};
        std::array<EventMask, Registry::count> task_boost_mask_{};
        std::array<std::size_t, Registry::count> task_boost_prio_{};
        std::array<std::size_t, Registry::count> task_boost_remaining_{};
        AlertHook alert_hook_{nullptr};
        struct AlertState {
            std::size_t queue_warn{0};
            std::size_t timer_warn{0};
            util::u64 filtered_warn{0};
            std::size_t queue_err{0};
            std::size_t timer_err{0};
            util::u64 filtered_err{0};
        };
        AlertState alert_state_{};
        util::u64 idle_rounds_{0};
        Snapshot last_snapshot_{};
        std::array<std::array<util::u32, event_id_count>, Registry::count> event_rate_gap_{};
        std::array<std::array<typename Caps::TimeSource::Tick, event_id_count>, Registry::count> event_rate_time_{};

        struct StarveState {
            std::array<std::size_t, Config::priority_levels> stall{};
        };
        StarveState starve_{};

        void post_init_events() noexcept {
            for (std::size_t i = 0; i < Registry::count; ++i) {
                const TaskId task{i};
                if (!registry_->template is_active<Config>(task)) {
                    continue;
                }
                (void)post(task, make_event(EventId::init));
            }
        }

        void check_alerts() noexcept {
            if constexpr (!Config::enable_alert) {
                return;
            }
            if (alert_hook_ == nullptr) {
                return;
            }
            if constexpr (Config::alert_queue_warn > 0) {
                if (stats_.max_queue >= Config::alert_queue_warn && stats_.max_queue != alert_state_.queue_warn) {
                    alert_state_.queue_warn = stats_.max_queue;
                    alert_hook_(AlertType::queue, AlertLevel::warning, stats_.max_queue);
                }
            }
            if constexpr (Config::alert_timer_warn > 0) {
                if (stats_.max_timer >= Config::alert_timer_warn && stats_.max_timer != alert_state_.timer_warn) {
                    alert_state_.timer_warn = stats_.max_timer;
                    alert_hook_(AlertType::timer, AlertLevel::warning, stats_.max_timer);
                }
            }
            if constexpr (Config::alert_filtered_warn > 0) {
                if (stats_.filtered >= Config::alert_filtered_warn && stats_.filtered != alert_state_.filtered_warn) {
                    alert_state_.filtered_warn = stats_.filtered;
                    alert_hook_(AlertType::filtered, AlertLevel::warning, static_cast<std::size_t>(stats_.filtered));
                }
            }
            if constexpr (Config::alert_queue_err > 0) {
                if (stats_.max_queue >= Config::alert_queue_err && stats_.max_queue != alert_state_.queue_err) {
                    alert_state_.queue_err = stats_.max_queue;
                    alert_hook_(AlertType::queue, AlertLevel::error, stats_.max_queue);
                }
            }
            if constexpr (Config::alert_timer_err > 0) {
                if (stats_.max_timer >= Config::alert_timer_err && stats_.max_timer != alert_state_.timer_err) {
                    alert_state_.timer_err = stats_.max_timer;
                    alert_hook_(AlertType::timer, AlertLevel::error, stats_.max_timer);
                }
            }
            if constexpr (Config::alert_filtered_err > 0) {
                if (stats_.filtered >= Config::alert_filtered_err && stats_.filtered != alert_state_.filtered_err) {
                    alert_state_.filtered_err = stats_.filtered;
                    alert_hook_(AlertType::filtered, AlertLevel::error, static_cast<std::size_t>(stats_.filtered));
                }
            }
        }

        [[nodiscard]] DropPolicy drop_policy() const noexcept {
            if constexpr (Config::drop_oldest) {
                return DropPolicy::drop_oldest;
            } else {
                return DropPolicy::drop_newest;
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
