module;

#include <cstdint>
#include <concepts>

export module hal_irq;

import hal_core;

export namespace hal {
    template <typename T>
    concept IrqGuard = requires {
        typename T::Token;
        { T::enter() } -> std::same_as<typename T::Token>;
        { T::leave(typename T::Token{}) } -> std::same_as<void>;
    };

    template <typename T>
    concept IrqController = requires {
        { T::enable() } -> std::same_as<void>;
        { T::disable() } -> std::same_as<void>;
    };
}
