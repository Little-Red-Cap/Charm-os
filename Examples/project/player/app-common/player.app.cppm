module;
#include <string>
#include <vector>

export module player.app;

import audio.player;
import audio.result;
import charm.system.clock;
import player.storage;
import player.ui;
import player.ui_builder;
import charm.core.soa_gui;
import input.raw_event;
import ui.input_adapter;

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

        template <typename Controller, typename Factory>
        void bind_ui(Factory& factory, Controller& controller) {
            apply_player_theme();
            controller.icons = register_player_icons();
            controller.handles = build_ui(factory, controller, controller.icons);
            controller.init_text_slots();
            controller.focus_list();
            controller.set_time_label(0);
            controller.mount_status = "Mounting storage...";
            controller.set_status("Mounting storage");
            controller.update_list_placeholder();
            controller.sync_volume_value();
        }

        template <typename Controller>
        void dispatch_raw_input(SoaGui& gui, Controller& controller, const input::RawInputEvent& ev) {
            const auto bridge = input::adapter::bridge_from_raw(ev);
            if (bridge.event) {
                gui.dispatch_event(*bridge.event);
                controller.process_input_events();
            }
        }

    private:
        audio::AudioPlayer player_;
    };
}
