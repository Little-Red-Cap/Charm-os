module;

#include <array>
#include <cstddef>

export module service.fixed_vector;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class FixedVector {
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

        [[nodiscard]] util::usize size() const noexcept {
            return size_;
        }

        [[nodiscard]] T& operator[](util::usize index) noexcept {
            return data_[index];
        }

        [[nodiscard]] const T& operator[](util::usize index) const noexcept {
            return data_[index];
        }

    private:
        std::array<T, Capacity> data_{};
        util::usize size_{0};
    };
}
