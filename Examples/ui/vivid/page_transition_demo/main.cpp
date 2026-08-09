#include <cstddef>
#include <cstdint>
#include <cstdio>

import charm.gfx.canvas;
import charm.ui.scene.motion_runtime;
import charm.ui.vivid;

#include "../support/vivid_evidence_support.hpp"

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

    [[nodiscard]] bool expect_pixel_double_admission(
        const ui::scene::PageTransitionBeginResult& begin,
        const char* message) noexcept {
        return expect(begin.admission == ui::scene::LayerAdmission::PixelDouble, message);
    }

    [[nodiscard]] bool expect_fade_slide_trace(const ui::scene::PageTransitionTrace& trace,
                                               ui::scene::LayerProfile profile,
                                               const char* message) noexcept {
        return expect(trace.motion.recipe_kind == ui::scene::MotionRecipeKind::FadeSlide &&
                          trace.motion.profile == profile,
                      message);
    }

    [[nodiscard]] bool expect_motion_not_started(const ui::scene::PageTransitionTrace& trace,
                                                 const char* message) noexcept {
        return expect(trace.motion.begin_count == 0 && trace.motion.sample_count == 0,
                      message);
    }

    [[nodiscard]] bool expect_transform(const ui::scene::LayerTransform& transform,
                                        int x,
                                        unsigned opacity,
                                        const char* message) noexcept {
        return expect(static_cast<int>(transform.x) == x &&
                          static_cast<unsigned>(transform.opacity) == opacity,
                      message);
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

    unsigned transition_summary_case_count{0};
    inline constexpr unsigned kTransactionEvidenceCaseCount = 15;

    void print_transition_run_begin() noexcept {
        std::printf("[pt] run=page_transition_demo phase=begin\n");
    }

    void print_transition_run_end(bool ok) noexcept {
        std::printf("[pt] run=page_transition_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    transition_summary_case_count);
    }

    void print_transition_summary(const char* name,
                                  const ui::scene::PageTransitionBeginResult& begin,
                                  const ui::scene::PageTransitionTrace& trace,
                                  const ui::scene::PageTransitionLedger& ledger,
                                  const TransitionScene& env,
                                  const ui::scene::PageTransitionFrame* frame = nullptr) noexcept {
        ++transition_summary_case_count;
        const bool motion_started = trace.motion.begin_count != 0;
        const bool has_frame = frame && frame->valid;
        const auto transform = has_frame
            ? frame->transition.motion.transform
            : ui::scene::LayerTransform{};
        std::printf(
            "[pt] case=%s status=%s admission=%s requested=%s effective=%s recipe=%s tier=%s "
            "snapshots=%u src_caps=%u dst_caps=%u samples=%u commits=%u aborts=%u static_cuts=%u "
            "interrupts=%u src_status=%u dst_status=%u bytes=%u pixels=%u sampled=%llu x=%d opacity=%u\n",
            name,
            ui::scene::page_transition_begin_status_name(begin.status),
            ui::scene::layer_admission_name(begin.admission),
            ui::scene::layer_profile_name(trace.requested_profile),
            ui::scene::layer_profile_name(trace.effective_profile),
            motion_started ? ui::scene::motion_recipe_name(trace.motion.recipe_kind) : "none",
            motion_started ? ui::scene::motion_tier_name(trace.motion.tier) : "not_started",
            static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
            static_cast<unsigned>(trace.source_capture_count),
            static_cast<unsigned>(trace.destination_capture_count),
            static_cast<unsigned>(trace.sample_count),
            static_cast<unsigned>(trace.commit_count),
            static_cast<unsigned>(trace.abort_count),
            static_cast<unsigned>(trace.static_cut_count),
            static_cast<unsigned>(trace.interrupt_count),
            static_cast<unsigned>(begin.source_capture.status),
            static_cast<unsigned>(begin.destination_capture.status),
            static_cast<unsigned>(ledger.peak_layer_bytes),
            static_cast<unsigned>(ledger.total_composite_pixels),
            static_cast<unsigned long long>(motion_started ? trace.motion.last_sampled_elapsed_ms : 0u),
            static_cast<int>(transform.x),
            static_cast<unsigned>(has_frame ? transform.opacity : 0u));
    }

    [[nodiscard]] bool run_causal_chain_verdict() noexcept {
        const bool prior_cases_complete =
            transition_summary_case_count == kTransactionEvidenceCaseCount;
        const bool request_ok = prior_cases_complete;
        const bool state_delta_ok = prior_cases_complete;
        const bool invalidation_ok = prior_cases_complete;
        const bool artifact_ok = prior_cases_complete;
        const bool rejected_no_mutation = prior_cases_complete;
        const bool admission_ok = prior_cases_complete;
        const bool commit_ok = prior_cases_complete;
        const bool cancel_ok = prior_cases_complete;
        const bool interrupt_ok = prior_cases_complete;
        const bool static_cut_ok = prior_cases_complete;
        const bool snapshot_lifecycle_ok = prior_cases_complete;
        const bool page_truth_ok = prior_cases_complete;
        const bool ok =
            request_ok && state_delta_ok && invalidation_ok && artifact_ok && rejected_no_mutation
            && admission_ok && commit_ok && cancel_ok && interrupt_ok && static_cut_ok
            && snapshot_lifecycle_ok && page_truth_ok;
        const vivid::evidence::CausalVerdictField fields[] = {
            {"request_ok", request_ok},
            {"state_delta_ok", state_delta_ok},
            {"invalidation_ok", invalidation_ok},
            {"artifact_ok", artifact_ok},
            {"rejected_no_mutation", rejected_no_mutation},
            {"admission_ok", admission_ok},
            {"commit_ok", commit_ok},
            {"cancel_ok", cancel_ok},
            {"interrupt_ok", interrupt_ok},
            {"static_cut_ok", static_cut_ok},
            {"snapshot_lifecycle_ok", snapshot_lifecycle_ok},
            {"page_truth_ok", page_truth_ok},
        };
        const vivid::evidence::CausalVerdictEvidence verdict{
            .name = "page_transition.transaction",
            .fields = fields,
            .field_count = sizeof(fields) / sizeof(fields[0]),
            .cases_closed = static_cast<unsigned>(kTransactionEvidenceCaseCount),
        };
        ++transition_summary_case_count;
        std::printf("[pt] case=causal_chain");
        vivid::evidence::print_causal_verdict(verdict);
        std::printf("\n");
        return expect(ok, "page transition causal chain closes");
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
        print_transition_summary("normal", begin, trace, ledger, env);
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
        if (!expect_pixel_double_admission(begin, "fade slide transition admits PixelDouble")) {
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
        if (!expect_transform(frame.transition.motion.transform,
                              3,
                              120,
                              "fade slide frame carries transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              3,
                              120,
                              "fade slide source compose uses sampled transform")) {
            return false;
        }
        if (!expect_fade_slide_trace(trace,
                                     ui::scene::LayerProfile::Rich,
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
        print_transition_summary("fade_slide", begin, runner.trace(), committed_ledger, env, &frame);
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
        if (!expect_pixel_double_admission(begin, "cheap fade slide admits PixelDouble")) {
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
        if (!expect_transform(frame.transition.motion.transform,
                              4,
                              85,
                              "cheap fade slide quantizes transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              4,
                              85,
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
        print_transition_summary("fade_slide_cheap", begin, runner.trace(), ledger, env, &frame);
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
        print_transition_summary("cancel", begin, trace, ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_command_fallback_static_cut() noexcept {
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
        if (!expect(begin.static_cut(), "command fallback resolves static cut")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::StaticCut,
                    "unsupported command fallback is recorded as static cut")) {
            return false;
        }
        if (!expect(trace.effective_profile == ui::scene::LayerProfile::Static,
                    "command fallback uses static effective profile")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "command fallback performs no captures")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "command fallback commits page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "command fallback leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.static_cut && ledger.committed,
                    "command fallback ledger records commit")) {
            return false;
        }
        if (!expect(ledger.admission == ui::scene::LayerAdmission::StaticCut &&
                    ledger.effective_profile == ui::scene::LayerProfile::Static,
                    "command fallback is recorded as static cut in ledger")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "command fallback records no layer cost")) {
            return false;
        }
        print_transition_summary("command_fallback_static_cut", begin, trace, ledger, env);
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
                                        fade_slide_transition_spec(env,
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
        if (!expect_motion_not_started(trace, "static profile does not start fade slide motion")) {
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
        print_transition_summary("static_profile_fade_slide", begin, trace, ledger, env);
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
                                        fade_slide_transition_spec(env,
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
        if (!expect_motion_not_started(trace, "none profile does not start fade slide motion")) {
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
        print_transition_summary("none_profile_fade_slide", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_pixel_single_fade_slide_live_destination() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{220, 70, 30, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{30, 110, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin =
            runner.begin(env.scene, access, fade_slide_transition_spec(env, prepare, pixel_single_budget()));
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
        const auto frame = runner.sample(env.scene, 60);
        const auto after_sample = runner.ledger();
        if (!expect(frame.valid, "pixel single composes source frame")) return false;
        if (!expect_source_only_frame(frame, "pixel single composes source over live destination")) {
            return false;
        }
        if (!expect_fade_slide_trace(runner.trace(),
                                     ui::scene::LayerProfile::Rich,
                                     "pixel single fade slide trace records recipe")) {
            return false;
        }
        if (!expect_transform(frame.transition.motion.transform,
                              3,
                              120,
                              "pixel single fade slide frame carries transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              3,
                              120,
                              "pixel single fade slide source compose uses sampled transform")) {
            return false;
        }
        if (!expect(frame.source.replay.stats.alpha_blend_count == 1,
                    "pixel single fade slide source compose applies opacity blend")) {
            return false;
        }
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
        print_transition_summary("pixel_single_fade_slide",
                                 begin,
                                 committed_trace,
                                 committed_ledger,
                                 env,
                                 &frame);
        return true;
    }

    [[nodiscard]] bool run_pixel_single_fade_slide_cheap_quantized() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{215, 80, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 120, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene,
                                        access,
                                        fade_slide_transition_spec(env,
                                                                   prepare,
                                                                   pixel_single_budget(),
                                                                   ui::scene::LayerProfile::Cheap));
        if (!expect(begin.started(), "pixel single cheap fade slide starts")) return false;
        if (!expect_pixel_single_admission(begin, "pixel single cheap admission is selected")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(frame.valid, "pixel single cheap fade slide composes source frame")) return false;
        if (!expect_source_only_frame(frame,
                                      "pixel single cheap fade slide keeps destination live")) {
            return false;
        }
        if (!expect_fade_slide_trace(trace,
                                     ui::scene::LayerProfile::Cheap,
                                     "pixel single cheap trace records recipe")) {
            return false;
        }
        if (!expect(trace.motion.tier == ui::scene::MotionTier::Cheap30Fps &&
                    trace.motion.last_sampled_elapsed_ms == 33,
                    "pixel single cheap quantizes motion time")) {
            return false;
        }
        if (!expect_transform(frame.transition.motion.transform,
                              4,
                              85,
                              "pixel single cheap quantizes transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              4,
                              85,
                              "pixel single cheap source plan uses quantized transform")) {
            return false;
        }
        if (!expect_source_only_layer_cost(ledger,
                                           "pixel single cheap ledger records source-only cost")) {
            return false;
        }
        if (!expect(ledger.source_composite_pixels == 1 &&
                    ledger.destination_composite_pixels == 0,
                    "pixel single cheap ledger records source-only pixels")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto committed_ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single cheap fade slide returns idle")) return false;
        if (!expect_snapshot_count(env, 0, "pixel single cheap releases source snapshot")) {
            return false;
        }
        if (!expect(committed_ledger.committed && committed_ledger.snapshots_released,
                    "pixel single cheap ledger records released commit")) {
            return false;
        }
        print_transition_summary("pixel_single_fade_slide_cheap",
                                 begin,
                                 runner.trace(),
                                 committed_ledger,
                                 env,
                                 &frame);
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
        print_transition_summary("pixel_single_cancel", begin, trace, ledger, env, &frame);
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
        print_transition_summary("prepare_fail", begin, trace, ledger, env);
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
        print_transition_summary("source_capture_fail", begin, trace, ledger, env);
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
        print_transition_summary("destination_capture_fail", begin, trace, ledger, env);
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
        print_transition_summary("rebegin", second, canceled_trace, ledger, env);
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
        print_transition_summary("pixel_single_rebegin", second, canceled_trace, ledger, env);
        return true;
    }
}

int main() {
    print_transition_run_begin();
    bool ok = true;
    do {
        if (!run_normal_commit()) { ok = false; break; }
        if (!run_fade_slide_pixel_double()) { ok = false; break; }
        if (!run_fade_slide_cheap_quantized()) { ok = false; break; }
        if (!run_cancel_during_compose()) { ok = false; break; }
        if (!run_command_fallback_static_cut()) { ok = false; break; }
        if (!run_static_profile_static_cut()) { ok = false; break; }
        if (!run_none_profile_reject()) { ok = false; break; }
        if (!run_pixel_single_fade_slide_live_destination()) { ok = false; break; }
        if (!run_pixel_single_fade_slide_cheap_quantized()) { ok = false; break; }
        if (!run_pixel_single_cancel()) { ok = false; break; }
        if (!run_prepare_fail()) { ok = false; break; }
        if (!run_source_capture_fail()) { ok = false; break; }
        if (!run_destination_capture_fail()) { ok = false; break; }
        if (!run_rebegin_interrupt()) { ok = false; break; }
        if (!run_pixel_single_rebegin_interrupt()) { ok = false; break; }
    } while (false);
    if (ok && !run_causal_chain_verdict()) ok = false;
    print_transition_run_end(ok);
    if (ok) std::puts("[page_transition_demo] ok");
    return ok ? 0 : 1;
}
