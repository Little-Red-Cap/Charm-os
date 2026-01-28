export module kernel.timer;

import <array>;
import <cstddef>;
import <optional>;
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

    template <typename Tick, util::usize Capacity>
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
            if (count_ == 0) {
                return std::nullopt;
            }

            util::usize best_index = Capacity;
            Tick best_due{};
            for (util::usize i = 0; i < count_; ++i) {
                const auto due = entries_[i].due;
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

            TimerEntry<Tick> result = entries_[best_index];
            entries_[best_index] = entries_[count_ - 1];
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
