module;

#include <cstdint>

export module player.ui_policy;

export namespace player {
    enum class PlayerUiFontMode : std::uint8_t {
        BuiltinOnly = 0,
        FileFontsIfAvailable,
    };

    enum class PlayerUiCoverMode : std::uint8_t {
        PlaceholderOnly = 0,
        ResourceProviderOnly,
        DecodeIfAvailable,
    };

    enum class PlayerUiTransitionMode : std::uint8_t {
        StaticCut = 0,
        AutoLayered,
    };

    struct PlayerUiPolicy {
        PlayerUiFontMode font_mode{PlayerUiFontMode::BuiltinOnly};
        PlayerUiCoverMode cover_mode{PlayerUiCoverMode::PlaceholderOnly};
        PlayerUiTransitionMode transition_mode{PlayerUiTransitionMode::StaticCut};
        std::uint16_t list_cover_cache_entries{12};
        bool enable_perf_overlay{true};
    };
}
