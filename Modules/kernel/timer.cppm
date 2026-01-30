module;

#include <array>
#include <cstddef>
#include <concepts>
#include <optional>

export module kernel.timer;

import kernel.evt;
import kernel.eda;
import kernel.event_token;
import util.core;

export namespace kernel {
    template <typename Tick>
    struct TimerEntry {
        Tick due{};
        TaskId task{};
        Event event{};
        util::u64 order{0};
        util::u64 tag{0};
    };

    template <typename Policy, typename Tick, util::usize Capacity>
    concept TimerPolicy = requires(
        typename Policy::template Storage<Tick, Capacity> storage,
        TimerEntry<Tick> entry,
        Tick now,
        util::u64 tag,
        TaskId task) {
        { Policy::template insert<Tick, Capacity>(storage, entry) } -> std::same_as<bool>;
        { Policy::template pop_due<Tick, Capacity>(storage, now) } -> std::same_as<std::optional<TimerEntry<Tick>>>;
        { Policy::template size<Tick, Capacity>(storage) } -> std::same_as<util::usize>;
        { Policy::template cancel<Tick, Capacity>(storage, tag) } -> std::same_as<bool>;
        { Policy::template cancel_task<Tick, Capacity>(storage, task) } -> std::same_as<bool>;
    };

    struct LinearTimerPolicy {
        template <typename Tick, util::usize Capacity>
        struct Storage {
            std::array<TimerEntry<Tick>, Capacity> entries{};
            util::usize count{0};
        };

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool insert(Storage<Tick, Capacity>& storage, TimerEntry<Tick> entry) noexcept {
            if (storage.count >= Capacity) {
                return false;
            }
            storage.entries[storage.count++] = entry;
            return true;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static std::optional<TimerEntry<Tick>> pop_due(Storage<Tick, Capacity>& storage, Tick now) noexcept {
            if (storage.count == 0) {
                return std::nullopt;
            }

            util::usize best_index = Capacity;
            Tick best_due{};
            util::u64 best_order{};
            for (util::usize i = 0; i < storage.count; ++i) {
                const auto due = storage.entries[i].due;
                if (due <= now) {
                    const auto order = storage.entries[i].order;
                    if (best_index == Capacity || due < best_due
                        || (due == best_due && order < best_order)) {
                        best_index = i;
                        best_due = due;
                        best_order = order;
                    }
                }
            }

            if (best_index == Capacity) {
                return std::nullopt;
            }

            TimerEntry<Tick> result = storage.entries[best_index];
            storage.entries[best_index] = storage.entries[storage.count - 1];
            --storage.count;
            return result;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static util::usize size(const Storage<Tick, Capacity>& storage) noexcept {
            return storage.count;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool cancel(Storage<Tick, Capacity>& storage, util::u64 tag) noexcept {
            for (util::usize i = 0; i < storage.count; ++i) {
                if (storage.entries[i].tag == tag) {
                    storage.entries[i] = storage.entries[storage.count - 1];
                    --storage.count;
                    return true;
                }
            }
            return false;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool cancel_task(Storage<Tick, Capacity>& storage, TaskId task) noexcept {
            bool removed = false;
            for (util::usize i = 0; i < storage.count;) {
                if (storage.entries[i].task.value == task.value) {
                    storage.entries[i] = storage.entries[storage.count - 1];
                    --storage.count;
                    removed = true;
                    continue;
                }
                ++i;
            }
            return removed;
        }
    };

    struct HeapTimerPolicy {
        template <typename Tick, util::usize Capacity>
        struct Storage {
            std::array<TimerEntry<Tick>, Capacity> entries{};
            util::usize count{0};
        };

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool insert(Storage<Tick, Capacity>& storage, TimerEntry<Tick> entry) noexcept {
            if (storage.count >= Capacity) {
                return false;
            }
            util::usize idx = storage.count++;
            storage.entries[idx] = entry;
            while (idx > 0) {
                const util::usize parent = (idx - 1) / 2;
                const auto& parent_entry = storage.entries[parent];
                const auto& current_entry = storage.entries[idx];
                if (parent_entry.due < current_entry.due
                    || (parent_entry.due == current_entry.due
                        && parent_entry.order <= current_entry.order)) {
                    break;
                }
                auto tmp = storage.entries[parent];
                storage.entries[parent] = storage.entries[idx];
                storage.entries[idx] = tmp;
                idx = parent;
            }
            return true;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static std::optional<TimerEntry<Tick>> pop_due(Storage<Tick, Capacity>& storage, Tick now) noexcept {
            if (storage.count == 0) {
                return std::nullopt;
            }
            if (storage.entries[0].due > now) {
                return std::nullopt;
            }

            TimerEntry<Tick> result = storage.entries[0];
            storage.entries[0] = storage.entries[storage.count - 1];
            --storage.count;

            util::usize idx = 0;
            while (true) {
                const util::usize left = idx * 2 + 1;
                const util::usize right = left + 1;
                if (left >= storage.count) {
                    break;
                }
                util::usize smallest = left;
                if (right < storage.count) {
                    const auto& left_entry = storage.entries[left];
                    const auto& right_entry = storage.entries[right];
                    if (right_entry.due < left_entry.due
                        || (right_entry.due == left_entry.due
                            && right_entry.order < left_entry.order)) {
                        smallest = right;
                    }
                }
                const auto& current = storage.entries[idx];
                const auto& next = storage.entries[smallest];
                if (current.due < next.due || (current.due == next.due && current.order <= next.order)) {
                    break;
                }
                auto tmp = storage.entries[idx];
                storage.entries[idx] = storage.entries[smallest];
                storage.entries[smallest] = tmp;
                idx = smallest;
            }

            return result;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static util::usize size(const Storage<Tick, Capacity>& storage) noexcept {
            return storage.count;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool cancel(Storage<Tick, Capacity>& storage, util::u64 tag) noexcept {
            auto less = [](const TimerEntry<Tick>& a, const TimerEntry<Tick>& b) {
                return a.due < b.due || (a.due == b.due && a.order < b.order);
            };
            util::usize idx = storage.count;
            for (util::usize i = 0; i < storage.count; ++i) {
                if (storage.entries[i].tag == tag) {
                    idx = i;
                    break;
                }
            }
            if (idx == storage.count) {
                return false;
            }
            storage.entries[idx] = storage.entries[storage.count - 1];
            --storage.count;
            if (idx >= storage.count) {
                return true;
            }

            while (idx > 0) {
                const util::usize parent = (idx - 1) / 2;
                if (!less(storage.entries[idx], storage.entries[parent])) {
                    break;
                }
                auto tmp = storage.entries[parent];
                storage.entries[parent] = storage.entries[idx];
                storage.entries[idx] = tmp;
                idx = parent;
            }

            while (true) {
                const util::usize left = idx * 2 + 1;
                const util::usize right = left + 1;
                if (left >= storage.count) {
                    break;
                }
                util::usize smallest = left;
                if (right < storage.count && less(storage.entries[right], storage.entries[left])) {
                    smallest = right;
                }
                if (!less(storage.entries[smallest], storage.entries[idx])) {
                    break;
                }
                auto tmp = storage.entries[idx];
                storage.entries[idx] = storage.entries[smallest];
                storage.entries[smallest] = tmp;
                idx = smallest;
            }

            return true;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool cancel_task(Storage<Tick, Capacity>& storage, TaskId task) noexcept {
            auto less = [](const TimerEntry<Tick>& a, const TimerEntry<Tick>& b) {
                return a.due < b.due || (a.due == b.due && a.order < b.order);
            };
            bool removed = false;
            for (util::usize idx = 0; idx < storage.count;) {
                if (storage.entries[idx].task.value != task.value) {
                    ++idx;
                    continue;
                }
                storage.entries[idx] = storage.entries[storage.count - 1];
                --storage.count;
                removed = true;
                if (idx >= storage.count) {
                    continue;
                }

                util::usize fix = idx;
                while (fix > 0) {
                    const util::usize parent = (fix - 1) / 2;
                    if (!less(storage.entries[fix], storage.entries[parent])) {
                        break;
                    }
                    auto tmp = storage.entries[parent];
                    storage.entries[parent] = storage.entries[fix];
                    storage.entries[fix] = tmp;
                    fix = parent;
                }
                while (true) {
                    const util::usize left = fix * 2 + 1;
                    const util::usize right = left + 1;
                    if (left >= storage.count) {
                        break;
                    }
                    util::usize smallest = left;
                    if (right < storage.count && less(storage.entries[right], storage.entries[left])) {
                        smallest = right;
                    }
                    if (!less(storage.entries[smallest], storage.entries[fix])) {
                        break;
                    }
                    auto tmp = storage.entries[fix];
                    storage.entries[fix] = storage.entries[smallest];
                    storage.entries[smallest] = tmp;
                    fix = smallest;
                }
            }
            return removed;
        }
    };

    struct WheelTimerPolicy {
        template <typename Tick, util::usize Capacity>
        struct Storage {
            std::array<TimerEntry<Tick>, Capacity> entries{};
            std::array<int, Capacity> head{};
            std::array<int, Capacity> next{};
            std::array<int, Capacity> free{};
            util::usize free_count{0};
            util::usize count{0};

            Storage() noexcept {
                for (util::usize i = 0; i < Capacity; ++i) {
                    head[i] = -1;
                    next[i] = -1;
                    free[i] = static_cast<int>(Capacity - 1 - i);
                }
                free_count = Capacity;
            }
        };

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool insert(Storage<Tick, Capacity>& storage, TimerEntry<Tick> entry) noexcept {
            if (storage.free_count == 0) {
                return false;
            }
            const auto slot = static_cast<util::usize>(entry.due % Capacity);
            const int idx = storage.free[--storage.free_count];
            storage.entries[idx] = entry;
            storage.next[idx] = storage.head[slot];
            storage.head[slot] = idx;
            ++storage.count;
            return true;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static std::optional<TimerEntry<Tick>> pop_due(
            Storage<Tick, Capacity>& storage,
            Tick now) noexcept {
            if (storage.count == 0) {
                return std::nullopt;
            }
            const auto slot = static_cast<util::usize>(now % Capacity);
            int current = storage.head[slot];
            int prev = -1;
            int best = -1;
            int best_prev = -1;
            Tick best_due{};
            util::u64 best_order{};

            while (current != -1) {
                const auto& entry = storage.entries[static_cast<util::usize>(current)];
                if (entry.due <= now) {
                    if (best == -1 || entry.due < best_due
                        || (entry.due == best_due && entry.order < best_order)) {
                        best = current;
                        best_prev = prev;
                        best_due = entry.due;
                        best_order = entry.order;
                    }
                }
                prev = current;
                current = storage.next[static_cast<util::usize>(current)];
            }

            if (best == -1) {
                return std::nullopt;
            }

            const int best_next = storage.next[static_cast<util::usize>(best)];
            if (best_prev == -1) {
                storage.head[slot] = best_next;
            } else {
                storage.next[static_cast<util::usize>(best_prev)] = best_next;
            }

            TimerEntry<Tick> result = storage.entries[static_cast<util::usize>(best)];
            storage.free[storage.free_count++] = best;
            --storage.count;
            return result;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static util::usize size(const Storage<Tick, Capacity>& storage) noexcept {
            return storage.count;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool cancel(Storage<Tick, Capacity>& storage, util::u64 tag) noexcept {
            if (storage.count == 0) {
                return false;
            }
            for (util::usize slot = 0; slot < Capacity; ++slot) {
                int current = storage.head[slot];
                int prev = -1;
                while (current != -1) {
                    const auto idx = static_cast<util::usize>(current);
                    if (storage.entries[idx].tag == tag) {
                        const int next = storage.next[idx];
                        if (prev == -1) {
                            storage.head[slot] = next;
                        } else {
                            storage.next[static_cast<util::usize>(prev)] = next;
                        }
                        storage.free[storage.free_count++] = current;
                        --storage.count;
                        return true;
                    }
                    prev = current;
                    current = storage.next[idx];
                }
            }
            return false;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool cancel_task(Storage<Tick, Capacity>& storage, TaskId task) noexcept {
            if (storage.count == 0) {
                return false;
            }
            bool removed = false;
            for (util::usize slot = 0; slot < Capacity; ++slot) {
                int current = storage.head[slot];
                int prev = -1;
                while (current != -1) {
                    const auto idx = static_cast<util::usize>(current);
                    const auto next = storage.next[idx];
                    if (storage.entries[idx].task.value == task.value) {
                        if (prev == -1) {
                            storage.head[slot] = next;
                        } else {
                            storage.next[static_cast<util::usize>(prev)] = next;
                        }
                        storage.free[storage.free_count++] = current;
                        --storage.count;
                        removed = true;
                    } else {
                        prev = current;
                    }
                    current = next;
                }
            }
            return removed;
        }
    };

    template <typename Config>
    struct TimerPolicySelector {
        using type = LinearTimerPolicy;
    };

    template <typename Config>
        requires requires { typename Config::timer_policy; }
    struct TimerPolicySelector<Config> {
        using type = typename Config::timer_policy;
    };

    template <typename Tick, util::usize Capacity, typename Policy = LinearTimerPolicy>
    class TimerQueue {
    public:
        static_assert(Capacity >= 1);
        static_assert(TimerPolicy<Policy, Tick, Capacity>);

        [[nodiscard]] bool schedule(TimerEntry<Tick> entry) noexcept {
            return Policy::template insert<Tick, Capacity>(storage_, entry);
        }

        [[nodiscard]] std::optional<TimerEntry<Tick>> pop_due(Tick now) noexcept {
            return Policy::template pop_due<Tick, Capacity>(storage_, now);
        }

        [[nodiscard]] bool cancel(util::u64 tag) noexcept {
            return Policy::template cancel<Tick, Capacity>(storage_, tag);
        }

        [[nodiscard]] bool cancel_task(TaskId task) noexcept {
            return Policy::template cancel_task<Tick, Capacity>(storage_, task);
        }

        [[nodiscard]] util::usize size() const noexcept {
            return Policy::template size<Tick, Capacity>(storage_);
        }

    private:
        typename Policy::template Storage<Tick, Capacity> storage_{};
    };

    template <typename Tick>
    class NoopTimerQueue {
    public:
        [[nodiscard]] bool schedule(TimerEntry<Tick>) noexcept {
            return false;
        }

        [[nodiscard]] std::optional<TimerEntry<Tick>> pop_due(Tick) noexcept {
            return std::nullopt;
        }

        [[nodiscard]] bool cancel(util::u64) noexcept {
            return false;
        }

        [[nodiscard]] bool cancel_task(TaskId) noexcept {
            return false;
        }
    };
}
