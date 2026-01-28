module;

#include <array>
#include <cstddef>
#include <concepts>
#include <optional>

export module kernel.timer;

import kernel.evt;
import kernel.eda;
import util.core;

export namespace kernel {
    template <typename Tick>
    struct TimerEntry {
        Tick due{};
        TaskId task{};
        Event event{};
    };

    template <typename Policy, typename Tick, util::usize Capacity>
    concept TimerPolicy = requires(
        std::array<TimerEntry<Tick>, Capacity>& entries,
        util::usize& count,
        TimerEntry<Tick> entry,
        Tick now) {
        { Policy::insert(entries, count, entry) } -> std::same_as<bool>;
        { Policy::pop_due(entries, count, now) } -> std::same_as<std::optional<TimerEntry<Tick>>>;
    };

    struct LinearTimerPolicy {
        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool insert(
            std::array<TimerEntry<Tick>, Capacity>& entries,
            util::usize& count,
            TimerEntry<Tick> entry) noexcept {
            if (count >= Capacity) {
                return false;
            }
            entries[count++] = entry;
            return true;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static std::optional<TimerEntry<Tick>> pop_due(
            std::array<TimerEntry<Tick>, Capacity>& entries,
            util::usize& count,
            Tick now) noexcept {
            if (count == 0) {
                return std::nullopt;
            }

            util::usize best_index = Capacity;
            Tick best_due{};
            for (util::usize i = 0; i < count; ++i) {
                const auto due = entries[i].due;
                if (due <= now) {
                    if (best_index == Capacity || due < best_due) {
                        best_index = i;
                        best_due = due;
                    }
                }
            }

            if (best_index == Capacity) {
                return std::nullopt;
            }

            TimerEntry<Tick> result = entries[best_index];
            entries[best_index] = entries[count - 1];
            --count;
            return result;
        }
    };

    struct HeapTimerPolicy {
        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static bool insert(
            std::array<TimerEntry<Tick>, Capacity>& entries,
            util::usize& count,
            TimerEntry<Tick> entry) noexcept {
            if (count >= Capacity) {
                return false;
            }
            util::usize idx = count++;
            entries[idx] = entry;
            while (idx > 0) {
                const util::usize parent = (idx - 1) / 2;
                if (entries[parent].due <= entries[idx].due) {
                    break;
                }
                auto tmp = entries[parent];
                entries[parent] = entries[idx];
                entries[idx] = tmp;
                idx = parent;
            }
            return true;
        }

        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static std::optional<TimerEntry<Tick>> pop_due(
            std::array<TimerEntry<Tick>, Capacity>& entries,
            util::usize& count,
            Tick now) noexcept {
            if (count == 0) {
                return std::nullopt;
            }
            if (entries[0].due > now) {
                return std::nullopt;
            }

            TimerEntry<Tick> result = entries[0];
            entries[0] = entries[count - 1];
            --count;

            util::usize idx = 0;
            while (true) {
                const util::usize left = idx * 2 + 1;
                const util::usize right = left + 1;
                if (left >= count) {
                    break;
                }
                util::usize smallest = left;
                if (right < count && entries[right].due < entries[left].due) {
                    smallest = right;
                }
                if (entries[idx].due <= entries[smallest].due) {
                    break;
                }
                auto tmp = entries[idx];
                entries[idx] = entries[smallest];
                entries[smallest] = tmp;
                idx = smallest;
            }

            return result;
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
            return Policy::template insert<Tick, Capacity>(entries_, count_, entry);
        }

        [[nodiscard]] std::optional<TimerEntry<Tick>> pop_due(Tick now) noexcept {
            return Policy::template pop_due<Tick, Capacity>(entries_, count_, now);
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

    private:
        std::array<TimerEntry<Tick>, Capacity> entries_{};
        util::usize count_{0};
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
