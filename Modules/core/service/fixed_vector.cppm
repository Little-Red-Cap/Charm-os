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

        [[nodiscard]] bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] constexpr util::usize capacity() const noexcept {
            return Capacity;
        }

        [[nodiscard]] bool push_back(const T& value) noexcept {
            if (size_ >= Capacity) {
                return false;
            }
            data_[size_++] = value;
            return true;
        }

        [[nodiscard]] bool resize(util::usize new_size) noexcept {
            if (new_size > Capacity) {
                return false;
            }
            if (new_size > size_) {
                for (util::usize i = size_; i < new_size; ++i) {
                    data_[i] = T{};
                }
            }
            size_ = new_size;
            return true;
        }

        void clear() noexcept {
            size_ = 0;
        }

        void fill(const T& value) noexcept {
            for (util::usize i = 0; i < Capacity; ++i) {
                data_[i] = value;
            }
            size_ = Capacity;
        }

        [[nodiscard]] T* begin() noexcept {
            return data_.data();
        }

        [[nodiscard]] const T* begin() const noexcept {
            return data_.data();
        }

        [[nodiscard]] T* end() noexcept {
            return data_.data() + size_;
        }

        [[nodiscard]] const T* end() const noexcept {
            return data_.data() + size_;
        }

        [[nodiscard]] T& front() noexcept {
            return data_[0];
        }

        [[nodiscard]] const T& front() const noexcept {
            return data_[0];
        }

        [[nodiscard]] T& back() noexcept {
            return data_[size_ - 1];
        }

        [[nodiscard]] const T& back() const noexcept {
            return data_[size_ - 1];
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
