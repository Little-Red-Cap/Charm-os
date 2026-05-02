#include <cstdio>

import charm.gfx.canvas;
import charm.ui.vivid;

namespace {
    struct TransitionScene {
        DefaultFrameBuffer fb{};
        DefaultCanvas canvas{fb};
        ui::scene::Scene scene{canvas};
        WidgetHandle root{};
        WidgetHandle source_root{};
        WidgetHandle destination_root{};
        ui::scene::PageLayer source{};
        ui::scene::PageLayer destination{};

        TransitionScene() {
            scene.build([&](ui::scene::SceneBuilder& builder) {
                root = builder.create_container();
                source_root = builder.create_container();
                destination_root = builder.create_container();
                builder.set_rect(root, {0, 0, 32, 32});
                builder.set_rect(source_root, {0, 0, 32, 32});
                builder.set_rect(destination_root, {0, 0, 32, 32});
                builder.link(root, source_root);
                builder.link(root, destination_root);
                builder.set_input_root(root);
                builder.set_root(root);
            });
            source.set_root(source_root);
            destination.set_root(destination_root);
            auto access = scene.access();
            source.show(access);
            destination.hide(access);
            canvas.clear(rgba{0, 0, 0, 255});
        }
    };

    struct PaintPrepare {
        DefaultCanvas* canvas{nullptr};
        rgba color{};
        int x{0};
        int y{0};
        bool ok{true};
    };

    inline constexpr unsigned one_pixel_snapshot_bytes = 3;

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool expect_snapshot_count(const TransitionScene& env,
                                             unsigned expected,
                                             const char* message) noexcept {
        return expect(static_cast<unsigned>(env.scene.layer_stats().snapshot_count) == expected,
                      message);
    }

    [[nodiscard]] bool expect_page_truth(const TransitionScene& env,
                                         bool source_visible,
                                         bool destination_visible,
                                         const char* message) noexcept {
        return expect(env.source.visible() == source_visible &&
                          env.destination.visible() == destination_visible,
                      message);
    }

    [[nodiscard]] bool expect_pixel_single_admission(
        const ui::scene::PageTransitionBeginResult& begin,
        const char* message) noexcept {
        return expect(begin.admission == ui::scene::LayerAdmission::PixelSingle, message);
    }

