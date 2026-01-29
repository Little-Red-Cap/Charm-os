module;

#include <array>
#include <cstddef>

export module service.handle_table;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class HandleTable {
    public:
        struct Handle {
            util::u32 index{0};
            util::u32 generation{0};
        };

        [[nodiscard]] bool allocate(Handle& out) noexcept {
            if (free_count_ == 0) {
                return false;
            }
            const auto index = free_[--free_count_];
            used_[index] = true;
            out.index = static_cast<util::u32>(index);
            out.generation = generations_[index];
            return true;
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
        HandleTable() noexcept {
            init_free();
        }
    };
}
