#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

// GCC 16.1 cannot merge the legacy runtime/platform/evidence BMIs in this Host TU.
// Keep the equivalent composition local; canonical Player Port code does not use this layer.
namespace ui::scene {
    struct SceneCmdEvidence {
        std::uint32_t cmd_count{}, cmd_capacity{}, cmd_bytes{};
        std::uint32_t text_used{}, text_capacity{}, blob_used{}, blob_capacity{};
        std::uint32_t batch_shrink{}, batch_shrink_line{}, batch_shrink_path{};
        std::uint32_t batch_shrink_rect{}, batch_shrink_round{}, batch_shrink_image{};
        std::uint32_t batch_shrink_focus{}, cmd_overflowed{}, text_overflowed{}, blob_overflowed{};
    };

    struct SceneExecEvidence {
        std::uint32_t cmd_count{}, cmd_bytes{}, failed_cmds{};
        std::uint32_t clip_pushes{}, clip_pops{}, clip_push_overflow{}, clip_pop_underflow{};
        std::uint32_t clip_invalid{}, fail_blob{}, fail_path{}, fail_clip{}, fail_other{};
        std::uint32_t dispatch_groups{}, batch_flushes{};
        std::uint32_t group_rect{}, group_text{}, group_image{}, group_line{}, group_path{}, group_other{};
        std::uint32_t cmd_text{}, cmd_rect{}, cmd_image{}, cmd_line{}, cmd_path{}, cmd_other{};
        std::uint32_t fail_text{}, fail_image{}, alpha_blend_count{}, overflowed{};
    };

    struct SceneLayerEvidence {
        std::uint32_t snapshot_count{}, snapshot_rebuild_count{}, stale_snapshot_count{};
        std::uint32_t layer_bytes{}, composite_pixels{}, pixel_blit_count{}, pixel_blit_pixels{};
    };

    struct SceneTimingEvidence {
        std::uint32_t available{}, record_us{}, execute_us{}, render_us{};
    };

    struct SceneEvidence {
        SceneCmdEvidence cmd{};
        SceneExecEvidence exec{};
        SceneLayerEvidence layer{};
        SceneTimingEvidence timing{};
        ui::draw_cmd::DrawCmdDetailEvidence draw_detail{};
    };

    inline constexpr std::uint32_t host_scene_evidence_u32(std::uint64_t value) noexcept {
        return static_cast<std::uint32_t>(value > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : value);
    }

    inline constexpr std::uint32_t host_scene_evidence_size(std::size_t value) noexcept {
        return host_scene_evidence_u32(static_cast<std::uint64_t>(value));
    }

    inline SceneEvidence make_scene_evidence(const Scene& scene) noexcept {
        const auto cmd = scene.last_cmd_stats();
        const auto exec = scene.last_exec_stats();
        const auto layer = scene.layer_stats();
        const auto timing = scene.last_render_timing();
        SceneEvidence out{};
        out.cmd = {
            host_scene_evidence_size(cmd.cmd_count), host_scene_evidence_size(cmd.cmd_capacity),
            host_scene_evidence_size(cmd.cmd_bytes), host_scene_evidence_size(cmd.text_used),
            host_scene_evidence_size(cmd.text_capacity), host_scene_evidence_size(cmd.blob_used),
            host_scene_evidence_size(cmd.blob_capacity), host_scene_evidence_size(cmd.batch_shrink),
            host_scene_evidence_size(cmd.batch_shrink_line), host_scene_evidence_size(cmd.batch_shrink_path),
            host_scene_evidence_size(cmd.batch_shrink_rect), host_scene_evidence_size(cmd.batch_shrink_round),
            host_scene_evidence_size(cmd.batch_shrink_image), host_scene_evidence_size(cmd.batch_shrink_focus),
            cmd.cmd_overflowed ? 1U : 0U, cmd.text_overflowed ? 1U : 0U,
            cmd.blob_overflowed ? 1U : 0U,
        };
        out.exec = {
            host_scene_evidence_size(exec.cmd_count), host_scene_evidence_size(exec.cmd_bytes),
            host_scene_evidence_size(exec.failed_cmds), host_scene_evidence_size(exec.clip_pushes),
            host_scene_evidence_size(exec.clip_pops), host_scene_evidence_size(exec.clip_push_overflow),
            host_scene_evidence_size(exec.clip_pop_underflow), host_scene_evidence_size(exec.clip_invalid),
            host_scene_evidence_size(exec.fail_blob), host_scene_evidence_size(exec.fail_path),
            host_scene_evidence_size(exec.fail_clip), host_scene_evidence_size(exec.fail_other),
            host_scene_evidence_size(exec.dispatch_groups), host_scene_evidence_size(exec.batch_flushes),
            host_scene_evidence_size(exec.group_rect), host_scene_evidence_size(exec.group_text),
            host_scene_evidence_size(exec.group_image), host_scene_evidence_size(exec.group_line),
            host_scene_evidence_size(exec.group_path), host_scene_evidence_size(exec.group_other),
            host_scene_evidence_size(exec.cmd_text), host_scene_evidence_size(exec.cmd_rect),
            host_scene_evidence_size(exec.cmd_image), host_scene_evidence_size(exec.cmd_line),
            host_scene_evidence_size(exec.cmd_path), host_scene_evidence_size(exec.cmd_other),
            host_scene_evidence_size(exec.fail_text), host_scene_evidence_size(exec.fail_image),
            host_scene_evidence_u32(exec.alpha_blend_count), exec.overflowed ? 1U : 0U,
        };
        out.layer = {
            layer.snapshot_count, layer.snapshot_rebuild_count, layer.stale_snapshot_count,
            layer.layer_bytes, layer.composite_pixels, layer.pixel_blit_count, layer.pixel_blit_pixels,
        };
        out.timing = {timing.available, timing.record_us, timing.execute_us, timing.render_us};
        out.draw_detail = ui::draw_cmd::make_draw_cmd_detail_evidence(scene.last_draw_detail_stats());
        return out;
    }
}

