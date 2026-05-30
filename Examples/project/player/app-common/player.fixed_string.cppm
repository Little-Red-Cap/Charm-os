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
        std::size_t size() const noexcept { return size_; }

        const char* c_str() const noexcept { return buffer_.data(); }
        char back() const noexcept { return size_ ? buffer_[size_ - 1] : '\0'; }

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

        bool append(std::string_view value) noexcept {
            if (value.empty()) return true;
            if (size_ >= Capacity - 1) return false;
            const std::size_t avail = (Capacity - 1) - size_;
            const std::size_t count = (value.size() < avail) ? value.size() : avail;
            for (std::size_t i = 0; i < count; ++i) {
                buffer_[size_ + i] = value[i];
            }
            size_ += count;
            buffer_[size_] = '\0';
            return count == value.size();
        }

        bool operator+=(std::string_view value) noexcept {
            return append(value);
        }

        bool operator+=(const char* value) noexcept {
            return append(value ? std::string_view(value) : std::string_view{});
        }

        template <std::size_t OtherCapacity>
        bool operator+=(const FixedString<OtherCapacity>& value) noexcept {
            return append(value.view());
        }

    private:
        std::array<char, Capacity> buffer_{};
        std::size_t size_{0};
    };
}
