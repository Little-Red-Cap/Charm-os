module;

#include <array>
#include <cstddef>
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

    struct LinearTimerPolicy {
        template <typename Tick, util::usize Capacity>
        [[nodiscard]] static std::optional<util::usize> select_index(
            const std::array<TimerEntry<Tick>, Capacity>& entries,
            util::usize count,
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
            return best_index;
        }
    };

    template <typename Tick, util::usize Capacity, typename Policy = LinearTimerPolicy>
    class TimerQueue {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool schedule(TimerEntry<Tick> entry) noexcept {
            if (count_ >= Capacity) {
                return false;
            }
            entries_[count_++] = entry;
            return true;
        }

        [[nodiscard]] std::optional<TimerEntry<Tick>> pop_due(Tick now) noexcept {
            const auto index = Policy::template select_index<Tick, Capacity>(entries_, count_, now);
            if (!index.has_value()) {
                return std::nullopt;
            }

            TimerEntry<Tick> result = entries_[*index];
            entries_[*index] = entries_[count_ - 1];
            --count_;
            return result;
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