namespace player {
    class PlayerOwnedDisplayBuffer {
    public:
        static constexpr PlayerDisplayPixelFormat pixel_format = default_player_display_pixel_format;
        static constexpr std::size_t bytes_per_pixel = player_display_bytes_per_pixel(pixel_format);
        static constexpr std::size_t stride_bytes =
            static_cast<std::size_t>(screen_width) * bytes_per_pixel;
        static constexpr std::size_t buffer_bytes =
            stride_bytes * static_cast<std::size_t>(screen_height);

        PlayerDisplaySurface surface() noexcept {
            return PlayerDisplaySurface{
                storage_.data(),
                screen_width,
                screen_height,
                stride_bytes,
                pixel_format,
                PlayerDisplaySurfaceOwnership::Owned,
            };
        }

    private:
        alignas(4) std::array<std::byte, buffer_bytes> storage_{};
    };

    struct PlayerPlatform {
        explicit PlayerPlatform(PlayerDisplaySurface surface) noexcept
            : surface_storage(surface),
              canvas(surface_storage.pixels,
                     surface_storage.width,
                     surface_storage.height,
                     to_vivid_pixel_format(surface_storage.pixel_format),
                     surface_storage.stride_bytes),
              scene(canvas) {}

        template <typename Fn>
        void build_scene(Fn&& fn) {
            scene.build(std::forward<Fn>(fn));
        }

        void clear(const rgba& color) noexcept { canvas.clear(color); }
        void begin_frame() noexcept { canvas.begin_frame(); }
        void render() { scene.render(); }
        void end_frame() noexcept { canvas.end_frame(); }

        RuntimeCanvas& canvas_ref() noexcept { return canvas; }
        const RuntimeCanvas& canvas_ref() const noexcept { return canvas; }
        PlayerDisplaySurface& surface_ref() noexcept { return surface_storage; }
        const PlayerDisplaySurface& surface_ref() const noexcept { return surface_storage; }
        ::ui::scene::Scene& scene_ref() noexcept { return scene; }
        const ::ui::scene::Scene& scene_ref() const noexcept { return scene; }

        std::size_t stride_bytes() const noexcept { return surface_storage.stride_bytes; }
        int width() const noexcept { return surface_storage.width; }
        int height() const noexcept { return surface_storage.height; }

        rgba get_pixel(int x, int y) const noexcept { return canvas.get_pixel(x, y); }
        FrameBufferView framebuffer_view() noexcept { return to_framebuffer_view(surface_storage); }
        FrameBufferView framebuffer_view() const noexcept { return to_framebuffer_view(surface_storage); }

    private:
        PlayerDisplaySurface surface_storage{};
        RuntimeCanvas canvas;
        ::ui::scene::Scene scene;
    };

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

    template <typename Controller, typename Page, typename Platform>
    class PlayerRuntime {
    public:
        PlayerRuntime(charm::system::Clock& clock,
                      Platform& platform,
                      Controller& controller,
                      PlayerRuntimeConfig<Page> config)
            : clock_(&clock),
              platform_(&platform),
              controller_(&controller),
              config_(std::move(config)) {}

