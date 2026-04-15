#pragma once

#include <cstddef>
#include <type_traits>

namespace out::detail {
    template <class UInt>
    inline char* append_unsigned_base(char* first,
                                      char* last,
                                      UInt value,
                                      unsigned base,
                                      bool upper = false) noexcept
    {
        static_assert(std::is_integral_v<UInt>);

        if (!first || !last || first > last) {
            return nullptr;
        }
        if (base < 2 || base > 16) {
            return nullptr;
        }

        using Unsigned = std::make_unsigned_t<UInt>;
        constexpr char kDigitsLower[] = "0123456789abcdef";
        constexpr char kDigitsUpper[] = "0123456789ABCDEF";
        const char* digits = upper ? kDigitsUpper : kDigitsLower;

        Unsigned current = static_cast<Unsigned>(value);
        char scratch[sizeof(Unsigned) * 8]{};
        char* begin = scratch + sizeof(scratch);

        do {
            const auto digit = static_cast<unsigned>(current % static_cast<Unsigned>(base));
            *--begin = digits[digit];
            current /= static_cast<Unsigned>(base);
        } while (current != 0);

        const auto len = static_cast<std::size_t>((scratch + sizeof(scratch)) - begin);
        if (static_cast<std::size_t>(last - first) < len) {
            return nullptr;
        }

        for (std::size_t i = 0; i < len; ++i) {
            first[i] = begin[i];
        }
        return first + len;
    }

    template <class UInt>
    inline char* append_unsigned_decimal(char* first, char* last, UInt value) noexcept
    {
        return append_unsigned_base(first, last, value, 10u, false);
    }
}
