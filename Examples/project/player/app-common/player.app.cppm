module;

#include <string>
#include <vector>

export module player.app;

import player.mcu_policy;
import audio.player;
import audio.result;
import charm.system.clock;
import player.storage;
import player.ui;
import player.ui_builder;
import charm.ui.scene;
import input.raw_event;
import ui.input_adapter;

inline constexpr bool kPlayerAppMcuGuard =
    (player::mcu_policy::guard("player.app uses std::string/std::vector; port before MCU build."), true);

export namespace player {
    struct AppConfig {
        audio::PlayerConfig player_config{};
        std::string ttf_path{};
        int ttf_small_px{14};
        int ttf_normal_px{18};
        int ttf_large_px{76};
    };

    class App {
    public:
        App(AppConfig config, charm::system::Clock& clock)
            : config_(std::move(config)),
              player_(config_.player_config, clock) {}

        audio::Result<void> play(const char* path) { return player_.play(path); }
        audio::Result<void> stop() { return player_.stop(); }

        void tick() { player_.tick(); }
        void shutdown() noexcept { player_.shutdown(); }

        template <typename Controller>
        void shutdown(Controller& controller) noexcept {
            if constexpr (requires { controller.flush_weekly_listening_stats_history(); }) {
                (void)controller.flush_weekly_listening_stats_history();
            }
            player_.shutdown();
        }
        bool is_running() const noexcept { return player_.is_running(); }
        audio::AudioPlayer& player() noexcept { return player_; }
        const audio::AudioPlayer& player() const noexcept { return player_; }

        StorageState scan_storage() {
            last_storage_ = player::scan_storage();
            return last_storage_;
        }

        const StorageState& storage_state() const noexcept { return last_storage_; }
        StorageView storage_view() noexcept {
            player::ensure_track_labels(last_storage_);
            return make_storage_view(last_storage_);
        }

        template <typename Controller>
        void bind_player(Controller& controller) {
            controller.bind_player(player_);
        }

        template <typename Controller>
        bool bootstrap_player(Controller& controller,
                              int initial_index,
                              bool auto_start) {
            auto storage = scan_storage();
            controller.apply_storage_view(storage_view());
            if constexpr (requires { controller.load_weekly_listening_stats_history(); }) {
                controller.load_weekly_listening_stats_history();
            }
            if (controller.fs_ready && last_storage_.tracks.size() != 0) {
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

        template <typename Controller>
        void bind_ui(::ui::scene::SceneBuilder& builder, Controller& controller) {
            if (!config_.ttf_path.empty()) {
                if constexpr (requires {
                                  controller.set_font_config(config_.ttf_path,
                                                             config_.ttf_small_px,
                                                             config_.ttf_normal_px,
                                                             config_.ttf_large_px);
                              }) {
                    controller.set_font_config(config_.ttf_path,
                                               config_.ttf_small_px,
                                               config_.ttf_normal_px,
                                               config_.ttf_large_px);
                }
            }
            apply_player_theme();
            controller.icons = register_player_icons();
            controller.handles = build_ui(builder, controller, controller.icons);
            controller.init_text_slots();
            controller.init_pages();
            if constexpr (requires { controller.refresh_exact_font_styles(); }) {
                controller.refresh_exact_font_styles();
            }
            controller.focus_list();
            controller.set_time_label(0);
            controller.mount_status.assign("Mounting storage...");
            controller.set_status("Mounting storage");
            controller.update_list_placeholder();
            controller.sync_volume_value();
        }

        template <typename Controller>
        void dispatch_raw_input(::ui::scene::Scene& scene, Controller& controller, const input::RawInputEvent& ev) {
            const auto bridge = input::adapter::bridge_from_raw(ev);
            if (bridge.event) {
                scene.dispatch_event(*bridge.event);
                controller.process_input_events();
            }
        }

    private:
        AppConfig config_{};
        audio::AudioPlayer player_;
        StorageState last_storage_{};
    };
}

