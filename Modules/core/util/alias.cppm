module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

export module util.alias;

export namespace util {
    using std::span;
    using std::as_bytes;
    using std::as_writable_bytes;
    using std::string_view;
    template <typename T, std::size_t N>
    using array = std::array<T, N>;
}
