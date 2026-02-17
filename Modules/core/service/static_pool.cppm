module;

#include <array>
#include <cstddef>
#include <optional>

export module service.static_pool;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class StaticPool {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] std::optional<util::usize> allocate() noexcept {
            if (free_count_ == 0) {
                return std::nullopt;
            }
            const auto index = free_[--free_count_];
            used_[index] = true;
            return index;
        }

        [[nodiscard]] T& get(util::usize index) noexcept {
            return storage_[index];
        }

        [[nodiscard]] const T& get(util::usize index) const noexcept {
            return storage_[index];
        }

        void free(util::usize index) noexcept {
            if (index >= Capacity || !used_[index]) {
                return;
            }
            used_[index] = false;
            free_[free_count_++] = index;
        }

    private:
        std::array<T, Capacity> storage_{};
        std::array<util::usize, Capacity> free_{};
        std::array<bool, Capacity> used_{};
        util::usize free_count_{Capacity};

        constexpr void init_free() noexcept {
            for (util::usize i = 0; i < Capacity; ++i) {
                free_[i] = Capacity - 1 - i;
                used_[i] = false;
            }
        }

    public:
        StaticPool() noexcept {
            init_free();
        }
    };
}
