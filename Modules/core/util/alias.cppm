module;

#include <cstddef>
#include <type_traits>

#if defined(CHARM_USE_ETL)
#include <etl/array.h>
#include <etl/span.h>
#include <etl/string_view.h>
#else
#include <array>
#include <span>
#include <string_view>
#endif

export module util.alias;

export namespace util {
#if defined(CHARM_USE_ETL)
    using etl::span;
    using etl::string_view;
    template <typename T, std::size_t N>
    using array = etl::array<T, N>;

    template <class T>
    constexpr span<const std::byte> as_bytes(span<const T> s) noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        return {reinterpret_cast<const std::byte*>(s.data()), s.size() * sizeof(T)};
    }

    template <class T>
    constexpr span<std::byte> as_writable_bytes(span<T> s) noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        return {reinterpret_cast<std::byte*>(s.data()), s.size() * sizeof(T)};
    }
#else
    using std::span;
    using std::as_bytes;
    using std::as_writable_bytes;
    using std::string_view;
    template <typename T, std::size_t N>
    using array = std::array<T, N>;
#endif
}
