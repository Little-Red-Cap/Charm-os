    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    void run_layer_snapshot_regressions(UiCiResult& res,
                                        SceneT& scene,
                                        ContextT& ctx,
                                        PumpFrameFn& pump_frame) {
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
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 16, 16},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto capture = scene.capture_command_snapshot_result(spec);
            const auto handle = capture.handle;
            const auto* record = scene.snapshot_record(handle);
            const bool lifecycle = capture.ok()
                && static_cast<bool>(handle)
                && record
                && record->kind == ::ui::scene::SnapshotKind::CommandBuffer
                && scene.snapshot_current(handle)
                && scene.mark_snapshot_stale(handle)
                && !scene.snapshot_current(handle)
                && scene.release_snapshot(handle)
                && !scene.snapshot_record(handle);
            if (lifecycle && scene.layer_stats().snapshot_count == 0) {
                ui_ci_emit("layer_snapshot_lifecycle", true, nullptr);
            } else {
                ui_ci_emit("layer_snapshot_lifecycle", false, "snapshot_lifecycle");
                res.ok = false;
                res.failed++;
            }
        }
        {
            auto& layer = ctx.page_home_layer;
            auto& scene_runtime = ctx.scene_runtime;
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 24, 24},
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto freeze = layer.freeze(scene_runtime, scene_runtime.access, spec, true);
            const bool frozen = freeze.ok()
                && freeze.handle
                && layer.snapshot() == freeze.handle
                && layer.state() == ::ui::scene::PageLayerState::Frozen
                && !layer.visible();
            layer.mark_transitioning();
            const bool transitioning = frozen
                && layer.state() == ::ui::scene::PageLayerState::Transitioning;
            layer.thaw(scene_runtime, scene_runtime.access, true);
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
                    (void)layer.release_snapshot(scene_runtime);
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
    }
