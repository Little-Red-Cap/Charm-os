module;

#include <array>
#include <cstddef>
#include <optional>

export module kernel.timer_wheel;

import kernel.timer;
import util.core;

export namespace kernel {
    template <util::usize Slots, util::usize Levels>
    struct HierWheelTimerPolicy {
        template <typename Tick, util::usize Capacity>
        struct Storage {
            std::array<TimerEntry<Tick>, Capacity> entries{};
            std::array<std::array<int, Slots>, Levels> head{};
            std::array<int, Capacity> next{};
            std::array<int, Capacity> free{};
            util::usize free_count{0};
            util::usize count{0};

            Storage() noexcept {
                for (util::usize l = 0; l < Levels; ++l) {
                    for (util::usize s = 0; s < Slots; ++s) {
                        head[l][s] = -1;
                    }
                }
                for (util::usize i = 0; i < Capacity; ++i) {
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
            const auto slot = static_cast<util::usize>(entry.due % Slots);
            const int idx = storage.free[--storage.free_count];
            storage.entries[idx] = entry;
            storage.next[idx] = storage.head[0][slot];
            storage.head[0][slot] = idx;
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

            const auto slot = static_cast<util::usize>(now % Slots);
            int current = storage.head[0][slot];
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
                storage.head[0][slot] = best_next;
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
            for (util::usize level = 0; level < Levels; ++level) {
                for (util::usize slot = 0; slot < Slots; ++slot) {
                    int current = storage.head[level][slot];
                    int prev = -1;
                    while (current != -1) {
                        const auto idx = static_cast<util::usize>(current);
                        if (storage.entries[idx].tag == tag) {
                            const int next = storage.next[idx];
                            if (prev == -1) {
                                storage.head[level][slot] = next;
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
            }
            return false;
        }
    };
}
