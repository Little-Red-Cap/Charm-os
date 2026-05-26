module;

#include <cstddef>

export module player.board_port;

import audio.player;
import player.app_config;
import player.cover_resource;
import player.display;
import player.font_resource;
import player.input;

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

    struct PlayerBoardPortConfig {
        PlayerBoardFramebuffer framebuffer{};
        PlayerBoardDisplayCallbacks display_callbacks{};
        PlayerTouchSampleSource touch_source{};
        PlayerFontPackageResourceView font_package{};
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
        app_config.font_resources = make_package_font_resource_config(config.font_package);
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
