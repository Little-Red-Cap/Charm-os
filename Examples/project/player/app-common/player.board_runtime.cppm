module;

#include <optional>
#include <utility>

export module player.board_runtime;

import charm.gfx.color;
import charm.system.clock;
import player.board_port;
import player.display;
import player.platform;
import player.runtime;
import player.storage;

export namespace player {
    template <typename Controller, typename Page>
    struct PlayerBoardRuntimeStorage {
        PlayerBoardDisplaySinkState display_sink_state{};
        std::optional<PlayerPlatform> platform{};
        std::optional<PlayerRuntime<Controller, Page>> runtime{};

        void reset() noexcept {
            if (runtime) {
                runtime->shutdown();
                runtime.reset();
            }
            platform.reset();
            display_sink_state = {};
        }
    };

    template <typename Page>
    struct PlayerBoardRuntimeConfig {
        PlayerBoardPortConfig port{};
        StorageConfig storage_config{};
        Page start_page{};
        int initial_track_index{0};
        bool auto_start{false};
        rgba clear_color{0, 0, 0, 255};
    };

    template <typename Page>
    PlayerRuntimeConfig<Page> make_player_board_runtime_config(
        const PlayerBoardRuntimeConfig<Page>& config) noexcept {
        return PlayerRuntimeConfig<Page>{
            .app_config = make_player_board_app_config(config.port),
            .storage_config = config.storage_config,
            .start_page = config.start_page,
            .initial_track_index = config.initial_track_index,
            .auto_start = config.auto_start,
            .clear_color = config.clear_color,
        };
    }

    template <typename Controller, typename Page>
    PlayerDisplaySink make_player_board_runtime(charm::system::Clock& clock,
                                                Controller& controller,
                                                const PlayerBoardRuntimeConfig<Page>& config,
                                                PlayerBoardRuntimeStorage<Controller, Page>& storage) {
        storage.reset();
        auto bindings = make_player_board_port_bindings(config.port, storage.display_sink_state);
        storage.platform.emplace(bindings.surface);
        storage.runtime.emplace(clock,
                                *storage.platform,
                                controller,
                                PlayerRuntimeConfig<Page>{
                                    .app_config = std::move(bindings.app_config),
                                    .storage_config = config.storage_config,
                                    .start_page = config.start_page,
                                    .initial_track_index = config.initial_track_index,
                                    .auto_start = config.auto_start,
                                    .clear_color = config.clear_color,
                                });
        return bindings.display_sink;
    }
}
