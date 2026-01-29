module;

#include <array>
#include <cstddef>
#include <optional>

export module service.slab;

import util.core;
import util.span;

export namespace service {
    template <util::usize BlockSize, util::usize BlockCount>
    class Slab {
    public:
        static_assert(BlockSize >= 1);
        static_assert(BlockCount >= 1);

        [[nodiscard]] std::optional<util::usize> allocate() noexcept {
            if (free_count_ == 0) {
                return std::nullopt;
            }
            const auto index = free_[--free_count_];
            used_[index] = true;
            return index;
        }

        void release(util::usize index) noexcept {
            if (index >= BlockCount || !used_[index]) {
                return;
            }
            used_[index] = false;
            free_[free_count_++] = index;
        }

        [[nodiscard]] util::span<std::byte> block(util::usize index) noexcept {
            return util::span<std::byte>(storage_[index].data(), BlockSize);
        }

    private:
        std::array<std::array<std::byte, BlockSize>, BlockCount> storage_{};
        std::array<util::usize, BlockCount> free_{};
        std::array<bool, BlockCount> used_{};
        util::usize free_count_{BlockCount};

        constexpr void init_free() noexcept {
            for (util::usize i = 0; i < BlockCount; ++i) {
                free_[i] = BlockCount - 1 - i;
                used_[i] = false;
            }
        }

    public:
        Slab() noexcept {
            init_free();
        }
    };
}
