module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service.bitmap;

export namespace service {
    template <std::size_t Bits>
    class Bitmap {
    public:
        static_assert(Bits >= 1);

        void set(std::size_t bit) noexcept {
            const auto index = bit / 32;
            const auto offset = bit % 32;
            data_[index] |= (std::uint32_t{1} << offset);
        }

        void clear(std::size_t bit) noexcept {
            const auto index = bit / 32;
            const auto offset = bit % 32;
            data_[index] &= ~(std::uint32_t{1} << offset);
        }

        [[nodiscard]] bool test(std::size_t bit) const noexcept {
            const auto index = bit / 32;
            const auto offset = bit % 32;
            return (data_[index] >> offset) & 0x1u;
        }

    private:
        static constexpr std::size_t WordCount = (Bits + 31) / 32;
        std::array<std::uint32_t, WordCount> data_{};
    };
}
