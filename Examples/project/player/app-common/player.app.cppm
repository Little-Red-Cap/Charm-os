module;

export module player.app;

import audio.player;
import audio.result;
import charm.system.clock;
import player.fixed_string;
import player.product_config;
import player.storage;
import player.ui;
import player.ui_builder;
import charm.ui.scene;
import input.raw_event;
import ui.input_adapter;

export namespace player {
    struct FontResourceConfig {
        FixedString<260> primary_path{};
        FixedString<260> fallback_path{};
        int small_px{product_config::default_font_small_px};
        int normal_px{product_config::default_font_normal_px};
        int large_px{product_config::default_font_large_px};
        bool file_backed{false};

        bool has_file_resource() const noexcept {
            return file_backed && !primary_path.empty();
        }
    };

    struct AppConfig {
        audio::PlayerConfig player_config{};
        FontResourceConfig font_resources{};
    };

    class App {
    public:
        App(AppConfig config, charm::system::Clock& clock)
            : config_(config),
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
            (void)scan_storage();
            if constexpr (requires { controller.apply_storage_view(storage_view(), false); }) {
                controller.apply_storage_view(storage_view(), false);
            } else {
                controller.apply_storage_view(storage_view());
            }
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
            const auto& font = config_.font_resources;
            if (font.has_file_resource()) {
                if constexpr (requires {
                                  controller.set_font_config(font.primary_path.view(),
                                                             font.fallback_path.view(),
                                                             font.small_px,
                                                             font.normal_px,
                                                             font.large_px);
                              }) {
                    controller.set_font_config(font.primary_path.view(),
                                               font.fallback_path.view(),
                                               font.small_px,
                                               font.normal_px,
                                               font.large_px);
                } else if constexpr (requires {
                                         controller.set_font_config(font.primary_path.view(),
                                                                    font.small_px,
                                                                    font.normal_px,
                                                                    font.large_px);
                                     }) {
                    controller.set_font_config(font.primary_path.view(),
                                               font.small_px,
                                               font.normal_px,
                                               font.large_px);
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