        App* app() noexcept { return app_ ? &(*app_) : nullptr; }
        const App* app() const noexcept { return app_ ? &(*app_) : nullptr; }
        Platform* platform() noexcept { return platform_; }
        const Platform* platform() const noexcept { return platform_; }
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
            app_.emplace(config_.app_config,
                         *clock_,
                         make_storage_binding(config_.storage_config));
            app_->bind_player(*controller_);
            if constexpr (requires { controller_->cover_resource_binding = active_cover_resource_binding_; }) {
                controller_->cover_resource_binding = active_cover_resource_binding_;
            }
            if constexpr (requires {
                controller_->bind_scene_runtime(make_player_scene_runtime(platform_->scene_ref()));
            }) {
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
            return app_->bootstrap_player(
                *controller_, config_.initial_track_index, config_.auto_start);
        }

        void tick(charm::system::ClockTick now_us, PlayerRuntimeHooks hooks = {}) {
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
            if (app_ && platform_ && controller_) {
                app_->dispatch_player_input(platform_->scene_ref(), *controller_, ev);
            }
        }

        void dispatch_raw_input(const input::RawInputEvent& ev) {
            if (app_ && platform_ && controller_) {
                app_->dispatch_raw_input(platform_->scene_ref(), *controller_, ev);
            }
        }

