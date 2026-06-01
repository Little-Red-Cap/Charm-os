    template<typename RuntimeT,
             typename SceneT,
             typename ContextT,
             typename WaitForPageFn,
             typename SettleNowPlayingTransitionFn,
             typename PumpFrameFn>
    void run_page_flow_regressions(UiCiResult& res,
                                   RuntimeT& runtime,
                                   SceneT& scene,
                                   ContextT& ctx,
                                   WaitForPageFn& wait_for_page,
                                   SettleNowPlayingTransitionFn& settle_now_playing_transition,
                                   PumpFrameFn& pump_frame) {
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
            && ctx.layer_transition_destination_capture_count > destination_captures_before_back
            && ctx.layer_transition_destination_compose_count > destination_composes_before_back
            && ctx.layer_transition_destination_compose_pixels > destination_compose_pixels_before_back
            && ctx.effective_layer_profile == ::ui::scene::LayerProfile::Rich
            && ctx.layer_transition_fallback_reason == ::ui::scene::LayerFallbackReason::None
            && ctx.layer_transition_last_budget_ok
            && ctx.layer_transition_last_layer_bytes > 0
            && ctx.layer_transition_last_layer_bytes_budget > 0
            && ctx.layer_transition_last_composite_pixels > 0
            && ctx.layer_transition_last_composite_pixels_budget > 0) {
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
    }
