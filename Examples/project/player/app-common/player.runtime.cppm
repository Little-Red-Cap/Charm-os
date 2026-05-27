module;

#include <cstdint>
#include <optional>
#include <utility>

export module player.runtime;

import audio.player;
import charm.gfx.color;
import charm.gfx.draw_cmd;
import charm.system.clock;
import charm.ui.scene;
import player.app;
import player.display;
import player.input;
import player.platform;
import player.storage;

export namespace player {
    struct PlayerRuntimeHooks {
        using TickVisualFn = void (*)(void* ctx, float t_sec, bool active) noexcept;

        void* tick_visual_ctx{nullptr};
        TickVisualFn tick_visual{nullptr};
    };

    template <typename Page>
    struct PlayerRuntimeConfig {
        AppConfig app_config{};
        StorageConfig storage_config{};
        Page start_page{};
        int initial_track_index{0};
        bool auto_start{false};
        rgba clear_color{0, 0, 0, 255};
    };

    template <typename Controller, typename Page>
    class PlayerRuntime {
    public:
        PlayerRuntime(charm::system::Clock& clock,
                      PlayerPlatform& platform,
                      Controller& controller,
                      PlayerRuntimeConfig<Page> config)
            : clock_(&clock),
              platform_(&platform),
              controller_(&controller),
              config_(std::move(config)) {}

        App* app() noexcept { return app_ ? &(*app_) : nullptr; }
        const App* app() const noexcept { return app_ ? &(*app_) : nullptr; }

        PlayerPlatform* platform() noexcept { return platform_; }
        const PlayerPlatform* platform() const noexcept { return platform_; }

        Controller* controller() noexcept { return controller_; }
        const Controller* controller() const noexcept { return controller_; }

        ::ui::scene::Scene& scene_ref() noexcept { return platform_->scene_ref(); }
        const ::ui::scene::Scene& scene_ref() const noexcept { return platform_->scene_ref(); }

        float t_sec() const noexcept { return t_sec_; }
        const PlayerRuntimeConfig<Page>& config() const noexcept { return config_; }

        bool bootstrap() {
            if (!clock_ || !platform_ || !controller_) {
                return false;
            }
            charm::system::ClockCaps::TimeSource::bind(*clock_);
            app_.emplace(config_.app_config, *clock_);
            init_storage(config_.storage_config);
            app_->bind_player(*controller_);
            controller_->bind_scene(platform_->scene_ref());
            if constexpr (requires { controller_->set_start_page(config_.start_page); }) {
                controller_->set_start_page(config_.start_page);
            }
            (void)app_->scan_storage();
            apply_storage_view_compat();
            platform_->build_scene([&](::ui::scene::SceneBuilder& builder) {
                app_->bind_ui(builder, *controller_);
            });
            if constexpr (requires { controller_->set_page(config_.start_page); }) {
                controller_->set_page(config_.start_page);
            }
            return app_->bootstrap_player(*controller_,
                                          config_.initial_track_index,
                                          config_.auto_start);
        }

        void tick(charm::system::ClockTick now_us,
                  PlayerRuntimeHooks hooks = {}) {
            if (!app_ || !controller_) {
                return;
            }
            t_sec_ = static_cast<float>(now_us) * 0.000001f;
            app_->tick();
            controller_->tick_player(app_->player());
            if (hooks.tick_visual) {
                hooks.tick_visual(hooks.tick_visual_ctx, t_sec_, controller_->is_playing());
            }
        }

        void dispatch_input(const PlayerInputEvent& ev) {
            if (!app_ || !platform_ || !controller_) {
                return;
            }
            app_->dispatch_player_input(platform_->scene_ref(), *controller_, ev);
        }

        bool render(PlayerDisplaySink* display_sink,
                    ::ui::scene::Scene::OverlayFn overlay_fn = nullptr,
                    void* overlay_ctx = nullptr) {
            if (!platform_ || !controller_) {
                return false;
            }
            PlayerFrameContext frame{
                .platform = platform_,
                .display_sink = display_sink,
                .clear_color = config_.clear_color,
            };
            return render_player_frame(frame, *controller_, overlay_fn, overlay_ctx);
        }

        void shutdown() noexcept {
            if (app_) {
                if (controller_) {
                    app_->shutdown(*controller_);
                } else {
                    app_->shutdown();
                }
                app_.reset();
            }
        }

    private:
        void apply_storage_view_compat() {
            if constexpr (requires { controller_->apply_storage_view(app_->storage_view(), false); }) {
                controller_->apply_storage_view(app_->storage_view(), false);
            } else {
                controller_->apply_storage_view(app_->storage_view());
            }
        }

        charm::system::Clock* clock_{nullptr};
        PlayerPlatform* platform_{nullptr};
        Controller* controller_{nullptr};
        PlayerRuntimeConfig<Page> config_{};
        std::optional<App> app_{};
        float t_sec_{0.0f};
    };
}
