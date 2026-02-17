module;

// Optional/experimental module: list-based event queue (dynamic priority).
#include <array>
#include <cstddef>
#include <optional>

export module kernel.event_queue_list;

import kernel.eda;
import kernel.evt;
import kernel.event_queue;
import util.core;

export namespace kernel {

    struct ReadyLink {
        int prev{-1};
        int next{-1};
        bool in_list{false};
        std::size_t prio{0};
    };

    template <util::usize Capacity, util::usize TaskCount, std::size_t PriorityLevels>
    class EventQueueList {
    public:
        static_assert(Capacity >= 1);
        static_assert(TaskCount >= 1);
        static_assert(PriorityLevels >= 1);

        [[nodiscard]] bool push(TaskId task, Event event, util::u64 tag, std::size_t prio,
            DropPolicy policy = DropPolicy::drop_newest) noexcept {
            if (task.value >= TaskCount || prio >= PriorityLevels) {
                return false;
            }
            const int node = alloc_node();
            if (node == -1) {
                if (policy == DropPolicy::drop_oldest) {
                    const int drop_task = ready_head_[prio];
                    if (drop_task == -1) {
                        return false;
                    }
                    const int drop_node = evt_head_[static_cast<std::size_t>(drop_task)];
                    if (drop_node == -1) {
                        return false;
                    }
                    evt_head_[static_cast<std::size_t>(drop_task)] = next_[static_cast<util::usize>(drop_node)];
                    if (evt_head_[static_cast<std::size_t>(drop_task)] == -1) {
                        evt_tail_[static_cast<std::size_t>(drop_task)] = -1;
                        remove_ready(TaskId{static_cast<std::size_t>(drop_task)});
                    }
                    release_node(drop_node);
                    return push(task, event, tag, prio, policy);
                }
                return false;
            }
            nodes_[static_cast<util::usize>(node)] = EventNode{task, event, tag};
            next_[static_cast<util::usize>(node)] = -1;
            append_event(task, node);
            if (!ready_[task.value].in_list) {
                add_ready(task, prio);
            }
            return true;
        }

        [[nodiscard]] std::optional<EventNode> pop(std::size_t prio) noexcept {
            if (prio >= PriorityLevels) {
                return std::nullopt;
            }
            const int task_idx = ready_head_[prio];
            if (task_idx == -1) {
                return std::nullopt;
            }
            TaskId task{static_cast<std::size_t>(task_idx)};
            remove_ready(task);

            const int node = evt_head_[task.value];
            if (node == -1) {
                return std::nullopt;
            }

            EventNode result = nodes_[static_cast<util::usize>(node)];
            evt_head_[task.value] = next_[static_cast<util::usize>(node)];
            if (evt_head_[task.value] == -1) {
                evt_tail_[task.value] = -1;
            }
            release_node(node);

            if (evt_head_[task.value] != -1) {
                add_ready(task, ready_[task.value].prio);
            }
            return result;
        }

        [[nodiscard]] bool cancel(util::u64 tag) noexcept {
            for (util::usize t = 0; t < TaskCount; ++t) {
                int prev = -1;
                int current = evt_head_[t];
                while (current != -1) {
                    const auto idx = static_cast<util::usize>(current);
                    const auto next = next_[idx];
                    if (nodes_[idx].tag == tag) {
                        if (prev == -1) {
                            evt_head_[t] = next;
                        } else {
                            next_[static_cast<util::usize>(prev)] = next;
                        }
                        if (evt_tail_[t] == current) {
                            evt_tail_[t] = prev;
                        }
                        release_node(current);
                        if (evt_head_[t] == -1) {
                            remove_ready(TaskId{t});
                        }
                        return true;
                    }
                    prev = current;
                    current = next;
                }
            }
            return false;
        }

        [[nodiscard]] bool update_ready_priority(TaskId task, std::size_t new_prio) noexcept {
            if (task.value >= TaskCount || new_prio >= PriorityLevels) {
                return false;
            }
            if (!ready_[task.value].in_list) {
                return false;
            }
            if (ready_[task.value].prio == new_prio) {
                return true;
            }
            remove_ready(task);
            add_ready(task, new_prio);
            return true;
        }

        [[nodiscard]] bool drop_task(TaskId task) noexcept {
            if (task.value >= TaskCount) {
                return false;
            }
            bool removed = false;
            int current = evt_head_[task.value];
            while (current != -1) {
                const auto next = next_[static_cast<util::usize>(current)];
                release_node(current);
                current = next;
                removed = true;
            }
            evt_head_[task.value] = -1;
            evt_tail_[task.value] = -1;
            remove_ready(task);
            return removed;
        }

