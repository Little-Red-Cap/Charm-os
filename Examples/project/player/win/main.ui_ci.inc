    constexpr std::size_t kUiCmdBudget = 1200;
    constexpr std::uint64_t kUiAlphaBlendBudget = 1000000;
    constexpr std::uint32_t kUiLayerBytesBudget = 0;

    const std::array<std::uint32_t, 4>& ui_ci_static_cover_pixels() noexcept;
    player::PlayerCoverResourceRecordTableView ui_ci_static_cover_record_table() noexcept;
    bool ui_ci_static_cover_provider(void*,
                                     const player::CoverResourceRequest& request,
                                     player::CoverResourceView& out) noexcept;

    #include "main.ui_ci.portability.inc"

    #include "main.ui_ci.layer_snapshot.inc"

    #include "main.ui_ci.page_flow.inc"

    #include "main.ui_ci.now_playing_matrix.inc"

    #include "main.ui_ci.bootstrap.inc"

    template<typename SceneT>
    void ui_ci_emit_perf_evidence(const char* page, const SceneT& scene) {
        const auto evidence = ::ui::scene::make_scene_evidence(scene);
        std::printf(
            "[ui-ci.perf] page=%s cmd=%u/%u bytes=%u text=%u/%u blob=%u/%u "
            "batch=%u/%u/%u exec=%u/%u fail=%u alpha=%u "
            "groups=%u/%u/%u/%u flush=%u clip_fail=%u kinds=%u/%u/%u/%u/%u/%u fail_detail=%u/%u/%u "
            "layer=%u/%u/%u/%u/%u/%u/%u\n",
            page ? page : "unknown",
            evidence.cmd.cmd_count,
            evidence.cmd.cmd_capacity,
            evidence.cmd.cmd_bytes,
            evidence.cmd.text_used,
            evidence.cmd.text_capacity,
            evidence.cmd.blob_used,
            evidence.cmd.blob_capacity,
            evidence.cmd.batch_shrink,
            evidence.cmd.batch_shrink_rect + evidence.cmd.batch_shrink_round + evidence.cmd.batch_shrink_focus,
            evidence.cmd.batch_shrink_line + evidence.cmd.batch_shrink_path + evidence.cmd.batch_shrink_image,
            evidence.exec.cmd_count,
            evidence.exec.cmd_bytes,
            evidence.exec.failed_cmds,
            evidence.exec.alpha_blend_count,
            evidence.exec.group_rect,
            evidence.exec.group_text,
            evidence.exec.group_image,
            evidence.exec.group_line + evidence.exec.group_path + evidence.exec.group_other,
            evidence.exec.batch_flushes,
            evidence.exec.clip_push_overflow
                + evidence.exec.clip_pop_underflow
                + evidence.exec.clip_invalid
                + evidence.exec.fail_clip,
            evidence.exec.cmd_rect,
            evidence.exec.cmd_text,
            evidence.exec.cmd_image,
            evidence.exec.cmd_line,
            evidence.exec.cmd_path,
            evidence.exec.cmd_other,
            evidence.exec.fail_text,
            evidence.exec.fail_image,
            evidence.exec.fail_blob + evidence.exec.fail_path + evidence.exec.fail_clip + evidence.exec.fail_other,
            evidence.layer.snapshot_count,
            evidence.layer.snapshot_rebuild_count,
            evidence.layer.stale_snapshot_count,
            evidence.layer.layer_bytes,
            evidence.layer.composite_pixels,
            evidence.layer.pixel_blit_count,
            evidence.layer.pixel_blit_pixels);
    }

    struct UiCiFrameTiming {
        std::uint32_t available{0};
        std::uint32_t frame_us{0};
        std::uint32_t tick_us{0};
        std::uint32_t render_us{0};
        std::uint32_t record_us{0};
        std::uint32_t execute_us{0};
        std::uint32_t present_us{0};
    };

    struct UiCiTimedDisplaySink {
        player::PlayerDisplaySink* inner{nullptr};
        std::uint32_t present_us{0};
    };

    std::uint32_t ui_ci_delta_us(charm::system::ClockTick start,
                                 charm::system::ClockTick end) noexcept {
        return end >= start
            ? ::ui::scene::clamp_scene_timing_us(static_cast<std::uint64_t>(end - start))
            : 0U;
    }

    bool ui_ci_timed_present(void* ctx,
                             const player::PlayerDisplaySurface& surface,
                             player::PlayerDirtyRegion dirty) noexcept {
        auto* timed = static_cast<UiCiTimedDisplaySink*>(ctx);
        const auto start = g_clock.now_us();
        const bool ok = !timed || !timed->inner || timed->inner->present(surface, dirty);
        const auto end = g_clock.now_us();
        if (timed) {
            timed->present_us = ui_ci_delta_us(start, end);
        }
        return ok;
    }

    UiCiFrameTiming ui_ci_pump_frame_timed(PlayerRuntime& runtime,
                                           player::PlayerDisplaySink* display_sink) {
        UiCiFrameTiming out{};
        out.available = 1U;

        UiCiTimedDisplaySink timed_sink_state{display_sink, 0U};
        player::PlayerDisplaySink timed_sink{&timed_sink_state, &ui_ci_timed_present};
        auto* sink = display_sink ? &timed_sink : nullptr;

        const auto frame_start = g_clock.now_us();
        const auto tick_start = frame_start;
        runtime.tick(g_clock.now_us());
        const auto tick_end = g_clock.now_us();
        (void)runtime.render(sink);
        const auto frame_end = g_clock.now_us();

        const auto scene_timing = runtime.scene_ref().last_render_timing();
        out.frame_us = ui_ci_delta_us(frame_start, frame_end);
        out.tick_us = ui_ci_delta_us(tick_start, tick_end);
        out.render_us = ui_ci_delta_us(tick_end, frame_end);
        out.record_us = scene_timing.record_us;
        out.execute_us = scene_timing.execute_us;
        out.present_us = timed_sink_state.present_us;
        return out;
    }

    void ui_ci_emit_perf_time_evidence(const char* page,
                                       const UiCiFrameTiming& timing,
                                       const ::ui::scene::SceneEvidence& scene_evidence) {
        std::printf(
            "[ui-ci.perf_time] page=%s available=%u frame=%u tick=%u render=%u "
            "record=%u execute=%u present=%u scene_available=%u\n",
            page ? page : "unknown",
            timing.available,
            timing.frame_us,
            timing.tick_us,
            timing.render_us,
            timing.record_us,
            timing.execute_us,
            timing.present_us,
            scene_evidence.timing.available);
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn, typename TimedPumpFrameFn>
    void run_ui_ci_perf_evidence_log(SceneT& scene,
                                     ContextT& ctx,
                                     PumpFrameFn& pump_frame,
                                     TimedPumpFrameFn& timed_pump_frame,
                                     player::PlayerDisplaySink* display_sink) {
        ctx.set_page(player::PlayerPage::Home);
        auto home_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence("home", scene);
        ui_ci_emit_perf_time_evidence("home", home_timing, ::ui::scene::make_scene_evidence(scene));

        ctx.set_page(player::PlayerPage::NowPlaying);
        auto now_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence("now", scene);
        ui_ci_emit_perf_time_evidence("now", now_timing, ::ui::scene::make_scene_evidence(scene));

        ctx.set_page(player::PlayerPage::Library);
        auto library_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence("library", scene);
        ui_ci_emit_perf_time_evidence("library", library_timing, ::ui::scene::make_scene_evidence(scene));

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
    }

    UiCiResult run_ui_ci(PlayerRuntime& runtime, player::PlayerDisplaySink* perf_display_sink = nullptr) {
        UiCiResult res{};
        auto& ctx = *runtime.controller();
        auto& scene = runtime.scene_ref();

        auto pump_frame = [&]() {
            ui_ci_pump_frame(runtime, nullptr);
        };
        auto timed_pump_frame = [&](player::PlayerDisplaySink* display_sink = nullptr) {
            return ui_ci_pump_frame_timed(runtime, display_sink);
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
        run_ui_ci_perf_evidence_log(scene, ctx, pump_frame, timed_pump_frame, perf_display_sink);

        run_object_tree_input_regressions(res);
        run_ui_ci_bootstrap_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        run_layer_snapshot_regressions(res, scene, ctx, pump_frame);

        run_page_flow_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        run_now_playing_state_matrix_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    int run_ui_ci_preview(SdlHostRuntime& runtime) noexcept {
        const UiCiResult result = run_ui_ci(*g_runtime, &runtime.display_sink);
        shutdown_player_preview(runtime);
        return result.ok ? 0 : 2;
    }
