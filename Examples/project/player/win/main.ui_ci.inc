    struct UiCiResult {
        bool ok{true};
        int failed{0};
    };

    constexpr std::size_t kUiCmdBudget = 1200;
    constexpr std::uint64_t kUiAlphaBlendBudget = 1000000;
    constexpr std::uint32_t kUiLayerBytesBudget = 0;

    void ui_ci_emit(const char* name, bool ok, const char* reason) {
        if (ok) {
            std::printf("[ui-ci] case=%s ok=1\n", name);
        } else {
            std::printf("[ui-ci] case=%s ok=0 reason=%s\n", name, reason ? reason : "unknown");
        }
    }

    constexpr std::uint32_t ui_ci_cover_pixel(std::uint8_t a,
                                              std::uint8_t r,
                                              std::uint8_t g,
                                              std::uint8_t b) noexcept {
        return (static_cast<std::uint32_t>(b) << 24)
            | (static_cast<std::uint32_t>(g) << 16)
            | (static_cast<std::uint32_t>(r) << 8)
            | static_cast<std::uint32_t>(a);
    }

    bool ui_ci_static_cover_provider(const player::CoverResourceRequest& request,
                                     player::CoverResourceView& out) noexcept {
        static constexpr std::array<std::uint32_t, 4> kPixels{
            ui_ci_cover_pixel(255, 240, 120, 80),
            ui_ci_cover_pixel(255, 60, 110, 210),
            ui_ci_cover_pixel(255, 96, 180, 130),
            ui_ci_cover_pixel(255, 250, 220, 120),
        };
        if (request.kind != player::CoverResourceKind::EmbeddedTrack) return false;
        if (request.path != std::string_view{"/ui-ci/static-cover"}) return false;
        out.key = "/ui-ci/static-cover-resource";
        out.argb = std::span<const std::uint32_t>(kPixels.data(), kPixels.size());
        out.width = 2;
        out.height = 2;
        return true;
    }

    bool run_cover_resource_provider_ci() noexcept {
        const auto previous = player::cover_resource_provider();
        player::set_cover_resource_provider(&ui_ci_static_cover_provider);

        player::CoverImage image{};
        player::CoverResourceRequest request{};
        request.path = "/ui-ci/static-cover";
        request.kind = player::CoverResourceKind::EmbeddedTrack;
        request.fallback_variant = player::DefaultCoverVariant::HomeHeroPill;
        const bool loaded = player::load_cover_image(request, image);
        const bool ok = loaded
            && ::ui::gfx::image_id_valid(image.image_id)
            && image.width == 2
            && image.height == 2
            && image.argb.size() == 4
            && image.path == "/ui-ci/static-cover-resource";

        player::release_cover_image(image);
        player::set_cover_resource_provider(previous);
        return ok;
    }

    void ui_ci_pointer(PlayerRuntime& runtime,
                       std::uint32_t ms,
                       player::PlayerPointerAction action,
                       bool down,
                       int x,
                       int y) {
        runtime.dispatch_input(player::PlayerInputEvent::make_pointer(
            ms,
            action,
            player::PlayerPointerSample{
                down,
                static_cast<float>(x),
                static_cast<float>(y),
                0}));
    }

    void ui_ci_click(PlayerRuntime& runtime, int x, int y) {
        ui_ci_pointer(runtime, 0, player::PlayerPointerAction::Down, true, x, y);
        ui_ci_pointer(runtime, 1, player::PlayerPointerAction::Up, false, x, y);
    }

    void ui_ci_pump_frame(PlayerRuntime& runtime, player::PlayerDisplaySink* display_sink) {
        runtime.tick(g_clock.now_us());
        (void)runtime.render(display_sink);
    }

    struct RuntimeMemoryProbeHostStorage {
        static std::array<std::byte, player::PlayerOwnedDisplayBuffer::buffer_bytes> external_buffer;
        static player::PlayerPlatform memory_platform;
        static PlayerUiContext memory_ctx;
        static std::optional<PlayerRuntime> memory_runtime;
    };

    std::array<std::byte, player::PlayerOwnedDisplayBuffer::buffer_bytes> RuntimeMemoryProbeHostStorage::external_buffer{};
    player::PlayerPlatform RuntimeMemoryProbeHostStorage::memory_platform{player::PlayerDisplaySurface{
        RuntimeMemoryProbeHostStorage::external_buffer.data(),
        screen_width,
        screen_height,
        player::PlayerOwnedDisplayBuffer::stride_bytes,
        player::PlayerOwnedDisplayBuffer::pixel_format,
        player::PlayerDisplaySurfaceOwnership::Borrowed,
    }};
    PlayerUiContext RuntimeMemoryProbeHostStorage::memory_ctx{};
    std::optional<PlayerRuntime> RuntimeMemoryProbeHostStorage::memory_runtime{};

    player::PlayerRuntimeConfig<player::PlayerPage> make_runtime_probe_config(const PreviewOptions& options) {
        RuntimeMemoryProbeHostStorage::memory_ctx = PlayerUiContext{};
        return player::PlayerRuntimeConfig<player::PlayerPage>{
            .app_config = make_app_config(options),
            .storage_config = player::default_storage_config(),
            .start_page = options.start_page,
            .initial_track_index = options.track_index_override,
            .auto_start = false,
            .clear_color = kUiBackground,
        };
    }

    bool runtime_probe_result_matches_external_surface(const player::PlayerRuntimeMemoryProbeResult& result,
                                                       const player::PlayerDisplaySurface& presented) noexcept {
        return result.ok
            && presented.pixels == RuntimeMemoryProbeHostStorage::external_buffer.data()
            && presented.width == screen_width
            && presented.height == screen_height
            && presented.stride_bytes == player::PlayerOwnedDisplayBuffer::stride_bytes
            && presented.pixel_format == player::PlayerOwnedDisplayBuffer::pixel_format
            && result.dirty.w == screen_width
            && result.dirty.h == screen_height;
    }

    player::PlayerRuntimeMemoryProbeResult run_memory_display_sink_probe(const PreviewOptions& options = PreviewOptions{}) {
        auto memory_cfg = make_runtime_probe_config(options);
        player::MemoryDisplaySinkState sink_state{};
        player::PlayerDisplaySink sink = player::make_memory_display_sink(sink_state);
        auto result = player::run_player_runtime_memory_probe(g_clock,
                                                              RuntimeMemoryProbeHostStorage::memory_platform,
                                                              RuntimeMemoryProbeHostStorage::memory_ctx,
                                                              std::move(memory_cfg),
                                                              RuntimeMemoryProbeHostStorage::memory_runtime,
                                                              sink_state,
                                                              sink,
                                                              g_clock.now_us());
        result.ok = runtime_probe_result_matches_external_surface(result, sink_state.last_surface);
        return result;
    }

    struct UiCiBoardDisplayCallbackState {
        int clean_cache_count{0};
        int flush_dirty_count{0};
        int present_count{0};
        player::PlayerDirtyRegion last_dirty{};
    };

    bool ui_ci_board_clean_cache(void* ctx,
                                 const player::PlayerDisplaySurface&,
                                 player::PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<UiCiBoardDisplayCallbackState*>(ctx);
        if (!state || dirty.empty()) return false;
        state->clean_cache_count += 1;
        state->last_dirty = dirty;
        return true;
    }

    bool ui_ci_board_flush_dirty(void* ctx,
                                 const player::PlayerDisplaySurface&,
                                 player::PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<UiCiBoardDisplayCallbackState*>(ctx);
        if (!state || dirty.empty()) return false;
        state->flush_dirty_count += 1;
        state->last_dirty = dirty;
        return true;
    }

    bool ui_ci_board_present(void* ctx,
                             const player::PlayerDisplaySurface&,
                             player::PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<UiCiBoardDisplayCallbackState*>(ctx);
        if (!state || dirty.empty()) return false;
        state->present_count += 1;
        state->last_dirty = dirty;
        return true;
    }

    player::PlayerRuntimeMemoryProbeResult run_board_display_sink_probe(const PreviewOptions& options = PreviewOptions{}) {
        auto memory_cfg = make_runtime_probe_config(options);
        UiCiBoardDisplayCallbackState callback_state{};
        player::PlayerBoardDisplaySinkState sink_state{};
        player::PlayerDisplaySink sink = player::make_board_display_sink(
            sink_state,
            player::PlayerBoardDisplayCallbacks{
                .ctx = &callback_state,
                .clean_cache = &ui_ci_board_clean_cache,
                .flush_dirty = &ui_ci_board_flush_dirty,
                .present = &ui_ci_board_present,
            });
        auto result = player::run_player_runtime_memory_probe(g_clock,
                                                              RuntimeMemoryProbeHostStorage::memory_platform,
                                                              RuntimeMemoryProbeHostStorage::memory_ctx,
                                                              std::move(memory_cfg),
                                                              RuntimeMemoryProbeHostStorage::memory_runtime,
                                                              sink_state,
                                                              sink,
                                                              g_clock.now_us());
        result.ok = runtime_probe_result_matches_external_surface(result, sink_state.last_surface)
            && sink_state.last_clean_cache_ok
            && sink_state.last_flush_dirty_ok
            && sink_state.last_present_ok
            && callback_state.clean_cache_count == 1
            && callback_state.flush_dirty_count == 1
            && callback_state.present_count == 1
            && callback_state.last_dirty.w == screen_width
            && callback_state.last_dirty.h == screen_height;
        return result;
    }

    int run_runtime_memory_smoke(const PreviewOptions& options) noexcept {
        player::ui::set_player_system_font_fallback_enabled(!options.disable_system_font_fallback);
        const auto result = run_memory_display_sink_probe(options);
        const bool ok = result.ok;
        std::printf("[runtime-memory-smoke] done ok=%d failed=%d\n", ok ? 1 : 0, ok ? 0 : 1);
        return ok ? 0 : 2;
    }

    UiCiResult run_ui_ci(PlayerRuntime& runtime) {
        UiCiResult res{};
        auto& ctx = *runtime.controller();
        auto& scene = runtime.scene_ref();

        auto pump_frame = [&]() {
            ui_ci_pump_frame(runtime, nullptr);
        };
        auto wait_for_page = [&](player::PlayerPage expected_page, int max_frames = 36) {
            if (ctx.current_page == expected_page) return true;
            for (int i = 0; i < max_frames; ++i) {
                SDL_Delay(16);
                pump_frame();
                if (ctx.current_page == expected_page) {
                    return true;
                }
            }
            return ctx.current_page == expected_page;
        };
        auto settle_now_playing_transition = [&](player::PlayerPage expected_page) {
            if (!ctx.now_playing_transition.active) {
                return ctx.current_page == expected_page;
            }
            ctx.finish_now_playing_transition();
            pump_frame();
            return ctx.current_page == expected_page;
        };

        pump_frame();

        if (run_memory_display_sink_probe().ok) {
            ui_ci_emit("memory_display_sink_player_ui_external_surface", true, nullptr);
        } else {
            ui_ci_emit("memory_display_sink_player_ui_external_surface", false, "memory_display_sink");
            res.ok = false;
            res.failed++;
        }
        if (run_board_display_sink_probe().ok) {
            ui_ci_emit("board_display_sink_player_ui_external_surface", true, nullptr);
        } else {
            ui_ci_emit("board_display_sink_player_ui_external_surface", false, "board_display_sink");
            res.ok = false;
            res.failed++;
        }
        pump_frame();

        if (run_cover_resource_provider_ci()) {
            ui_ci_emit("cover_resource_provider_static_view", true, nullptr);
        } else {
            ui_ci_emit("cover_resource_provider_static_view", false, "static_view");
            res.ok = false;
            res.failed++;
        }

        {
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const Rect r = scene.world_rect(ctx.handles.bottom_hit);
            if (r.w > 0 && r.h > 0) {
                const int cx = r.x + r.w / 2;
                const int cy = r.y + r.h / 2;
                ui_ci_pointer(runtime, 10, player::PlayerPointerAction::Down, true, cx, cy);
                ui_ci_pointer(runtime, 11, player::PlayerPointerAction::Move, true, cx + 3, cy + 2);
                ui_ci_pointer(runtime, 12, player::PlayerPointerAction::Up, false, cx + 3, cy + 2);
                if (wait_for_page(player::PlayerPage::NowPlaying)
                    || settle_now_playing_transition(player::PlayerPage::NowPlaying)) {
                    ui_ci_emit("input_boundary_touch_home_to_now", true, nullptr);
                } else {
                    ui_ci_emit("input_boundary_touch_home_to_now", false, "page_not_now");
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("input_boundary_touch_home_to_now", false, "bottom_hit_zero");
                res.ok = false;
                res.failed++;
            }
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
        }

        {
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const Rect r = scene.world_rect(ctx.handles.home_scroll);
            const int before_scroll = ctx.access.scroll_y(ctx.handles.home_scroll);
            if (r.w > 0 && r.h > 0) {
                runtime.dispatch_input(player::PlayerInputEvent::make_wheel(
                    20,
                    static_cast<float>(r.x + r.w / 2),
                    static_cast<float>(r.y + r.h / 2),
                    -3.0f));
                pump_frame();
                const int after_scroll = ctx.access.scroll_y(ctx.handles.home_scroll);
                if (after_scroll != before_scroll) {
                    ui_ci_emit("input_boundary_wheel_home_scroll", true, nullptr);
                } else {
                    ui_ci_emit("input_boundary_wheel_home_scroll", false, "scroll_unchanged");
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("input_boundary_wheel_home_scroll", false, "home_scroll_zero");
                res.ok = false;
                res.failed++;
            }
        }

        {
            const auto cmd_stats = scene.last_cmd_stats();
            const auto exec_stats = scene.last_exec_stats();
            const auto layer_stats = scene.layer_stats();
            if (cmd_stats.cmd_count > kUiCmdBudget) {
                ui_ci_emit("frame_budget_cmd", false, "cmd_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_cmd", true, nullptr);
            }
            if (exec_stats.alpha_blend_count > kUiAlphaBlendBudget) {
                ui_ci_emit("frame_budget_alpha", false, "alpha_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_alpha", true, nullptr);
            }
            if (layer_stats.layer_bytes > kUiLayerBytesBudget) {
                ui_ci_emit("frame_budget_layer", false, "layer_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_layer", true, nullptr);
            }
        }
        {
            const auto epoch = scene.current_layer_epoch();
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 16, 16},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.reserve_snapshot(spec);
            const auto* record = scene.snapshot_record(handle);
            const bool reserved = static_cast<bool>(handle)
                && record
                && record->epoch == epoch
                && scene.update_command_snapshot(handle)
                && scene.mark_snapshot_stale(handle)
                && scene.refresh_snapshot_epoch(handle)
                && scene.release_snapshot(handle)
                && !scene.snapshot_record(handle);
            if (reserved && scene.layer_stats().snapshot_count == 0) {
                ui_ci_emit("layer_snapshot_lifecycle", true, nullptr);
            } else {
                ui_ci_emit("layer_snapshot_lifecycle", false, "snapshot_lifecycle");
                res.ok = false;
                res.failed++;
            }
        }
        {
            auto& layer = ctx.page_home_layer;
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 24, 24},
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto freeze = layer.freeze(scene, scene.access(), spec, true);
            const bool frozen = freeze.ok()
                && freeze.handle
                && layer.snapshot() == freeze.handle
                && layer.state() == ::ui::scene::PageLayerState::Frozen
                && !layer.visible();
            layer.mark_transitioning();
            const bool transitioning = frozen
                && layer.state() == ::ui::scene::PageLayerState::Transitioning;
            layer.thaw(scene, scene.access(), true);
            const bool thawed = transitioning
                && layer.live()
                && layer.visible()
                && !layer.snapshot()
                && scene.layer_stats().snapshot_count == 0;
            if (thawed) {
                ui_ci_emit("page_layer_freeze_thaw", true, nullptr);
            } else {
                ui_ci_emit("page_layer_freeze_thaw", false, "page_layer_freeze_thaw");
                if (layer.snapshot()) {
                    (void)layer.release_snapshot(scene);
                }
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto capture = scene.capture_command_snapshot_result(spec);
            const auto handle = capture.handle;
            const auto* record = scene.snapshot_record(handle);
            const bool captured = capture.ok()
                && static_cast<bool>(handle)
                && record
                && record->kind == ::ui::scene::SnapshotKind::CommandBuffer
                && record->command_count > 0
                && record->bytes > 0
                && scene.layer_stats().snapshot_count == 1
                && scene.release_snapshot(handle)
                && scene.layer_stats().snapshot_count == 0;
            if (captured) {
                ui_ci_emit("layer_command_snapshot_capture", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_capture", false, "command_snapshot_capture");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            std::array<::ui::scene::SnapshotHandle, static_cast<std::size_t>(layer_cache_slots)> handles{};
            bool filled = true;
            for (auto& handle : handles) {
                const auto capture = scene.capture_command_snapshot_result(spec);
                if (!capture.ok() || !capture.handle) {
                    filled = false;
                    break;
                }
                handle = capture.handle;
            }
            const auto extra = scene.capture_command_snapshot_result(spec);
            bool released_all = true;
            for (auto handle : handles) {
                if (handle && !scene.release_snapshot(handle)) {
                    released_all = false;
                }
            }
            const bool exhausted = filled
                && extra.status == ::ui::scene::LayerCaptureStatus::NoSnapshotSlot
                && !extra.handle
                && released_all;
            if (exhausted) {
                ui_ci_emit("layer_command_snapshot_capture_full", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_capture_full", false, "command_snapshot_capture_full");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect root_rect = scene.world_rect(ctx.handles.root);
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 16, 16},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            bool stale_checked = static_cast<bool>(handle) && scene.snapshot_current(handle);
            if (ctx.handles.root) {
                scene.access().set_rect(ctx.handles.root, root_rect);
            }
            stale_checked = stale_checked
                && !scene.validate_snapshot(handle)
                && scene.snapshot_record(handle)
                && scene.snapshot_record(handle)->stale
                && scene.release_snapshot(handle);
            if (stale_checked) {
                ui_ci_emit("layer_snapshot_stale_epoch", true, nullptr);
            } else {
                ui_ci_emit("layer_snapshot_stale_epoch", false, "stale_epoch");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{4, 8, 20, 10},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto result = scene.compose_snapshot_dry_run({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{.x = 6, .y = 2, .opacity = 255},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto stats = scene.layer_stats();
            const bool composed = result.ok
                && result.kind == ::ui::scene::SnapshotKind::CommandBuffer
                && result.target_bounds.x == 10
                && result.target_bounds.y == 10
                && result.composite_pixels == 200
                && stats.composite_pixels >= result.composite_pixels
                && scene.release_snapshot(handle);
            if (composed) {
                ui_ci_emit("layer_compose_dry_run", true, nullptr);
            } else {
                ui_ci_emit("layer_compose_dry_run", false, "compose_dry_run");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{4, 8, 20, 10},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{.x = -10, .y = -3, .opacity = 192},
                .clip = Rect{0, 0, 12, 8},
                .has_clip = true,
            });
            const bool planned = plan.valid
                && plan.source_visible.x == 10
                && plan.source_visible.y == 8
                && plan.source_visible.w == 12
                && plan.source_visible.h == 3
                && plan.target_bounds.x == 0
                && plan.target_bounds.y == 5
                && plan.target_bounds.w == 12
                && plan.target_bounds.h == 3
                && plan.composite_pixels == 36;
            const bool released = scene.release_snapshot(handle);
            const bool ok = planned && released;
            if (ok) {
                ui_ci_emit("layer_compose_plan_clip", true, nullptr);
            } else {
                ui_ci_emit("layer_compose_plan_clip", false, "compose_plan_clip");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 40, 20},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{},
                .clip = Rect{0, 0, 40, 20},
                .has_clip = true,
            });
            const auto generous = scene.check_layer_budget(plan, {
                .max_layer_bytes = 2000000,
                .max_composite_pixels = 1000,
                .max_command_count = 2000,
            });
            const auto strict = scene.check_layer_budget(plan, {
                .max_layer_bytes = 1,
                .max_composite_pixels = 1,
                .max_command_count = 1,
            });
            const bool budget_ok = plan.valid
                && generous.ok
                && !generous.layer_bytes_over
                && !generous.composite_pixels_over
                && !generous.command_count_over
                && !strict.ok
                && strict.layer_bytes_over
                && strict.composite_pixels_over
                && strict.command_count_over;
            const bool released = scene.release_snapshot(handle);
            if (budget_ok && released) {
                ui_ci_emit("layer_compose_budget", true, nullptr);
            } else {
                ui_ci_emit("layer_compose_budget", false, "compose_budget");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto replay = scene.replay_command_snapshot(plan);
            const bool replayed = plan.valid
                && replay.ok()
                && replay.stats.cmd_count > 0
                && replay.stats.cmd_bytes > 0
                && replay.stats.failed_cmds == 0;
            const bool released = scene.release_snapshot(handle);
            if (replayed && released) {
                ui_ci_emit("layer_command_snapshot_replay", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_replay", false, "command_snapshot_replay");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect root_rect = scene.world_rect(ctx.handles.root);
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            if (ctx.handles.root) {
                scene.access().set_rect(ctx.handles.root, root_rect);
            }
            const auto replay = scene.replay_command_snapshot(plan);
            const auto* record = scene.snapshot_record(handle);
            const bool stale_rejected = plan.valid
                && replay.status == ::ui::scene::LayerReplayStatus::StaleSnapshot
                && record
                && record->stale;
            const bool released = scene.release_snapshot(handle);
            if (stale_rejected && released) {
                ui_ci_emit("layer_command_snapshot_replay_stale", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_replay_stale", false, "command_snapshot_replay_stale");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto first = scene.capture_command_snapshot(spec);
            const auto* first_record = scene.snapshot_record(first);
            const auto first_payload = first_record
                ? first_record->payload_slot
                : ::ui::scene::kInvalidSnapshotPayloadSlot;
            const bool first_released = scene.release_snapshot(first);
            const auto second = scene.capture_command_snapshot(spec);
            const auto* second_record = scene.snapshot_record(second);
            const auto second_payload = second_record
                ? second_record->payload_slot
                : ::ui::scene::kInvalidSnapshotPayloadSlot;
            const bool reused = first_released
                && first_payload != ::ui::scene::kInvalidSnapshotPayloadSlot
                && second_payload == first_payload
                && scene.release_snapshot(second);
            if (reused) {
                ui_ci_emit("layer_command_snapshot_payload_reuse", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_payload_reuse", false, "command_snapshot_payload_reuse");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect pixel_bounds{0, 0, 48, 32};
            const ::ui::scene::SnapshotSpec spec{
                .bounds = pixel_bounds,
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto capture = scene.capture_pixel_snapshot_result(spec);
            const auto* record = scene.snapshot_record(capture.handle);
            const bool captured = capture.ok()
                && record
                && record->kind == ::ui::scene::SnapshotKind::PixelSurface
                && record->bytes == ::ui::scene::snapshot_pixel_bytes(
                    ::screen_pixel_format, pixel_bounds.w, pixel_bounds.h);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = capture.handle,
                .transform = ::ui::scene::LayerTransform{.x = 8, .y = 6, .opacity = 255},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto before = scene.layer_stats();
            const auto replay = scene.compose_pixel_snapshot(plan);
            const auto after = scene.layer_stats();
            const bool composed = plan.valid
                && replay.ok()
                && replay.kind == ::ui::scene::SnapshotKind::PixelSurface
                && after.pixel_blit_count > before.pixel_blit_count
                && after.pixel_blit_pixels >= before.pixel_blit_pixels + plan.composite_pixels;
            const bool released = scene.release_snapshot(capture.handle);
            if (captured && composed && released) {
                ui_ci_emit("layer_pixel_snapshot_capture_compose", true, nullptr);
            } else {
                ui_ci_emit("layer_pixel_snapshot_capture_compose", false, "pixel_snapshot_capture_compose");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect pixel_bounds{0, 0, 24, 16};
            const ::ui::scene::SnapshotSpec spec{
                .bounds = pixel_bounds,
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto capture = scene.capture_pixel_snapshot_result(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = capture.handle,
                .transform = ::ui::scene::LayerTransform{.x = 3, .y = 4, .opacity = 128},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto before = scene.layer_stats();
            const auto replay = scene.compose_pixel_snapshot(plan);
            const auto after = scene.layer_stats();
            const bool composed = capture.ok()
                && plan.valid
                && replay.ok()
                && replay.stats.alpha_blend_count == plan.composite_pixels
                && after.pixel_blit_count > before.pixel_blit_count
                && after.pixel_blit_pixels >= before.pixel_blit_pixels + plan.composite_pixels;
            const bool released = scene.release_snapshot(capture.handle);
            if (composed && released) {
                ui_ci_emit("layer_pixel_snapshot_opacity_compose", true, nullptr);
            } else {
                ui_ci_emit("layer_pixel_snapshot_opacity_compose", false, "pixel_snapshot_opacity_compose");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::LayerBudgetResult budget_over{
                .ok = false,
                .layer_bytes_over = true,
            };
            const auto decision = ::ui::scene::decide_layer_profile(
                ::ui::scene::LayerProfile::Rich, budget_over);
            const ::ui::scene::LayerBudgetResult composite_over{
                .ok = false,
                .composite_pixels_over = true,
            };
            const auto composite_decision = ::ui::scene::decide_layer_profile(
                ::ui::scene::LayerProfile::Cheap, composite_over);
            const auto pixel_bytes = static_cast<std::uint32_t>(
                ::ui::scene::snapshot_pixel_bytes(screen_pixel_format, screen_width, screen_height));
            const auto rich_admission = ::ui::scene::decide_layer_admission({
                .profile = ::ui::scene::LayerProfile::Rich,
                .budget = ::ui::scene::LayerBudget{
                    .max_layer_bytes = pixel_bytes * 2u,
                    .max_composite_pixels = 0,
                    .max_command_count = 0,
                },
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
            const auto low_budget_admission = ::ui::scene::decide_layer_admission({
                .profile = ::ui::scene::LayerProfile::Cheap,
                .budget = ::ui::scene::LayerBudget{
                    .max_layer_bytes = 1u,
                    .max_composite_pixels = 0,
                    .max_command_count = 0,
                },
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
            const auto static_admission = ::ui::scene::decide_layer_admission({
                .profile = ::ui::scene::LayerProfile::Static,
                .budget = {},
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
            const bool ok = ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Rich, 128) == 128
                && ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Cheap, 128) == 170
                && ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Static, 127) == 0
                && ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Static, 128) == 255
                && decision.requested == ::ui::scene::LayerProfile::Rich
                && decision.effective == ::ui::scene::LayerProfile::Cheap
                && decision.reason == ::ui::scene::LayerFallbackReason::LayerBytesOver
                && composite_decision.requested == ::ui::scene::LayerProfile::Cheap
                && composite_decision.effective == ::ui::scene::LayerProfile::Static
                && composite_decision.reason == ::ui::scene::LayerFallbackReason::CompositePixelsOver
                && rich_admission == ::ui::scene::LayerAdmission::PixelDouble
                && low_budget_admission == ::ui::scene::LayerAdmission::CommandSnapshot
                && static_admission == ::ui::scene::LayerAdmission::StaticCut;
            if (ok) {
                ui_ci_emit("layer_profile_decision", true, nullptr);
            } else {
                ui_ci_emit("layer_profile_decision", false, "layer_profile_decision");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto capture = scene.capture_pixel_snapshot_result(spec);
            const bool cache_can_fit_fullscreen =
                layer_cache_width >= screen_width && layer_cache_height >= screen_height;
            const bool ok = cache_can_fit_fullscreen
                ? (capture.ok() && capture.handle && scene.release_snapshot(capture.handle))
                : (capture.status == ::ui::scene::LayerCaptureStatus::StoreFailed
                    && !capture.handle);
            if (ok) {
                ui_ci_emit("layer_pixel_snapshot_fullscreen_profile", true, nullptr);
            } else {
                ui_ci_emit("layer_pixel_snapshot_fullscreen_profile", false, "pixel_snapshot_fullscreen_profile");
                if (capture.handle) {
                    (void)scene.release_snapshot(capture.handle);
                }
                res.ok = false;
                res.failed++;
            }
        }

        auto click_handle = [&](WidgetHandle h, const char* case_name) -> bool {
            if (!h) {
                ui_ci_emit(case_name, false, "invalid_handle");
                res.ok = false;
                res.failed++;
                return false;
            }
            const Rect r = scene.world_rect(h);
            if (r.w <= 0 || r.h <= 0) {
                ui_ci_emit(case_name, false, "zero_rect");
                res.ok = false;
                res.failed++;
                return false;
            }
            const int cx = r.x + r.w / 2;
            const int cy = r.y + r.h / 2;
            ui_ci_click(runtime, cx, cy);
            return true;
        };

        ctx.set_page(player::PlayerPage::Library);
        pump_frame();
        if (click_handle(ctx.handles.nav_home, "library_to_home")) {
            if (wait_for_page(player::PlayerPage::Home)) {
                ui_ci_emit("library_to_home", true, nullptr);
            } else {
                ui_ci_emit("library_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        const std::uint32_t captures_before_now = ctx.layer_transition_capture_count;
        const std::uint32_t releases_before_now = ctx.layer_transition_release_count;
        const std::uint32_t capture_fails_before_now = ctx.layer_transition_capture_fail_count;
        const std::uint32_t composes_before_now = ctx.layer_transition_compose_count;
        const std::uint32_t compose_pixels_before_now = ctx.layer_transition_composite_pixels;
        const std::uint32_t pixel_composes_before_now = ctx.layer_transition_pixel_compose_count;
        const std::uint32_t pixel_compose_pixels_before_now = ctx.layer_transition_pixel_compose_pixels;
        const std::uint32_t destination_captures_before_now = ctx.layer_transition_destination_capture_count;
        const std::uint32_t destination_composes_before_now = ctx.layer_transition_destination_compose_count;
        const std::uint32_t destination_compose_pixels_before_now =
            ctx.layer_transition_destination_compose_pixels;
        if (click_handle(ctx.handles.bottom_hit, "home_to_now")) {
            if (wait_for_page(player::PlayerPage::NowPlaying)
                || settle_now_playing_transition(player::PlayerPage::NowPlaying)) {
                ui_ci_emit("home_to_now", true, nullptr);
            } else {
                ui_ci_emit("home_to_now", false, "page_not_now");
                res.ok = false;
                res.failed++;
            }
        }
        if (ctx.layer_transition_capture_count > captures_before_now
            && ctx.layer_transition_release_count > releases_before_now
            && ctx.layer_transition_capture_fail_count == capture_fails_before_now
            && ctx.last_layer_transition_capture_status == ::ui::scene::LayerCaptureStatus::Ok) {
            ui_ci_emit("layer_transition_home_to_now_snapshot", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_home_to_now_snapshot", false, "transition_snapshot");
            res.ok = false;
            res.failed++;
        }
        if (ctx.layer_transition_compose_count > composes_before_now
            && ctx.layer_transition_composite_pixels > compose_pixels_before_now
            && ctx.layer_transition_pixel_compose_count > pixel_composes_before_now
            && ctx.layer_transition_pixel_compose_pixels > pixel_compose_pixels_before_now
            && ctx.layer_transition_destination_capture_count > destination_captures_before_now
            && ctx.layer_transition_destination_compose_count > destination_composes_before_now
            && ctx.layer_transition_destination_compose_pixels > destination_compose_pixels_before_now
            && ctx.effective_layer_profile == ::ui::scene::LayerProfile::Rich
            && ctx.layer_transition_fallback_reason == ::ui::scene::LayerFallbackReason::None
            && ctx.layer_transition_last_budget_ok
            && ctx.layer_transition_last_layer_bytes > 0
            && ctx.layer_transition_last_layer_bytes_budget > 0
            && ctx.layer_transition_last_composite_pixels > 0
            && ctx.layer_transition_last_composite_pixels_budget > 0) {
            ui_ci_emit("layer_transition_home_to_now_compose", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_home_to_now_compose", false, "transition_compose");
            res.ok = false;
            res.failed++;
        }

        pump_frame();
        const std::uint32_t captures_before_back = ctx.layer_transition_capture_count;
        const std::uint32_t releases_before_back = ctx.layer_transition_release_count;
        const std::uint32_t capture_fails_before_back = ctx.layer_transition_capture_fail_count;
        const std::uint32_t composes_before_back = ctx.layer_transition_compose_count;
        const std::uint32_t compose_pixels_before_back = ctx.layer_transition_composite_pixels;
        const std::uint32_t pixel_composes_before_back = ctx.layer_transition_pixel_compose_count;
        const std::uint32_t pixel_compose_pixels_before_back = ctx.layer_transition_pixel_compose_pixels;
        const std::uint32_t destination_captures_before_back = ctx.layer_transition_destination_capture_count;
        const std::uint32_t destination_composes_before_back = ctx.layer_transition_destination_compose_count;
        const std::uint32_t destination_compose_pixels_before_back =
            ctx.layer_transition_destination_compose_pixels;
        if (click_handle(ctx.handles.now_back, "now_back_to_home")) {
            if (wait_for_page(player::PlayerPage::Home)
                || settle_now_playing_transition(player::PlayerPage::Home)) {
                ui_ci_emit("now_back_to_home", true, nullptr);
            } else {
                ui_ci_emit("now_back_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }
        if (ctx.layer_transition_capture_count > captures_before_back
            && ctx.layer_transition_release_count > releases_before_back
            && ctx.layer_transition_capture_fail_count == capture_fails_before_back
            && ctx.last_layer_transition_capture_status == ::ui::scene::LayerCaptureStatus::Ok) {
            ui_ci_emit("layer_transition_now_to_home_snapshot", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_now_to_home_snapshot", false, "transition_snapshot");
            res.ok = false;
            res.failed++;
        }
        if (ctx.layer_transition_compose_count > composes_before_back
            && ctx.layer_transition_composite_pixels > compose_pixels_before_back
            && ctx.layer_transition_pixel_compose_count > pixel_composes_before_back
            && ctx.layer_transition_pixel_compose_pixels > pixel_compose_pixels_before_back
            && ctx.layer_transition_destination_capture_count > destination_captures_before_back
            && ctx.layer_transition_destination_compose_count > destination_composes_before_back
            && ctx.layer_transition_destination_compose_pixels > destination_compose_pixels_before_back) {
            ui_ci_emit("layer_transition_now_to_home_compose", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_now_to_home_compose", false, "transition_compose");
            res.ok = false;
            res.failed++;
        }

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        const auto profile_before_drill = ctx.requested_layer_profile;
        const bool drill_before = ctx.layer_profile_budget_drill_enabled;
        const std::uint32_t static_cuts_before = ctx.layer_static_cut_count;
        const std::uint32_t admission_static_cuts_before = ctx.layer_admission_static_cut_count;
        const std::uint32_t budget_fails_before = ctx.layer_transition_budget_fail_count;
        const std::uint32_t captures_before_admission_drill = ctx.layer_transition_capture_count;
        const std::uint32_t destination_captures_before_admission_drill =
            ctx.layer_transition_destination_capture_count;
        ctx.requested_layer_profile = ::ui::scene::LayerProfile::Cheap;
        ctx.layer_profile_budget_drill_enabled = true;
        if (click_handle(ctx.handles.bottom_hit, "home_to_now_static_cut")) {
            if (wait_for_page(player::PlayerPage::NowPlaying)
                || settle_now_playing_transition(player::PlayerPage::NowPlaying)) {
                ui_ci_emit("home_to_now_static_cut", true, nullptr);
            } else {
                ui_ci_emit("home_to_now_static_cut", false, "page_not_now");
                res.ok = false;
                res.failed++;
            }
        }
        const bool static_cut = ctx.current_page == player::PlayerPage::NowPlaying
            && !ctx.now_playing_transition.active
            && ctx.page_transition_state == player::PlayerController::PageTransitionState::Idle
            && ctx.effective_layer_profile == ::ui::scene::LayerProfile::Static
            && ctx.layer_transition_fallback_reason != ::ui::scene::LayerFallbackReason::None
            && !ctx.layer_transition_last_budget_ok
            && ctx.last_layer_admission == ::ui::scene::LayerAdmission::CommandSnapshot
            && ctx.layer_static_cut_count > static_cuts_before
            && ctx.layer_admission_static_cut_count > admission_static_cuts_before
            && ctx.layer_transition_budget_fail_count > budget_fails_before
            && ctx.layer_transition_capture_count == captures_before_admission_drill
            && ctx.layer_transition_destination_capture_count == destination_captures_before_admission_drill
            && scene.layer_stats().snapshot_count == 0;
        if (static_cut) {
            ui_ci_emit("layer_transition_static_cut_drill", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_static_cut_drill", false, "static_cut_drill");
            res.ok = false;
            res.failed++;
        }
        ctx.requested_layer_profile = profile_before_drill;
        ctx.layer_profile_budget_drill_enabled = drill_before;
        ctx.set_page(player::PlayerPage::Home);
        pump_frame();

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        const std::uint32_t aborts_before_interrupt = ctx.layer_transition_abort_count;
        const std::uint32_t releases_before_interrupt = ctx.layer_transition_release_count;
        const auto layer_count_before_interrupt = scene.layer_stats().snapshot_count;
        bool interrupt_started = false;
        if (ctx.begin_now_playing_expand_transition()) {
            interrupt_started = ctx.now_playing_transition.active
                && ctx.now_playing_transition.source_snapshot
                && scene.layer_stats().snapshot_count > layer_count_before_interrupt;
            ctx.set_page(player::PlayerPage::Library);
            pump_frame();
        }
        const bool interrupted = interrupt_started
            && !ctx.now_playing_transition.active
            && ctx.page_transition_state == player::PlayerController::PageTransitionState::Idle
            && ctx.layer_transition_abort_count > aborts_before_interrupt
            && ctx.layer_transition_release_count > releases_before_interrupt
            && scene.layer_stats().snapshot_count == 0
            && ctx.current_page == player::PlayerPage::Library
            && ctx.page_library_layer.visible()
            && !ctx.page_home_layer.snapshot()
            && !ctx.page_now_layer.snapshot()
            && !ctx.page_library_layer.snapshot();
        if (interrupted) {
            ui_ci_emit("layer_transition_interrupt_abort", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_interrupt_abort", false, "transition_interrupt_abort");
            res.ok = false;
            res.failed++;
        }

        const auto* tracks = ctx.storage.tracks;
        if (tracks && tracks->size() > 0) {
            ctx.set_page(player::PlayerPage::Library);
            pump_frame();
            const Rect list = scene.world_rect(ctx.handles.list);
            if (list.w > 0 && list.h > 0) {
                const int before = ctx.last_list_selected;
                ui_ci_click(runtime, list.x + 12, list.y + 12);
                pump_frame();
                const int after = ctx.last_list_selected;
                if (after >= 0) {
                    ui_ci_emit("list_select", true, nullptr);
                } else {
                    const char* reason = (before == after) ? "no_change" : "no_select";
                    ui_ci_emit("list_select", false, reason);
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("list_select", false, "list_rect_zero");
                res.ok = false;
                res.failed++;
            }
        } else {
            ui_ci_emit("list_select", true, "skipped_no_tracks");
        }

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    int run_ui_ci_preview(SdlHostRuntime& runtime) noexcept {
        const UiCiResult result = run_ui_ci(*g_runtime);
        shutdown_player_preview(runtime);
        return result.ok ? 0 : 2;
    }
