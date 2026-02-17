//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>

export module input.intent;

export namespace input
{
    // Default intents consumed by the UI layer.
    // Covers: key navigation, encoder adjust, basic pointer input.
    enum class IntentType : std::uint8_t {
        NavPrev,
        NavNext,
        Activate, // Enter/OK/Click
        Back,

        Adjust, // delta (encoder, axis, etc.)
        PointerDown,
        PointerMove,
        PointerUp,
    };

    struct Intent {
        IntentType    type{IntentType::NavNext};
        std::int16_t  a{0}; // Adjust=delta, Pointer=x
        std::int16_t  b{0}; // Pointer=y
        std::uint32_t ms{0};
    };
} // namespace input
