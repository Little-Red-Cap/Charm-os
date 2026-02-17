module;
#include <cstddef>
export module charm.core.string;

export
template<std::size_t N>
class StaticString {
public:
    constexpr StaticString() = default;

    constexpr explicit StaticString(const char* s) noexcept { assign(s); }

    constexpr void assign(const char* s) noexcept {
        size_ = 0;
        if (!s) {
            data_[0] = '\0';
            return;
        }
        while (s[size_] != '\0' && size_ < N) {
            data_[size_] = s[size_];
            ++size_;
        }
        data_[size_] = '\0';
    }

    constexpr const char* c_str() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }

private:
    char data_[N + 1]{};
    std::size_t size_{0};
};