        bool render(PlayerDisplaySink* display_sink,
                    ::ui::scene::Scene::OverlayFn overlay_fn = nullptr,
                    void* overlay_ctx = nullptr) {
            if (!platform_ || !controller_) {
                return false;
            }
            platform_->clear(config_.clear_color);
            platform_->begin_frame();
            platform_->scene_ref().set_overlay(overlay_fn, overlay_ctx);
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            if (controller_->transition_needs_destination_snapshot()) {
                if (controller_->transition_destination_snapshot_ready_to_capture()) {
                    controller_->prepare_transition_destination_snapshot_scene();
                    platform_->render();
                    controller_->finish_transition_destination_snapshot_capture();
                    platform_->clear(config_.clear_color);
                    platform_->begin_frame();
                    platform_->scene_ref().set_overlay(overlay_fn, overlay_ctx);
                } else {
                    controller_->schedule_transition_destination_snapshot_capture();
                }
            }
            controller_->compose_now_playing_transition_pixel_layer();
#endif
            platform_->render();
            platform_->end_frame();
            if (display_sink) {
                return display_sink->present(
                    platform_->surface_ref(),
                    full_player_dirty_region(platform_->surface_ref()));
            }
            return true;
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
                .release_snapshot_fn = [](void* ctx, ::ui::scene::SnapshotHandle handle) noexcept {
                    return ctx && static_cast<::ui::scene::Scene*>(ctx)->release_snapshot(handle);
                },
                .mark_snapshot_stale_fn = [](void* ctx, ::ui::scene::SnapshotHandle handle) noexcept {
                    return ctx && static_cast<::ui::scene::Scene*>(ctx)->mark_snapshot_stale(handle);
                },
                .layer_stats_fn = [](void* ctx) noexcept -> ::ui::scene::LayerStats {
                    return ctx ? static_cast<::ui::scene::Scene*>(ctx)->layer_stats()
                               : ::ui::scene::LayerStats{};
                },
                .capture_snapshot_fn =
                    [](void* ctx, const ::ui::scene::SnapshotSpec& spec) noexcept
                        -> PlayerLayerCaptureResult {
                        if (!ctx) return {};
                        const auto capture =
                            (spec.preferred_kind == ::ui::scene::SnapshotKind::PixelSurface)
                            ? static_cast<::ui::scene::Scene*>(ctx)->capture_pixel_snapshot_result(spec)
                            : static_cast<::ui::scene::Scene*>(ctx)->capture_command_snapshot_result(spec);
                        return PlayerLayerCaptureResult{capture.status, capture.handle};
                    },
                .make_compose_plan_fn =
                    [](void* ctx, const ::ui::scene::LayerComposeSpec& spec) noexcept {
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
            active_cover_resource_binding_ = binding;
            set_cover_resource_provider_binding(binding);
            cover_resource_provider_installed_ = true;
        }

        void restore_cover_resource_provider_binding() noexcept {
            if (!cover_resource_provider_installed_) {
                return;
            }
            set_cover_resource_provider_binding(previous_cover_resource_provider_);
            previous_cover_resource_provider_ = {};
            active_cover_resource_binding_ = {};
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
        Platform* platform_{nullptr};
        Controller* controller_{nullptr};
        PlayerRuntimeConfig<Page> config_{};
        std::optional<App> app_{};
        PlayerCoverResourceProviderBinding previous_cover_resource_provider_{};
        PlayerCoverResourceProviderBinding active_cover_resource_binding_{};
        bool cover_resource_provider_installed_{false};
        float t_sec_{0.0f};
    };

    template <typename Controller, typename Page, typename Platform>
    struct HostPlayerBoardRuntimeStorage {
        PlayerBoardDisplaySinkState display_sink_state{};
        std::optional<Platform> platform{};
        std::optional<PlayerRuntime<Controller, Page, Platform>> runtime{};

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
    struct HostPlayerBoardRuntimeConfig {
        PlayerBoardPortConfig port{};
        StorageConfig storage_config{};
        Page start_page{};
        int initial_track_index{0};
        bool auto_start{false};
        rgba clear_color{0, 0, 0, 255};
    };

    template <typename Controller, typename Page, typename Platform>
    PlayerDisplaySink make_host_player_board_runtime(
        charm::system::Clock& clock,
        Controller& controller,
        const HostPlayerBoardRuntimeConfig<Page>& config,
        HostPlayerBoardRuntimeStorage<Controller, Page, Platform>& storage) {
        storage.reset();
        auto bindings = make_player_board_port_bindings(config.port, storage.display_sink_state);
        storage.platform.emplace(bindings.surface);
        storage.runtime.emplace(clock,
                                *storage.platform,
                                controller,
                                PlayerRuntimeConfig<Page>{
                                    .app_config = std::move(bindings.app_config),
                                    .storage_config = config.storage_config,
                                    .cover_resource_provider = config.port.cover_resource_provider,
                                    .cover_resource_records = config.port.cover_resource_records,
                                    .start_page = config.start_page,
                                    .initial_track_index = config.initial_track_index,
                                    .auto_start = config.auto_start,
                                    .clear_color = config.clear_color,
                                });
        return bindings.display_sink;
    }

    struct HostPlayerRuntimeMemoryProbeResult {
        bool bootstrap_ok{false};
        bool render_ok{false};
        int present_count{0};
        PlayerDirtyRegion dirty{};
        bool surface_valid{false};
        bool has_nonzero_pixel{false};
        bool root_bound{false};
        bool ok{false};
    };

    inline bool host_runtime_probe_has_nonzero_pixel(const PlayerDisplaySurface& surface) noexcept {
        if (!surface.valid()) {
            return false;
        }
        const std::size_t bpp = surface.bytes_per_pixel();
        if (bpp == 0) {
            return false;
        }
        for (int y = 0; y < surface.height; y += 32) {
            const auto* row = surface.pixels + static_cast<std::size_t>(y) * surface.stride_bytes;
            for (int x = 0; x < surface.width; x += 32) {
                const auto* px = row + static_cast<std::size_t>(x) * bpp;
                for (std::size_t i = 0; i < bpp; ++i) {
                    if (px[i] != std::byte{}) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    template <typename Platform, typename Controller>
    bool host_runtime_probe_root_bound(Platform& platform, Controller& controller) noexcept {
        if constexpr (requires { controller.handles.root; }) {
            return controller.handles.root && platform.scene_ref().root() == controller.handles.root;
        } else {
            return false;
        }
    }

    template <typename Controller, typename Page, typename Platform, typename SinkState>
    HostPlayerRuntimeMemoryProbeResult run_host_player_runtime_memory_probe(
        charm::system::Clock& clock,
        Platform& platform,
        Controller& controller,
        PlayerRuntimeConfig<Page> config,
        std::optional<PlayerRuntime<Controller, Page, Platform>>& runtime_storage,
        SinkState& sink_state,
        PlayerDisplaySink& display_sink,
        charm::system::ClockTick tick_us) {
        runtime_storage.emplace(clock, platform, controller, std::move(config));
        auto& runtime = *runtime_storage;
        HostPlayerRuntimeMemoryProbeResult out{};
        (void)runtime.bootstrap();
        out.bootstrap_ok = runtime.app() != nullptr;
        runtime.tick(tick_us);
        out.render_ok = runtime.render(&display_sink);

        const auto& presented = sink_state.last_surface;
        out.present_count = sink_state.present_count;
        out.dirty = sink_state.last_dirty;
        out.surface_valid = presented.valid();
        out.has_nonzero_pixel = host_runtime_probe_has_nonzero_pixel(presented);
        out.root_bound = host_runtime_probe_root_bound(platform, controller);
        out.ok = out.bootstrap_ok
            && out.render_ok
            && out.present_count == 1
            && out.surface_valid
            && out.has_nonzero_pixel
            && out.root_bound;

        runtime.shutdown();
        runtime_storage.reset();
        return out;
    }
}
