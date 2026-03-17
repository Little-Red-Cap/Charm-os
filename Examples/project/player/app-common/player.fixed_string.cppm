module;
#include <array>
#include <cstddef>
#include <string_view>

export module player.fixed_string;

export namespace player {
    template <std::size_t Capacity>
    class FixedString {
    public:
        FixedString() = default;

        void clear() noexcept {
            size_ = 0;
            buffer_[0] = '\0';
        }

        bool empty() const noexcept { return size_ == 0; }

        const char* c_str() const noexcept { return buffer_.data(); }

        std::string_view view() const noexcept {
            return std::string_view(buffer_.data(), size_);
        }

        void assign(std::string_view value) noexcept {
            const std::size_t count = (value.size() < Capacity - 1) ? value.size() : (Capacity - 1);
            for (std::size_t i = 0; i < count; ++i) {
                buffer_[i] = value[i];
            }
            size_ = count;
            buffer_[size_] = '\0';
        }

        void assign(const char* value) noexcept {
            if (!value) {
                clear();
                return;
            }
            assign(std::string_view(value));
        }

    private:
        std::array<char, Capacity> buffer_{};
        std::size_t size_{0};
    };
}
