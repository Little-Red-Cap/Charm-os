#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace h747::profiles {

enum class EvidenceStatus : std::uint8_t {
    ok,
    error,
};

struct EvidenceField {
    std::string_view key{};
    std::string_view value{};
};

struct EvidenceFrame {
    std::string_view capability{};
    std::string_view provider{};
    EvidenceStatus status{EvidenceStatus::ok};
    std::array<EvidenceField, 6> fields{};
    std::size_t field_count{0};
};

struct ProfileEvidence {
    std::string_view profile{};
    std::string_view board{};
    std::array<EvidenceFrame, 4> bindings{};
};

[[nodiscard]] constexpr EvidenceFrame selected_provider(std::string_view capability,
                                                        std::string_view provider,
                                                        std::string_view profile) noexcept {
    return EvidenceFrame{
        .capability = capability,
        .provider = provider,
        .status = EvidenceStatus::ok,
        .fields = {{
            {"profile", profile},
            {"selection", "explicit_binding"},
        }},
        .field_count = 2U,
    };
}

[[nodiscard]] constexpr EvidenceFrame selected_raster_display_provider(std::string_view provider,
                                                                       std::string_view profile,
                                                                       std::string_view mode,
                                                                       std::string_view format,
                                                                       std::string_view buffer_policy) noexcept {
    return EvidenceFrame{
        .capability = "RasterDisplay.primary_display",
        .provider = provider,
        .status = EvidenceStatus::ok,
        .fields = {{
            {"profile", profile},
            {"selection", "explicit_binding"},
            {"mode", mode},
            {"format", format},
            {"buffer_policy", buffer_policy},
        }},
        .field_count = 5U,
    };
}

[[nodiscard]] constexpr EvidenceFrame selected_input_provider(std::string_view provider,
                                                              std::string_view profile,
                                                              std::string_view source,
                                                              std::string_view pointer,
                                                              std::string_view encoders) noexcept {
    return EvidenceFrame{
        .capability = "Input.primary_input",
        .provider = provider,
        .status = EvidenceStatus::ok,
        .fields = {{
            {"profile", profile},
            {"selection", "explicit_binding"},
            {"source", source},
            {"pointer", pointer},
            {"encoders", encoders},
        }},
        .field_count = 5U,
    };
}

[[nodiscard]] constexpr ProfileEvidence host_player_profile_evidence() noexcept {
    return ProfileEvidence{
        .profile = "host_player",
        .board = "host_mock",
        .bindings = {{
            selected_provider("TextSink.log", "host_memory_log", "host_player"),
            selected_provider("Clock.monotonic_time", "host_fixed_clock", "host_player"),
            selected_raster_display_provider("host_framebuffer",
                                             "host_player",
                                             "180x320",
                                             "argb8888",
                                             "single_memory_framebuffer"),
            selected_input_provider("host_null_input",
                                    "host_player",
                                    "null_input",
                                    "none",
                                    "none"),
        }},
    };
}

[[nodiscard]] constexpr ProfileEvidence h747_player_profile_evidence() noexcept {
    return ProfileEvidence{
        .profile = "player",
        .board = "h747_diy",
        .bindings = {{
            selected_provider("TextSink.log", "h747_console", "player"),
            selected_provider("Clock.monotonic_time", "h747_console_clock", "player"),
            selected_raster_display_provider("h747_raster_display_service",
                                             "player",
                                             "720x1280",
                                             "argb8888",
                                             "double_buffer_vblank_reload"),
            selected_input_provider("h747_input_service",
                                    "player",
                                    "h747_input_service",
                                    "gt9xx_best_effort",
                                    "dual_encoder"),
        }},
    };
}

} // namespace h747::profiles
