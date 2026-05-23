module;

#include <cstddef>
#include <cstdint>

export module player.app_config;

import audio.player;
import player.fixed_string;
import player.product_config;

export namespace player {
    enum class FontResourceKind : std::uint8_t {
        Builtin,
        FilePath,
        Package,
    };

    struct FontResourceConfig {
        FixedString<260> primary_path{};
        FixedString<260> fallback_path{};
        FixedString<96> package_key{};
        const std::byte* package_data{nullptr};
        std::size_t package_size_bytes{0};
        int small_px{product_config::default_font_small_px};
        int normal_px{product_config::default_font_normal_px};
        int large_px{product_config::default_font_large_px};
        FontResourceKind kind{FontResourceKind::Builtin};

        bool has_file_resource() const noexcept {
            return kind == FontResourceKind::FilePath && !primary_path.empty();
        }

        bool is_builtin() const noexcept {
            return kind == FontResourceKind::Builtin;
        }

        bool is_package() const noexcept {
            return kind == FontResourceKind::Package;
        }

        bool has_package_resource() const noexcept {
            return kind == FontResourceKind::Package
                && package_data != nullptr
                && package_size_bytes > 0;
        }
    };

    struct AppConfig {
        audio::PlayerConfig player_config{};
        FontResourceConfig font_resources{};
    };
}
