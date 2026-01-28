export module util.type_list;

import <cstddef>;
import <type_traits>;

export namespace util {
    template <typename... Ts>
    struct type_list { };

    template <typename T, typename List>
    struct index_of;

    template <typename T, typename... Ts>
    struct index_of<T, type_list<T, Ts...>> : std::integral_constant<std::size_t, 0> { };

    template <typename T, typename U, typename... Ts>
    struct index_of<T, type_list<U, Ts...>>
        : std::integral_constant<std::size_t, 1 + index_of<T, type_list<Ts...>>::value> { };
}
