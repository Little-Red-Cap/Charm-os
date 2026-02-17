module;

#include <array>
#include <cstddef>
#include <optional>

export module service.fixed_allocator;

import util.core;

export namespace service {
    template <util::usize Capacity>
    class FixedAllocator {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] std::optional<util::usize> allocate(util::usize size) noexcept {
            if (size == 0 || size > Capacity || used_) {
                return std::nullopt;
            }
            used_ = true;
            size_ = size;
            return 0;
        }

        void release() noexcept {
            used_ = false;
            size_ = 0;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return size_;
        }

        [[nodiscard]] std::byte* data() noexcept {
            return storage_.data();
        }

    private:
        std::array<std::byte, Capacity> storage_{};
        util::usize size_{0};
        bool used_{false};
    };
}
