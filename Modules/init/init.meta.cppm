module;

#include <cstddef>
#include <string_view>

export module init.meta;

import init.node;
import util.core;

export namespace init {
    template <std::size_t N>
    struct fixed_string {
        char value[N]{};

        constexpr fixed_string(const char (&text)[N]) noexcept {
            for (std::size_t i = 0; i < N; ++i) {
                value[i] = text[i];
            }
        }

        constexpr std::size_t size() const noexcept {
            return N;
        }

        constexpr const char* data() const noexcept {
            return value;
        }

        constexpr std::string_view sv() const noexcept {
            return std::string_view{value, N > 0 ? (N - 1) : 0};
        }

        constexpr bool operator==(const fixed_string&) const noexcept = default;
    };

    template <std::size_t N>
    fixed_string(const char (&)[N]) -> fixed_string<N>;

    template <fixed_string Name>
    struct cap_c {
        static constexpr auto name = Name;
        static constexpr CapId id = cap_id(Name.sv());

        static constexpr std::string_view view() noexcept {
            return name.sv();
        }
    };

    template <typename... Caps>
    struct cap_list {
    };

    template <typename List>
    struct cap_list_traits;

    template <typename... Caps>
    struct cap_list_traits<cap_list<Caps...>> {
        static constexpr util::usize size = sizeof...(Caps);

        template <typename Fn>
        static constexpr void for_each(Fn&& fn) noexcept {
            (fn(Caps::id), ...);
        }

        template <typename Fn>
        static constexpr void for_each_named(Fn&& fn) noexcept {
            (fn(Caps::id, Caps::view()), ...);
        }
    };
}
