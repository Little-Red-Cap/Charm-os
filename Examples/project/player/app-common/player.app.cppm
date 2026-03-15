module;
#include <string>
#include <vector>

export module player.app;

import audio.player;
import audio.result;
import charm.system.clock;
import player.storage;

export namespace player {
    struct AppConfig {
        audio::PlayerConfig player_config{};
    };

    class App {
    public:
        App(AppConfig config, charm::system::Clock& clock)
            : player_(config.player_config, clock) {}

        audio::Result<void> play(const char* path) { return player_.play(path); }
        audio::Result<void> stop() { return player_.stop(); }

        void tick() { player_.tick(); }
        bool is_running() const noexcept { return player_.is_running(); }
        audio::AudioPlayer& player() noexcept { return player_; }
        const audio::AudioPlayer& player() const noexcept { return player_; }

        StorageState scan_storage() { return player::scan_storage(); }

        template <typename Controller>
        void bind_player(Controller& controller) {
            controller.bind_player(player_);
        }

        template <typename Controller>
        bool bootstrap_player(Controller& controller,
                              std::vector<std::string>& tracks,
                              int initial_index,
                              bool auto_start) {
            auto storage = scan_storage();
            controller.apply_storage_state(std::move(storage));
            if (controller.fs_ready && !tracks.empty()) {
                if (!controller.load_track_index(initial_index)) {
                    controller.clear_track_state();
                } else if (auto_start) {
                    controller.start_playback();
                }
            } else {
                controller.clear_track_state();
            }
            controller.set_play_button_text(false);
            controller.set_time_label(0);
            controller.sync_progress_value(0);
            controller.reset_duration();
            controller.update_list_placeholder();
            return controller.track_ready();
        }

    private:
        audio::AudioPlayer player_;
    };
}
