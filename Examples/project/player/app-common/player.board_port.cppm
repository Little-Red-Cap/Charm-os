module;

#include <cstddef>

export module player.board_port;

import audio.player;
import player.app_config;
import player.cover_resource;
import player.display;
import player.input;
import player.product_config;

export namespace player {
    struct PlayerBoardFramebuffer {
        std::byte* pixels{nullptr};
        int width{0};
        int height{0};
        std::size_t stride_bytes{0};
        PlayerDisplayPixelFormat pixel_format{default_player_display_pixel_format};

        [[nodiscard]] PlayerDisplaySurface surface() const noexcept {
            return PlayerDisplaySurface{
                pixels,
                width,
                height,
                stride_bytes,
                pixel_format,
                PlayerDisplaySurfaceOwnership::Borrowed,
            };
        }

        [[nodiscard]] bool valid() const noexcept {
            return surface().valid();
        }
    };

    struct PlayerBoardFontPackageView {
        const std::byte* data{nullptr};
        std::size_t size_bytes{0};
        const char* key{nullptr};
        int small_px{product_config::default_font_small_px};
        int normal_px{product_config::default_font_normal_px};
        int large_px{product_config::default_font_large_px};

        [[nodiscard]] bool valid() const noexcept {
            return data != nullptr && size_bytes > 0;
        }
    };

    struct PlayerBoardPortConfig {
        PlayerBoardFramebuffer framebuffer{};
        PlayerBoardDisplayCallbacks display_callbacks{};
        PlayerTouchSampleSource touch_source{};
        PlayerBoardFontPackageView font_package{};
        PlayerCoverResourceProviderBinding cover_resource_provider{};
        PlayerCoverResourceRecordTableView cover_resource_records{};
        audio::PlayerConfig player_config{};
    };

    struct PlayerBoardPortBindings {
        PlayerDisplaySurface surface{};
        PlayerDisplaySink display_sink{};
        AppConfig app_config{};

        [[nodiscard]] bool valid() const noexcept {
            return surface.valid() && display_sink.present_fn != nullptr;
        }
    };

    inline AppConfig make_player_board_app_config(const PlayerBoardPortConfig& config) noexcept {
        AppConfig app_config{config.player_config};
        auto& font = app_config.font_resources;
        if (config.font_package.valid()) {
            font.kind = FontResourceKind::Package;
            font.package_data = config.font_package.data;
            font.package_size_bytes = config.font_package.size_bytes;
            font.package_key.assign(config.font_package.key);
            font.small_px = config.font_package.small_px;
            font.normal_px = config.font_package.normal_px;
            font.large_px = config.font_package.large_px;
        } else {
            font.kind = FontResourceKind::Builtin;
        }
        return app_config;
    }

    inline PlayerBoardPortBindings make_player_board_port_bindings(
        const PlayerBoardPortConfig& config,
        PlayerBoardDisplaySinkState& display_sink_state) noexcept {
        return PlayerBoardPortBindings{
            .surface = config.framebuffer.surface(),
            .display_sink = make_board_display_sink(display_sink_state, config.display_callbacks),
            .app_config = make_player_board_app_config(config),
        };
    }

    inline PlayerInputEventBatch read_player_board_touch_events(
        const PlayerBoardPortConfig& config,
        PlayerTouchAdapterState& state) noexcept {
        return read_player_touch_events(config.touch_source, state);
    }
}