        [[nodiscard]] bool drop_task_with_tag(TaskId task, util::u64 tag) noexcept {
            if (task.value >= TaskCount) {
                return false;
            }
            bool removed = false;
            int prev = -1;
            int current = evt_head_[task.value];
            while (current != -1) {
                const auto idx = static_cast<util::usize>(current);
                const auto next = next_[idx];
                if (nodes_[idx].tag == tag) {
                    if (prev == -1) {
                        evt_head_[task.value] = next;
                    } else {
                        next_[static_cast<util::usize>(prev)] = next;
                    }
                    if (evt_tail_[task.value] == current) {
                        evt_tail_[task.value] = prev;
                    }
                    release_node(current);
                    removed = true;
                } else {
                    prev = current;
                }
                current = next;
            }
            if (evt_head_[task.value] == -1) {
                remove_ready(task);
            }
            return removed;
        }

        [[nodiscard]] bool coalesce(TaskId task, EventId id, Event evt, util::u64 tag) noexcept {
            if (task.value >= TaskCount) {
                return false;
            }
            bool removed = false;
            int prev = -1;
            int current = evt_head_[task.value];
            while (current != -1) {
                const auto idx = static_cast<util::usize>(current);
                const auto next = next_[idx];
                if (nodes_[idx].event.id == id) {
                    if (prev == -1) {
                        evt_head_[task.value] = next;
                    } else {
                        next_[static_cast<util::usize>(prev)] = next;
                    }
                    if (evt_tail_[task.value] == current) {
                        evt_tail_[task.value] = prev;
                    }
                    release_node(current);
                    removed = true;
                } else {
                    prev = current;
                }
                current = next;
            }
            if (removed) {
                const int node = alloc_node();
                if (node == -1) {
                    return false;
                }
                nodes_[static_cast<util::usize>(node)] = EventNode{task, evt, tag};
                next_[static_cast<util::usize>(node)] = -1;
                append_event(task, node);
                if (!ready_[task.value].in_list) {
                    add_ready(task, ready_[task.value].prio);
                }
                return true;
            }
            return false;
        }

    private:
        std::array<EventNode, Capacity> nodes_{};
        std::array<int, Capacity> next_{};
        std::array<int, Capacity> free_{};
        util::usize free_count_{Capacity};

        std::array<int, TaskCount> evt_head_{};
        std::array<int, TaskCount> evt_tail_{};

        std::array<ReadyLink, TaskCount> ready_{};
        std::array<int, PriorityLevels> ready_head_{};
        std::array<int, PriorityLevels> ready_tail_{};

        constexpr void init_state() noexcept {
            for (util::usize i = 0; i < Capacity; ++i) {
                free_[i] = static_cast<int>(Capacity - 1 - i);
                next_[i] = -1;
            }
            for (util::usize t = 0; t < TaskCount; ++t) {
                evt_head_[t] = -1;
                evt_tail_[t] = -1;
                ready_[t] = ReadyLink{};
            }
            for (util::usize p = 0; p < PriorityLevels; ++p) {
                ready_head_[p] = -1;
                ready_tail_[p] = -1;
            }
        }

        int alloc_node() noexcept {
            if (free_count_ == 0) {
                return -1;
            }
            return free_[--free_count_];
        }

        void release_node(int node) noexcept {
            free_[free_count_++] = node;
        }

        void append_event(TaskId task, int node) noexcept {
            const auto t = task.value;
            if (evt_tail_[t] == -1) {
                evt_head_[t] = node;
                evt_tail_[t] = node;
            } else {
                next_[static_cast<util::usize>(evt_tail_[t])] = node;
                evt_tail_[t] = node;
            }
        }

        void add_ready(TaskId task, std::size_t prio) noexcept {
            const int t = static_cast<int>(task.value);
            ready_[task.value].in_list = true;
            ready_[task.value].prio = prio;
            ready_[task.value].prev = ready_tail_[prio];
            ready_[task.value].next = -1;
            if (ready_tail_[prio] == -1) {
                ready_head_[prio] = t;
                ready_tail_[prio] = t;
            } else {
                ready_[static_cast<util::usize>(ready_tail_[prio])].next = t;
                ready_tail_[prio] = t;
            }
        }

        void remove_ready(TaskId task) noexcept {
            const auto t = task.value;
            if (!ready_[t].in_list) {
                return;
            }
            const auto prio = ready_[t].prio;
            const int prev = ready_[t].prev;
            const int next = ready_[t].next;
            if (prev == -1) {
                ready_head_[prio] = next;
            } else {
                ready_[static_cast<util::usize>(prev)].next = next;
            }
            if (next == -1) {
                ready_tail_[prio] = prev;
            } else {
                ready_[static_cast<util::usize>(next)].prev = prev;
            }
            ready_[t] = ReadyLink{};
        }

    public:
        EventQueueList() noexcept {
            init_state();
        }
    };
}
