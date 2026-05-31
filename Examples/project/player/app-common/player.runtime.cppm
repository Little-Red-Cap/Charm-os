module;

#include <cstdint>
#include <optional>
#include <utility>

export module player.runtime;

import audio.player;
import charm.gfx.color;
import charm.system.clock;
import charm.ui.scene;
import player.app;
import player.app_config;
import player.cover_resource;
import player.display;
import player.input;
import player.platform;
import player.scene_runtime;
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
        PlayerCoverResourceProviderBinding cover_resource_provider{};
        PlayerCoverResourceRecordTableView cover_resource_records{};
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
            install_cover_resource_provider_binding();
            charm::system::ClockCaps::TimeSource::bind(*clock_);
            app_.emplace(config_.app_config, *clock_);
            init_storage(config_.storage_config);
            app_->bind_player(*controller_);
            if constexpr (requires { controller_->bind_scene_runtime(make_player_scene_runtime(platform_->scene_ref())); }) {
                controller_->bind_scene_runtime(make_player_scene_runtime(platform_->scene_ref()));
            } else {
                controller_->bind_scene(platform_->scene_ref());
            }
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
            restore_cover_resource_provider_binding();
        }

    private:
        static PlayerSceneRuntime make_player_scene_runtime(::ui::scene::Scene& scene) noexcept {
            return PlayerSceneRuntime{
                .ctx = &scene,
                .access = scene.access(),
                .release_snapshot_fn = [](void* ctx, ::ui::scene::SnapshotHandle handle) noexcept -> bool {
                    return ctx && static_cast<::ui::scene::Scene*>(ctx)->release_snapshot(handle);
                },
                .mark_snapshot_stale_fn = [](void* ctx, ::ui::scene::SnapshotHandle handle) noexcept -> bool {
                    return ctx && static_cast<::ui::scene::Scene*>(ctx)->mark_snapshot_stale(handle);
                },
                .layer_stats_fn = [](void* ctx) noexcept -> ::ui::scene::LayerStats {
                    return ctx ? static_cast<::ui::scene::Scene*>(ctx)->layer_stats() : ::ui::scene::LayerStats{};
                },
                .capture_snapshot_fn =
                    [](void* ctx, const ::ui::scene::SnapshotSpec& spec) noexcept -> PlayerLayerCaptureResult {
                        if (!ctx) return {};
                        const auto capture = (spec.preferred_kind == ::ui::scene::SnapshotKind::PixelSurface)
                            ? static_cast<::ui::scene::Scene*>(ctx)->capture_pixel_snapshot_result(spec)
                            : static_cast<::ui::scene::Scene*>(ctx)->capture_command_snapshot_result(spec);
                        return PlayerLayerCaptureResult{capture.status, capture.handle};
                    },
                .make_compose_plan_fn =
                    [](void* ctx, const ::ui::scene::LayerComposeSpec& spec) noexcept
                        -> ::ui::scene::LayerComposePlan {
                        return ctx
                            ? static_cast<::ui::scene::Scene*>(ctx)->make_snapshot_compose_plan(spec)
                            : ::ui::scene::LayerComposePlan{};
                    },
                .compose_pixel_snapshot_fn =
                    [](void* ctx, const ::ui::scene::LayerComposePlan& plan) noexcept
                        -> PlayerLayerReplayResult {
                        if (!ctx) return {};
                        const auto replay =
                            static_cast<::ui::scene::Scene*>(ctx)->compose_pixel_snapshot(plan);
                        return PlayerLayerReplayResult{
                            .status = replay.status,
                            .source = replay.source,
                            .kind = replay.kind,
                            .target_bounds = replay.target_bounds,
                            .alpha_blend_count = replay.stats.alpha_blend_count,
                        };
                    },
            };
        }

        void install_cover_resource_provider_binding() noexcept {
            if (cover_resource_provider_installed_) {
                return;
            }
            previous_cover_resource_provider_ = cover_resource_provider_binding();
            auto binding = config_.cover_resource_provider;
            if (!binding.valid() && config_.cover_resource_records.valid()) {
                binding = make_cover_resource_record_table_binding(config_.cover_resource_records);
            }
            set_cover_resource_provider_binding(binding);
            cover_resource_provider_installed_ = true;
        }

        void restore_cover_resource_provider_binding() noexcept {
            if (!cover_resource_provider_installed_) {
                return;
            }
            set_cover_resource_provider_binding(previous_cover_resource_provider_);
            previous_cover_resource_provider_ = {};
            cover_resource_provider_installed_ = false;
        }

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
        PlayerCoverResourceProviderBinding previous_cover_resource_provider_{};
        bool cover_resource_provider_installed_{false};
        float t_sec_{0.0f};
    };
}
