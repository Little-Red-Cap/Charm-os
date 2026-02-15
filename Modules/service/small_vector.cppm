module;

#include <array>
#include <cstddef>

export module service.small_vector;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class SmallVector {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool push_back(const T& value) noexcept {
            if (size_ >= Capacity) {
                return false;
            }
            data_[size_++] = value;
            return true;
        }

        [[nodiscard]] bool pop_back() noexcept {
            if (size_ == 0) {
                return false;
            }
            --size_;
            return true;
        }

        void clear() noexcept { size_ = 0; }

        [[nodiscard]] util::usize size() const noexcept { return size_; }
        [[nodiscard]] constexpr util::usize capacity() const noexcept { return Capacity; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

        [[nodiscard]] T& operator[](util::usize index) noexcept { return data_[index]; }
        [[nodiscard]] const T& operator[](util::usize index) const noexcept { return data_[index]; }

        [[nodiscard]] T* data() noexcept { return data_.data(); }
        [[nodiscard]] const T* data() const noexcept { return data_.data(); }

    private:
        std::array<T, Capacity> data_{};
        util::usize size_{0};
    };
}
