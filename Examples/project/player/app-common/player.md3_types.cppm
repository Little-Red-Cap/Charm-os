module;

#include <cstdint>

export module player.md3_types;

export namespace player {
    enum class PlayerPage : std::uint8_t {
        Probe,
        Home,
        NowPlaying,
        Library,
    };
}
