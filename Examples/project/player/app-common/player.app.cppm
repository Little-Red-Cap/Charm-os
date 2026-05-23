module;

#include <cstdint>

export module player.app;

import audio.player;
import audio.result;
import charm.system.clock;
import player.app_config;
import player.input;
import player.storage;
import player.ui;
import player.ui_builder;
import charm.core.event;
import charm.ui.scene;
import input.raw_event;
import ui.input_adapter;

export namespace player {
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

        template <typename Controller>
        void dispatch_player_input(::ui::scene::Scene& scene,
                                   Controller& controller,
                                   const PlayerInputEvent& ev) {
            switch (ev.kind) {
            case PlayerInputEventKind::Pointer:
                dispatch_player_pointer(scene, controller, ev);
                break;
            case PlayerInputEventKind::Wheel:
                scene.dispatch_event(Event::wheel(static_cast<int>(ev.pointer.x),
                                                  static_cast<int>(ev.pointer.y),
                                                  static_cast<int>(ev.wheel_y),
                                                  ev.ms));
                controller.process_input_events();
                break;
            case PlayerInputEventKind::Button:
                dispatch_player_button(scene, controller, ev);
                break;
            case PlayerInputEventKind::Command:
                dispatch_player_command(controller, ev.command);
                break;
            default:
                break;
            }
        }

    private:
        [[nodiscard]] static std::int16_t clamp_pointer_coord(float value) noexcept {
            if (value < -32768.0f) return static_cast<std::int16_t>(-32768);
            if (value > 32767.0f) return static_cast<std::int16_t>(32767);
            return static_cast<std::int16_t>(value);
        }

        [[nodiscard]] static input::PointerAction to_raw_pointer_action(PlayerPointerAction action) noexcept {
            switch (action) {
            case PlayerPointerAction::Down:
                return input::PointerAction::Down;
            case PlayerPointerAction::Up:
            case PlayerPointerAction::Cancel:
                return input::PointerAction::Up;
            case PlayerPointerAction::Move:
            default:
                return input::PointerAction::Move;
            }
        }

        [[nodiscard]] static input::Button to_raw_button(PlayerInputCommand command) noexcept {
            switch (command) {
            case PlayerInputCommand::Up:
                return input::Button::Up;
            case PlayerInputCommand::Down:
                return input::Button::Down;
            case PlayerInputCommand::Left:
            case PlayerInputCommand::Back:
                return input::Button::Back;
            case PlayerInputCommand::Enter:
            default:
                return input::Button::Enter;
            }
        }

        template <typename Controller>
        void dispatch_player_pointer(::ui::scene::Scene& scene,
                                     Controller& controller,
                                     const PlayerInputEvent& ev) {
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = ev.ms;
            raw.pointer = input::PointerRaw{ev.pointer.down,
                                            clamp_pointer_coord(ev.pointer.x),
                                            clamp_pointer_coord(ev.pointer.y),
                                            ev.pointer.id};
            if (ev.pointer_action == PlayerPointerAction::Cancel) {
                raw.pointer.down = false;
            }
            raw.pointer_action = to_raw_pointer_action(ev.pointer_action);
            dispatch_raw_input(scene, controller, raw);
        }

        template <typename Controller>
        void dispatch_player_button(::ui::scene::Scene& scene,
                                    Controller& controller,
                                    const PlayerInputEvent& ev) {
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Button;
            raw.ms = ev.ms;
            raw.button = to_raw_button(ev.command);
            raw.pressed = ev.button_pressed;
            dispatch_raw_input(scene, controller, raw);
        }

        template <typename Controller>
        void dispatch_player_command(Controller& controller, PlayerInputCommand command) {
            if constexpr (requires { controller.handle_input_command(command); }) {
                controller.handle_input_command(command);
            }
        }

        AppConfig config_{};
        audio::AudioPlayer player_;
        StorageState last_storage_{};
    };
}

