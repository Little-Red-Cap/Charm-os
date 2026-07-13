#include "vivid_features.generated.hpp"

import audio.player;
import charm.core.config;
import player.md3_port;

#include <cstddef>
#include <cstdio>

int main() {
    constexpr std::size_t application_object = sizeof(player::PlayerMd3PortApplication);
    constexpr std::size_t framebuffer = static_cast<std::size_t>(screen_width)
        * static_cast<std::size_t>(screen_height) * 4u;
    constexpr std::size_t vivid_resident =
        static_cast<std::size_t>(CHARM_VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES);
    constexpr std::size_t audio_workspace = sizeof(audio::AudioPlayer);
    constexpr std::size_t total = application_object + framebuffer;
    constexpr std::size_t application_limit = 6u * 1024u * 1024u;
    constexpr std::size_t total_limit = 10u * 1024u * 1024u;

    std::printf("player-memory application_object=%zu framebuffer=%zu "
                "vivid_resident=%zu audio_workspace=%zu total=%zu\n",
                application_object,
                framebuffer,
                vivid_resident,
                audio_workspace,
                total);
    if (application_object > application_limit || total > total_limit) {
        std::puts("player-memory: budget exceeded");
        return 1;
    }
    std::puts("player-memory: ok");
    return 0;
}
