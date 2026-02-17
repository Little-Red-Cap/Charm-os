module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service_pool;

import util.core;

export namespace service {
    template <util::usize BlockSize, util::usize Capacity>
    class Pool {
    public:
        constexpr Pool() = default;

        [[nodiscard]] void* alloc() noexcept {
            if (free_ == 0) {
                return nullptr;
            }
            const util::usize idx = free_list_[--free_];
            return blocks_[idx].data();
        }

        void free(void* p) noexcept {
            if (!p || free_ >= Capacity) {
                return;
            }
            const auto base = blocks_.data();
            const auto ptr = static_cast<std::byte*>(p);
            const auto idx = static_cast<util::usize>(ptr - base[0].data()) / BlockSize;
            if (idx < Capacity) {
                free_list_[free_++] = idx;
            }
        }

        constexpr void reset() noexcept {
            free_ = Capacity;
            for (util::usize i = 0; i < Capacity; ++i) {
                free_list_[i] = Capacity - 1 - i;
            }
        }

        [[nodiscard]] constexpr util::usize available() const noexcept { return free_; }

    private:
        std::array<std::array<std::byte, BlockSize>, Capacity> blocks_{};
        std::array<util::usize, Capacity> free_list_{};
        util::usize free_{Capacity};
    };
}
