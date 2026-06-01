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

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    void run_ui_ci_perf_evidence_log(SceneT& scene, ContextT& ctx, PumpFrameFn& pump_frame) {
        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        ui_ci_emit_perf_evidence("home", scene);

        ctx.set_page(player::PlayerPage::NowPlaying);
        pump_frame();
        ui_ci_emit_perf_evidence("now", scene);

        ctx.set_page(player::PlayerPage::Library);
        pump_frame();
        ui_ci_emit_perf_evidence("library", scene);

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
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
        run_ui_ci_perf_evidence_log(scene, ctx, pump_frame);

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
        const UiCiResult result = run_ui_ci(*g_runtime);
        shutdown_player_preview(runtime);
        return result.ok ? 0 : 2;
    }
