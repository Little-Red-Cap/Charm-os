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
        Tick now) {
        { Policy::template insert<Tick, Capacity>(storage, entry) } -> std::same_as<bool>;
        { Policy::template pop_due<Tick, Capacity>(storage, now) } -> std::same_as<std::optional<TimerEntry<Tick>>>;
        { Policy::template size<Tick, Capacity>(storage) } -> std::same_as<util::usize>;
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
    };
}