    [[nodiscard]] bool expect_source_only_capture(const ui::scene::PageTransitionTrace& trace,
                                                  const char* message) noexcept {
        return expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                      message);
    }

    [[nodiscard]] bool expect_source_only_frame(const ui::scene::PageTransitionFrame& frame,
                                                const char* message) noexcept {
        return expect(frame.source.valid && !frame.destination.valid, message);
    }

    [[nodiscard]] bool expect_source_only_layer_cost(
        const ui::scene::PageTransitionLedger& ledger,
        const char* message) noexcept {
        return expect(ledger.source_bytes == one_pixel_snapshot_bytes &&
                          ledger.destination_bytes == 0 &&
                          ledger.peak_layer_bytes == one_pixel_snapshot_bytes,
                      message);
    }

    [[nodiscard]] constexpr bool same_rgba(rgba a, rgba b) noexcept {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    bool prepare_destination(ui::scene::Scene&,
                             ui::scene::SceneAccess,
                             ui::scene::PageLayer&,
                             void* ctx) noexcept {
        auto* prepare = static_cast<PaintPrepare*>(ctx);
        if (!prepare || !prepare->ok) return false;
        prepare->canvas->set_pixel(prepare->x, prepare->y, prepare->color);
        return true;
    }

    [[nodiscard]] constexpr ui::scene::LayerBudget pixel_single_budget() noexcept {
        return {.max_layer_bytes = one_pixel_snapshot_bytes};
    }

    [[nodiscard]] ui::scene::PageTransitionSpec transition_spec(TransitionScene& env,
                                                               PaintPrepare& prepare,
                                                               ui::scene::LayerBudget budget = {},
                                                               ui::scene::LayerProfile profile =
                                                                   ui::scene::LayerProfile::Rich) noexcept {
        return {
            .source = &env.source,
            .destination = &env.destination,
            .source_snapshot = {
                .bounds = {.x = 2, .y = 2, .w = 1, .h = 1},
                .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
            },
            .destination_snapshot = {
                .bounds = {.x = 4, .y = 2, .w = 1, .h = 1},
                .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
            },
            .recipe = ui::scene::motion_slide(ui::scene::MotionAxis::X, 5, 100),
            .requested_profile = profile,
            .budget = budget,
            .start_ms = 0,
            .hide_source_live_root = true,
            .hide_destination_live_root = true,
            .prepare_destination = prepare_destination,
            .prepare_ctx = &prepare,
        };
    }

    [[nodiscard]] ui::scene::PageTransitionSpec fade_slide_transition_spec(
        TransitionScene& env,
        PaintPrepare& prepare,
        ui::scene::LayerBudget budget = {},
        ui::scene::LayerProfile profile = ui::scene::LayerProfile::Rich) noexcept {
        auto spec = transition_spec(env, prepare, budget, profile);
        spec.recipe = ui::scene::motion_fade_slide(ui::scene::MotionAxis::X, 6, 120, 0, 240);
        return spec;
    }

    [[nodiscard]] bool run_normal_commit() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{230, 20, 30, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{20, 40, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        if (!expect(begin.started(), "normal transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::PixelDouble,
                    "normal transition admits PixelDouble")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 2,
                    "normal transition owns two snapshots")) {
            return false;
        }
        if (!expect(!env.source.visible() && !env.destination.visible(),
                    "normal transition hides live roots")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 0);
        if (!expect(frame.valid, "normal transition composes frame")) return false;
        if (!expect(frame.destination.valid && frame.source.valid,
                    "normal transition composes destination and source")) {
            return false;
        }
        const auto destination_pixel = env.canvas.get_pixel(4, 2);
        const auto source_pixel = env.canvas.get_pixel(7, 2);
        if (!expect(destination_pixel.b == 220, "destination snapshot is composed")) return false;
        if (!expect(source_pixel.r == 230, "source snapshot is composed with motion")) return false;
        const auto done = runner.sample(env.scene, 120);
        if (!expect(done.transition.state == ui::scene::MotionTransitionState::Finished,
                    "normal transition reaches finished frame")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "normal transition returns idle")) return false;
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "normal commit updates page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "normal commit releases all snapshots")) {
            return false;
        }
        if (!expect(trace.commit_count == 1 && trace.abort_count == 0,
                    "normal trace records commit only")) {
            return false;
        }
        if (!expect(ledger.committed && !ledger.aborted, "normal ledger records commit")) return false;
        if (!expect(ledger.peak_layer_bytes == 6, "normal ledger records peak layer bytes")) return false;
        if (!expect(ledger.destination_composite_pixels == 2 &&
                    ledger.source_composite_pixels == 2 &&
                    ledger.total_composite_pixels == 4,
                    "normal ledger records compose pixels")) {
            return false;
        }
        if (!expect(ledger.snapshots_released, "normal ledger records released snapshots")) return false;
        std::printf("[pt] normal status=%s admission=%s snapshots=%u commit=%u bytes=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(trace.commit_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes),
                    static_cast<unsigned>(ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_fade_slide_pixel_double() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{235, 30, 50, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{25, 70, 225, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, fade_slide_transition_spec(env, prepare));
        if (!expect(begin.started(), "fade slide transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::PixelDouble,
                    "fade slide transition admits PixelDouble")) {
            return false;
        }
        if (!expect_snapshot_count(env, 2, "fade slide owns two snapshots")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(frame.valid, "fade slide composes frame")) return false;
        if (!expect(frame.destination.valid && frame.source.valid,
                    "fade slide composes both layers")) {
            return false;
        }
        if (!expect(frame.transition.motion.transform.x == 3 &&
                    frame.transition.motion.transform.opacity == 120,
                    "fade slide frame carries transform and opacity")) {
            return false;
        }
        if (!expect(frame.source.plan.transform.x == 3 &&
                    frame.source.plan.transform.opacity == 120,
                    "fade slide source compose uses sampled transform")) {
            return false;
        }
        if (!expect(trace.motion.recipe_kind == ui::scene::MotionRecipeKind::FadeSlide &&
                    trace.motion.profile == ui::scene::LayerProfile::Rich,
                    "fade slide trace records recipe and profile")) {
            return false;
        }
        if (!expect(ledger.destination_composite_pixels == 1 &&
                    ledger.source_composite_pixels == 1 &&
                    ledger.total_composite_pixels == 2,
                    "fade slide ledger records one composed frame")) {
            return false;
        }
        if (!expect(frame.source.replay.stats.alpha_blend_count == 1,
                    "fade slide source compose applies opacity blend")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto committed_ledger = runner.ledger();
        if (!expect(runner.idle(), "fade slide returns idle")) return false;
        if (!expect_page_truth(env, false, true, "fade slide commit updates page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "fade slide commit releases snapshots")) {
            return false;
        }
        if (!expect(committed_ledger.committed && committed_ledger.snapshots_released,
                    "fade slide ledger records released commit")) {
            return false;
        }
        std::printf("[pt] fade_slide status=%s admission=%s x=%d opacity=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<int>(frame.transition.motion.transform.x),
                    static_cast<unsigned>(frame.transition.motion.transform.opacity),
                    static_cast<unsigned>(committed_ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_fade_slide_cheap_quantized() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{225, 40, 70, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{35, 80, 215, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene,
                                        access,
                                        fade_slide_transition_spec(env,
                                                                   prepare,
                                                                   {},
                                                                   ui::scene::LayerProfile::Cheap));
        if (!expect(begin.started(), "cheap fade slide transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::PixelDouble,
                    "cheap fade slide admits PixelDouble")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto trace = runner.trace();
        if (!expect(frame.valid, "cheap fade slide composes frame")) return false;
        if (!expect(trace.motion.profile == ui::scene::LayerProfile::Cheap &&
                    trace.motion.tier == ui::scene::MotionTier::Cheap30Fps,
                    "cheap fade slide trace records cheap tier")) {
            return false;
        }
        if (!expect(trace.motion.last_elapsed_ms == 60 &&
                    trace.motion.last_sampled_elapsed_ms == 33,
                    "cheap fade slide quantizes motion time")) {
            return false;
        }
        if (!expect(frame.transition.motion.transform.x == 4 &&
                    frame.transition.motion.transform.opacity == 85,
                    "cheap fade slide quantizes transform and opacity")) {
            return false;
        }
        if (!expect(frame.source.plan.transform.x == 4 &&
                    frame.source.plan.transform.opacity == 85,
                    "cheap fade slide compose plan uses quantized transform")) {
            return false;
        }
        if (!expect(frame.source.replay.stats.alpha_blend_count == 1,
                    "cheap fade slide still uses alpha blend for quantized opacity")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "cheap fade slide returns idle")) return false;
        if (!expect_snapshot_count(env, 0, "cheap fade slide releases snapshots")) {
            return false;
        }
        if (!expect(ledger.committed && ledger.snapshots_released,
                    "cheap fade slide ledger records released commit")) {
            return false;
        }
        std::printf("[pt] fade_slide_cheap status=%s tier=%s sampled=%llu x=%d opacity=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::motion_tier_name(trace.motion.tier),
                    static_cast<unsigned long long>(trace.motion.last_sampled_elapsed_ms),
                    static_cast<int>(frame.transition.motion.transform.x),
                    static_cast<unsigned>(frame.transition.motion.transform.opacity));
        return true;
    }

    [[nodiscard]] bool run_cancel_during_compose() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{240, 40, 20, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{30, 60, 210, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        if (!expect(begin.started(), "cancel transition starts")) return false;
        const auto frame = runner.sample(env.scene, 0);
        if (!expect(frame.valid, "cancel transition composes before abort")) return false;
        runner.cancel(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "cancel transition returns idle")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "cancel restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "cancel releases all snapshots")) {
            return false;
        }
        if (!expect(trace.abort_count == 1 && trace.commit_count == 0,
                    "cancel trace records abort only")) {
            return false;
        }
        if (!expect(ledger.aborted && !ledger.committed, "cancel ledger records abort")) return false;
        if (!expect(ledger.total_composite_pixels == 2, "cancel ledger records one frame pixels")) return false;
        if (!expect(ledger.snapshots_released, "cancel ledger records released snapshots")) return false;
        std::printf("[pt] cancel status=%s samples=%u abort=%u snapshots=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(trace.sample_count),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_command_snapshot_static_cut() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{250, 50, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 70, 200, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, {
            .max_layer_bytes = 1,
        }));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.static_cut(), "command snapshot admission resolves static cut")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::CommandSnapshot,
                    "command snapshot admission is selected")) {
            return false;
        }
        if (!expect(trace.effective_profile == ui::scene::LayerProfile::Static,
                    "command snapshot static cut uses static effective profile")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "command snapshot static cut performs no captures")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "command snapshot static cut commits page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "command snapshot static cut leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.static_cut && ledger.committed,
                    "command snapshot static cut ledger records commit")) {
            return false;
        }
        if (!expect(ledger.admission == ui::scene::LayerAdmission::CommandSnapshot &&
                    ledger.effective_profile == ui::scene::LayerProfile::Static,
                    "command snapshot static cut is recorded in ledger")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "command snapshot static cut records no layer cost")) {
            return false;
        }
        std::printf("[pt] command_snapshot_static_cut status=%s admission=%s static_cut=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(trace.static_cut_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_static_profile_static_cut() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{245, 80, 60, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 90, 210, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene,
                                        access,
                                        transition_spec(env,
                                                        prepare,
                                                        {},
                                                        ui::scene::LayerProfile::Static));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.static_cut(), "static profile resolves static cut")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::StaticCut,
                    "static profile admission is StaticCut")) {
            return false;
        }
        if (!expect(trace.requested_profile == ui::scene::LayerProfile::Static &&
                    trace.effective_profile == ui::scene::LayerProfile::Static,
                    "static profile remains static effective profile")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "static profile performs no captures")) {
            return false;
        }
        if (!expect_page_truth(env, false, true, "static profile commits page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "static profile leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.static_cut && ledger.committed,
                    "static profile ledger records static cut commit")) {
            return false;
        }
        if (!expect(ledger.admission == ui::scene::LayerAdmission::StaticCut &&
                    ledger.requested_profile == ui::scene::LayerProfile::Static &&
                    ledger.effective_profile == ui::scene::LayerProfile::Static,
                    "static profile is recorded in ledger")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "static profile records no layer cost")) {
            return false;
        }
        std::printf("[pt] static_profile status=%s admission=%s static_cut=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(trace.static_cut_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_none_profile_reject() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{240, 100, 80, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 130, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto before_destination_pixel = env.canvas.get_pixel(4, 2);
        const auto begin = runner.begin(env.scene,
                                        access,
                                        transition_spec(env,
                                                        prepare,
                                                        {},
                                                        ui::scene::LayerProfile::None));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        const auto after_destination_pixel = env.canvas.get_pixel(4, 2);
        if (!expect(!begin.ok(), "none profile rejects transition")) return false;
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::Rejected,
                    "none profile reports rejected status")) {
            return false;
        }
        if (!expect(begin.admission == ui::scene::LayerAdmission::Reject,
                    "none profile admission is Reject")) {
            return false;
        }
        if (!expect(trace.requested_profile == ui::scene::LayerProfile::None &&
                    trace.effective_profile == ui::scene::LayerProfile::None,
                    "none profile remains none effective profile")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "none profile performs no captures")) {
            return false;
        }
        if (!expect(trace.static_cut_count == 0 && trace.commit_count == 0 &&
                    trace.abort_count == 0,
                    "none profile performs no transaction side effects")) {
            return false;
        }
        if (!expect_page_truth(env, true, false, "none profile preserves page truth")) {
            return false;
        }
        if (!expect(same_rgba(before_destination_pixel, after_destination_pixel),
                    "none profile does not run destination prepare")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "none profile leaves no snapshots")) {
            return false;
        }
        if (!expect(!ledger.committed && !ledger.aborted && !ledger.static_cut,
                    "none profile ledger records no terminal transaction")) {
            return false;
        }
        if (!expect(ledger.admission == ui::scene::LayerAdmission::Reject &&
                    ledger.requested_profile == ui::scene::LayerProfile::None &&
                    ledger.effective_profile == ui::scene::LayerProfile::None,
                    "none profile reject is recorded in ledger")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "none profile records no layer cost")) {
            return false;
        }
        std::printf("[pt] none_profile status=%s admission=%s commits=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(trace.commit_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_pixel_single_live_destination() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{220, 70, 30, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{30, 110, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, pixel_single_budget()));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.started(), "pixel single transition starts")) return false;
        if (!expect_pixel_single_admission(begin, "pixel single admission is selected")) {
            return false;
        }
        if (!expect_source_only_capture(trace, "pixel single captures source only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1, "pixel single owns one snapshot during transition")) {
            return false;
        }
        if (!expect_page_truth(env, false, true, "pixel single keeps destination live during transition")) {
            return false;
        }
        if (!expect(env.destination.live(), "pixel single destination is live")) return false;
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 0);
        const auto after_sample = runner.ledger();
        if (!expect(frame.valid, "pixel single composes source frame")) return false;
        if (!expect_source_only_frame(frame, "pixel single composes source over live destination")) {
            return false;
        }
        const auto source_pixel = env.canvas.get_pixel(7, 2);
        if (!expect(source_pixel.r == 220, "pixel single source snapshot is composed")) return false;
        if (!expect_source_only_layer_cost(after_sample,
                                           "pixel single ledger records one snapshot peak")) {
            return false;
        }
        if (!expect(after_sample.source_composite_pixels == 1 &&
                    after_sample.destination_composite_pixels == 0,
                    "pixel single ledger records source-only compose pixels")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto committed_trace = runner.trace();
        const auto committed_ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single returns idle")) return false;
        if (!expect_page_truth(env, false, true, "pixel single commit updates page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "pixel single commit releases source snapshot")) {
            return false;
        }
        if (!expect(committed_trace.commit_count == 1 && committed_trace.abort_count == 0,
                    "pixel single trace records commit only")) {
            return false;
        }
        if (!expect(committed_ledger.committed && committed_ledger.snapshots_released,
                    "pixel single ledger records released commit")) {
            return false;
        }
        std::printf("[pt] pixel_single status=%s admission=%s source_caps=%u dst_caps=%u snapshots=%u bytes=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(committed_trace.source_capture_count),
                    static_cast<unsigned>(committed_trace.destination_capture_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(committed_ledger.peak_layer_bytes),
                    static_cast<unsigned>(committed_ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_pixel_single_cancel() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 90, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 120, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, pixel_single_budget()));
        if (!expect(begin.started(), "pixel single cancel transition starts")) return false;
        if (!expect_pixel_single_admission(begin, "pixel single cancel admission is selected")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1, "pixel single cancel owns one snapshot before abort")) {
            return false;
        }
        if (!expect_page_truth(env, false, true, "pixel single cancel has live destination before abort")) {
            return false;
        }
        const auto frame = runner.sample(env.scene, 0);
        if (!expect_source_only_frame(frame, "pixel single cancel samples source-only frame")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single cancel returns idle")) return false;
        if (!expect_page_truth(env, true, false, "pixel single cancel restores page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "pixel single cancel releases source snapshot")) {
            return false;
        }
        if (!expect(trace.abort_count == 1 && trace.commit_count == 0,
                    "pixel single cancel trace records abort only")) {
            return false;
        }
        if (!expect(ledger.aborted && !ledger.committed,
                    "pixel single cancel ledger records abort")) {
            return false;
        }
        if (!expect_source_only_layer_cost(ledger,
                                           "pixel single cancel ledger records source-only bytes")) {
            return false;
        }
        if (!expect(ledger.source_composite_pixels == 1 &&
                    ledger.destination_composite_pixels == 0,
                    "pixel single cancel ledger records source-only pixels")) {
            return false;
        }
        if (!expect(ledger.snapshots_released, "pixel single cancel ledger records release")) {
            return false;
        }
        std::printf("[pt] pixel_single_cancel status=%s admission=%s abort=%u snapshots=%u bytes=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes),
                    static_cast<unsigned>(ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_prepare_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 30, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{20, 90, 230, 255}, .x = 4, .y = 2, .ok = false};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::PrepareFailed,
                    "prepare failure is reported")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "prepare failure happens after source capture")) {
            return false;
        }
        if (!expect(trace.abort_count == 1, "prepare failure records abort")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "prepare failure restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "prepare failure releases source snapshot")) {
            return false;
        }
        if (!expect(ledger.aborted && !ledger.committed, "prepare failure ledger records abort")) {
            return false;
        }
        if (!expect(ledger.source_bytes == 3 && ledger.destination_bytes == 0,
                    "prepare failure ledger records source-only bytes")) {
            return false;
        }
        std::printf("[pt] prepare_fail status=%s source_caps=%u abort=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(trace.source_capture_count),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_source_capture_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{190, 40, 60, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 90, 220, 255}, .x = 4, .y = 2};
        auto spec = transition_spec(env, prepare);
        spec.source_snapshot.bounds = {.x = 40, .y = 40, .w = 1, .h = 1};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, spec);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::SourceCaptureFailed,
                    "source capture failure is reported")) {
            return false;
        }
        if (!expect(begin.source_capture.status == ui::scene::LayerCaptureStatus::StoreFailed,
                    "source capture failure keeps capture status")) {
            return false;
        }
        if (!expect(trace.begin_status == ui::scene::PageTransitionBeginStatus::SourceCaptureFailed,
                    "source capture failure is recorded in trace")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "source capture failure performs no successful captures")) {
            return false;
        }
        if (!expect(trace.abort_count == 1, "source capture failure records abort")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "source capture failure restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "source capture failure leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.aborted && ledger.peak_layer_bytes == 0,
                    "source capture failure ledger records no captured bytes")) {
            return false;
        }
        std::printf("[pt] source_capture_fail status=%s capture=%u abort=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(begin.source_capture.status),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_destination_capture_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{180, 50, 70, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 100, 230, 255}, .x = 4, .y = 2};
        auto spec = transition_spec(env, prepare);
        spec.destination_snapshot.bounds = {.x = 40, .y = 40, .w = 1, .h = 1};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, spec);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::DestinationCaptureFailed,
                    "destination capture failure is reported")) {
            return false;
        }
        if (!expect(begin.source_capture.ok(), "destination capture failure keeps source capture result")) {
            return false;
        }
        if (!expect(begin.destination_capture.status == ui::scene::LayerCaptureStatus::StoreFailed,
                    "destination capture failure keeps capture status")) {
            return false;
        }
        if (!expect(trace.begin_status == ui::scene::PageTransitionBeginStatus::DestinationCaptureFailed,
                    "destination capture failure is recorded in trace")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "destination capture failure happens after source capture")) {
            return false;
        }
        if (!expect(trace.abort_count == 1, "destination capture failure records abort")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "destination capture failure restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "destination capture failure releases source snapshot")) {
            return false;
        }
        if (!expect(ledger.aborted && ledger.source_bytes == 3 && ledger.destination_bytes == 0,
                    "destination capture failure ledger records source-only bytes")) {
            return false;
        }
        std::printf("[pt] destination_capture_fail status=%s src_caps=%u dst_capture=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(trace.source_capture_count),
                    static_cast<unsigned>(begin.destination_capture.status),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_rebegin_interrupt() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 80, 30, 255});
        PaintPrepare first_prepare{.canvas = &env.canvas, .color = rgba{30, 80, 210, 255}, .x = 4, .y = 2};
        PaintPrepare second_prepare{.canvas = &env.canvas, .color = rgba{60, 120, 240, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto first = runner.begin(env.scene, access, transition_spec(env, first_prepare));
        if (!expect(first.started(), "rebegin first transition starts")) return false;
        const auto first_frame = runner.sample(env.scene, 0);
        if (!expect(first_frame.valid, "rebegin first transition composes")) return false;
        if (!expect(env.scene.layer_stats().snapshot_count == 2,
                    "rebegin first transition owns two snapshots")) {
            return false;
        }
        const auto second = runner.begin(env.scene, access, transition_spec(env, second_prepare));
        const auto trace = runner.trace();
        if (!expect(second.started(), "rebegin starts replacement transition")) return false;
        if (!expect(trace.interrupt_count == 1, "rebegin trace records interrupt")) return false;
        if (!expect(env.scene.layer_stats().snapshot_count == 2,
                    "rebegin releases old snapshots before reacquiring")) {
            return false;
        }
        if (!expect(!env.source.visible() && !env.destination.visible(),
                    "rebegin replacement transition owns hidden live roots")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto canceled_trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(canceled_trace.interrupt_count == 1,
                    "rebegin interrupt count survives cancel")) {
            return false;
        }
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "rebegin cancel restores original page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "rebegin cancel releases replacement snapshots")) {
            return false;
        }
        if (!expect(ledger.interrupted && ledger.aborted, "rebegin ledger records interrupt abort")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 6 && ledger.snapshots_released,
                    "rebegin ledger records replacement layer cost")) {
            return false;
        }
        std::printf("[pt] rebegin first=%s second=%s interrupts=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(first.status),
                    ui::scene::page_transition_begin_status_name(second.status),
                    static_cast<unsigned>(canceled_trace.interrupt_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_pixel_single_rebegin_interrupt() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{230, 100, 30, 255});
        PaintPrepare first_prepare{.canvas = &env.canvas, .color = rgba{30, 100, 230, 255}, .x = 4, .y = 2};
        PaintPrepare second_prepare{.canvas = &env.canvas, .color = rgba{70, 140, 240, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto budget = pixel_single_budget();
        const auto first = runner.begin(env.scene, access, transition_spec(env, first_prepare, budget));
        if (!expect(first.started(), "pixel single rebegin first transition starts")) return false;
        if (!expect_pixel_single_admission(first,
                                           "pixel single rebegin first admission is selected")) {
            return false;
        }
        const auto first_frame = runner.sample(env.scene, 0);
        if (!expect_source_only_frame(first_frame,
                                      "pixel single rebegin first transition composes source-only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1,
                                   "pixel single rebegin first transition owns one snapshot")) {
            return false;
        }
        const auto second = runner.begin(env.scene, access, transition_spec(env, second_prepare, budget));
        const auto trace = runner.trace();
        if (!expect(second.started(), "pixel single rebegin starts replacement transition")) return false;
        if (!expect_pixel_single_admission(second,
                                           "pixel single rebegin replacement admission is selected")) {
            return false;
        }
        if (!expect(trace.interrupt_count == 1, "pixel single rebegin trace records interrupt")) {
            return false;
        }
        if (!expect_source_only_capture(trace,
                                        "pixel single rebegin replacement captures source only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1,
                                   "pixel single rebegin releases old snapshot before reacquiring")) {
            return false;
        }
        if (!expect_page_truth(env, false, true,
                               "pixel single rebegin replacement keeps destination live")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto canceled_trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(canceled_trace.interrupt_count == 1,
                    "pixel single rebegin interrupt count survives cancel")) {
            return false;
        }
        if (!expect_page_truth(env, true, false,
                               "pixel single rebegin cancel restores original page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0,
                                   "pixel single rebegin cancel releases replacement snapshot")) {
            return false;
        }
        if (!expect(ledger.interrupted && ledger.aborted,
                    "pixel single rebegin ledger records interrupt abort")) {
            return false;
        }
        if (!expect_source_only_layer_cost(ledger,
                                           "pixel single rebegin ledger records source-only layer cost")) {
            return false;
        }
        if (!expect(ledger.snapshots_released,
                    "pixel single rebegin ledger records released snapshot")) {
            return false;
        }
        std::printf("[pt] pixel_single_rebegin first=%s second=%s interrupts=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(first.status),
                    ui::scene::page_transition_begin_status_name(second.status),
                    static_cast<unsigned>(canceled_trace.interrupt_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }
}

int main() {
    if (!run_normal_commit()) return 1;
    if (!run_fade_slide_pixel_double()) return 1;
    if (!run_fade_slide_cheap_quantized()) return 1;
    if (!run_cancel_during_compose()) return 1;
    if (!run_command_snapshot_static_cut()) return 1;
    if (!run_static_profile_static_cut()) return 1;
    if (!run_none_profile_reject()) return 1;
    if (!run_pixel_single_live_destination()) return 1;
    if (!run_pixel_single_cancel()) return 1;
    if (!run_prepare_fail()) return 1;
    if (!run_source_capture_fail()) return 1;
    if (!run_destination_capture_fail()) return 1;
    if (!run_rebegin_interrupt()) return 1;
    if (!run_pixel_single_rebegin_interrupt()) return 1;
    std::puts("[page_transition_demo] ok");
    return 0;
}
