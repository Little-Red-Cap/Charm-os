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

    #include "main.ui_ci.object_tree.inc"

    #include "main.ui_ci.portability.inc"

    #include "main.ui_ci.layer_snapshot.inc"

    #include "main.ui_ci.page_flow.inc"

    #include "main.ui_ci.bootstrap.inc"

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

        run_object_tree_input_regressions(res);
        run_ui_ci_bootstrap_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        run_layer_snapshot_regressions(res, scene, ctx, pump_frame);

        run_page_flow_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    int run_ui_ci_preview(SdlHostRuntime& runtime) noexcept {
        const UiCiResult result = run_ui_ci(*g_runtime);
        shutdown_player_preview(runtime);
        return result.ok ? 0 : 2;
    }
