module;

#include <array>
#include <cstddef>
#include <optional>

export module service.slot_pool;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class SlotPool {
    public:
        static_assert(Capacity >= 1);

        struct Handle {
            util::u32 index{0};
            util::u32 generation{0};
        };

        [[nodiscard]] std::optional<Handle> acquire() noexcept {
            if (free_count_ == 0) {
                return std::nullopt;
            }
            const auto index = free_[--free_count_];
            used_[index] = true;
            return Handle{static_cast<util::u32>(index), generations_[index]};
        }

        void release(Handle handle) noexcept {
            if (!valid(handle)) {
                return;
            }
            const auto index = static_cast<util::usize>(handle.index);
            used_[index] = false;
            ++generations_[index];
            free_[free_count_++] = index;
        }

        [[nodiscard]] bool valid(Handle handle) const noexcept {
            const auto index = static_cast<util::usize>(handle.index);
            if (index >= Capacity) {
                return false;
            }
            return used_[index] && generations_[index] == handle.generation;
        }

        [[nodiscard]] T& get(Handle handle) noexcept {
            return storage_[static_cast<util::usize>(handle.index)];
        }

    private:
        std::array<T, Capacity> storage_{};
        std::array<bool, Capacity> used_{};
        std::array<util::u32, Capacity> generations_{};
        std::array<util::usize, Capacity> free_{};
        util::usize free_count_{Capacity};

        constexpr void init_free() noexcept {
            for (util::usize i = 0; i < Capacity; ++i) {
                free_[i] = Capacity - 1 - i;
                used_[i] = false;
                generations_[i] = 0;
            }
        }

    public:
        SlotPool() noexcept {
            init_free();
        }
    };
}
