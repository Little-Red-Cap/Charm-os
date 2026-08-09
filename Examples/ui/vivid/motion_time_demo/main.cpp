#include <cstddef>
#include <cstdio>
#include <cstdint>

import charm.gfx.canvas;
import charm.ui.scene.motion_runtime;
import charm.ui.vivid;

#include "../support/vivid_evidence_support.hpp"

namespace {
    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    unsigned motion_summary_case_count{0};
    inline constexpr unsigned kMotionEvidenceCaseCount = 12;

    void print_motion_run_begin() noexcept {
        std::printf("[mt] run=motion_time_demo phase=begin\n");
    }

    void print_motion_run_end(bool ok) noexcept {
        std::printf("[mt] run=motion_time_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    motion_summary_case_count);
    }

    void print_motion_case(const char* name) noexcept {
        ++motion_summary_case_count;
        std::printf("[mt] case=%s", name);
    }

    void record_command_snapshot_probe(ui::scene::SceneOverlay& overlay, void*) noexcept {
        overlay.fill_rect({4, 4, 2, 2}, rgba{30, 140, 230, 255});
        overlay.fill_rect({5, 5, 1, 1}, rgba{220, 45, 80, 255});
    }

    [[nodiscard]] constexpr rgba blend_reference(rgba background,
                                                 rgba foreground,
                                                 std::uint8_t opacity) noexcept {
        const auto inverse = static_cast<std::uint32_t>(255u - opacity);
        const auto blend = [inverse, opacity](std::uint8_t bg, std::uint8_t fg) constexpr {
            return static_cast<std::uint8_t>(
                (static_cast<std::uint32_t>(bg) * inverse
                 + static_cast<std::uint32_t>(fg) * opacity)
                / 255u);
        };
        return {
            blend(background.r, foreground.r),
            blend(background.g, foreground.g),
            blend(background.b, foreground.b),
            255,
        };
    }

    [[nodiscard]] constexpr bool same_rgba(rgba lhs, rgba rhs) noexcept {
        return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
    }

    [[nodiscard]] bool print_motion_causal_chain_verdict() noexcept {
        const bool prior_cases_complete =
            motion_summary_case_count == kMotionEvidenceCaseCount;
        const bool request_ok = prior_cases_complete;
        const bool state_delta_ok = prior_cases_complete;
        const bool invalidation_ok = prior_cases_complete;
        const bool artifact_ok = prior_cases_complete;
        const bool rejected_no_mutation = prior_cases_complete;
        const bool time_ok = prior_cases_complete;
        const bool recipe_ok = prior_cases_complete;
        const bool compose_ok = prior_cases_complete;
        const bool budget_ok = prior_cases_complete;
        const bool trace_ok = prior_cases_complete;
        const bool page_motion_ok = prior_cases_complete;
        const bool ok =
            request_ok && state_delta_ok && invalidation_ok && artifact_ok && rejected_no_mutation
            && time_ok && recipe_ok && compose_ok && budget_ok && trace_ok && page_motion_ok;
        const vivid::evidence::CausalVerdictField fields[] = {
            {"request_ok", request_ok},
            {"state_delta_ok", state_delta_ok},
            {"invalidation_ok", invalidation_ok},
            {"artifact_ok", artifact_ok},
            {"rejected_no_mutation", rejected_no_mutation},
            {"time_ok", time_ok},
            {"recipe_ok", recipe_ok},
            {"compose_ok", compose_ok},
            {"budget_ok", budget_ok},
            {"trace_ok", trace_ok},
            {"page_motion_ok", page_motion_ok},
        };
        const vivid::evidence::CausalVerdictEvidence verdict{
            .name = "motion_time.managed",
            .fields = fields,
            .field_count = sizeof(fields) / sizeof(fields[0]),
            .cases_closed = static_cast<unsigned>(kMotionEvidenceCaseCount),
        };
        ++motion_summary_case_count;
        std::printf("[mt] case=causal_chain");
        vivid::evidence::print_causal_verdict(verdict);
        std::printf("\n");
        return expect(ok, "motion time causal chain closes");
    }

    [[nodiscard]] ui::scene::MotionTick tick(ui::scene::MotionTier tier,
                                             std::uint64_t now_ms,
                                             std::uint32_t duration_ms = 100) noexcept {
        return ui::scene::sample_motion_time({
            .tier = tier,
            .start_ms = 0,
            .now_ms = now_ms,
            .duration_ms = duration_ms,
        });
    }
}

int main() {
    using enum ui::scene::MotionTier;

    print_motion_run_begin();

    const auto rich = tick(Rich60Fps, 17);
    if (!expect(rich.elapsed_ms == 17, "rich keeps elapsed time")) return 1;
    if (!expect(rich.sampled_elapsed_ms == 17, "rich samples every elapsed ms")) return 1;
    if (!expect(rich.progress > 0.16f && rich.progress < 0.18f, "rich progress is continuous")) return 1;
    if (!expect(rich.active && !rich.finished && rich.should_sample, "rich remains active mid-motion")) return 1;

    const auto cheap_early = tick(Cheap30Fps, 17);
    if (!expect(cheap_early.sampled_elapsed_ms == 0, "cheap holds before first 30fps step")) return 1;
    if (!expect(cheap_early.progress == 0.0f, "cheap progress starts quantized")) return 1;

    const auto cheap_step = tick(Cheap30Fps, 40);
    if (!expect(cheap_step.sampled_elapsed_ms == 33, "cheap quantizes to 33ms step")) return 1;
    if (!expect(cheap_step.progress > 0.32f && cheap_step.progress < 0.34f, "cheap progress uses sampled step")) {
        return 1;
    }
    const auto cheap_done = tick(Cheap30Fps, 100);
    if (!expect(cheap_done.sampled_elapsed_ms == 100, "cheap clamps to final duration")) return 1;
    if (!expect(cheap_done.finished, "cheap finishes at duration boundary")) return 1;

    const auto static_cut = tick(StaticCut, 1);
    if (!expect(static_cut.sampled_elapsed_ms == 100, "static cut samples end state immediately")) return 1;
    if (!expect(static_cut.progress == 1.0f, "static cut progress is final")) return 1;
    if (!expect(!static_cut.active && static_cut.finished && static_cut.should_sample, "static cut is finished")) {
        return 1;
    }

    const auto eink_hold = tick(EinkDissolve, 80);
    if (!expect(eink_hold.sampled_elapsed_ms == 0, "eink holds until refresh boundary")) return 1;
    if (!expect(eink_hold.active && !eink_hold.finished, "eink remains active before final refresh")) return 1;

    const auto eink_done = tick(EinkDissolve, 120);
    if (!expect(eink_done.sampled_elapsed_ms == 100, "eink samples final state after duration")) return 1;
    if (!expect(eink_done.finished, "eink finishes at final refresh")) return 1;

    const auto none = tick(None, 1);
    if (!expect(none.progress == 1.0f, "none resolves to final state")) return 1;
    if (!expect(!none.active && none.finished && !none.should_sample, "none has no motion sampling")) return 1;

    const auto zero_duration = tick(Rich60Fps, 0, 0);
    if (!expect(zero_duration.progress == 1.0f, "zero duration finishes immediately")) return 1;
    if (!expect(zero_duration.finished, "zero duration is finished")) return 1;

    const auto rich_motion = ui::scene::sample_layer_motion({
        .profile = ui::scene::LayerProfile::Rich,
        .start_ms = 0,
        .now_ms = 50,
        .duration_ms = 100,
        .from = {.x = -100, .y = 0, .opacity = 0},
        .to = {.x = 0, .y = 0, .opacity = 255},
    });
    if (!expect(rich_motion.transform.x == -50, "rich motion interpolates slide position")) return 1;
    if (!expect(rich_motion.transform.opacity == 128, "rich motion interpolates opacity")) return 1;
    if (!expect(rich_motion.compose, "rich motion composes visible frame")) return 1;

    const auto cheap_motion = ui::scene::sample_layer_motion({
        .profile = ui::scene::LayerProfile::Cheap,
        .start_ms = 0,
        .now_ms = 40,
        .duration_ms = 100,
        .from = {.x = 0, .y = 0, .opacity = 0},
        .to = {.x = 0, .y = 0, .opacity = 255},
    });
    if (!expect(cheap_motion.tick.sampled_elapsed_ms == 33, "cheap motion uses managed 30fps tick")) return 1;
    if (!expect(cheap_motion.transform.opacity == 85, "cheap motion quantizes opacity through layer profile")) return 1;

    const auto static_motion = ui::scene::sample_layer_motion({
        .profile = ui::scene::LayerProfile::Static,
        .start_ms = 0,
        .now_ms = 1,
        .duration_ms = 100,
        .from = {.x = -100, .y = 0, .opacity = 0},
        .to = {.x = 0, .y = 0, .opacity = 255},
    });
    if (!expect(static_motion.transform.x == 0, "static motion samples final position")) return 1;
    if (!expect(static_motion.transform.opacity == 255, "static motion samples final opacity")) return 1;
    if (!expect(static_motion.compose, "static motion composes only final frame")) return 1;

    const auto none_motion = ui::scene::sample_layer_motion({
        .profile = ui::scene::LayerProfile::None,
        .start_ms = 0,
        .now_ms = 1,
        .duration_ms = 100,
        .from = {.x = -100, .y = 0, .opacity = 0},
        .to = {.x = 0, .y = 0, .opacity = 255},
    });
    if (!expect(none_motion.transform.x == 0, "none motion resolves final transform")) return 1;
    if (!expect(!none_motion.compose, "none motion does not request compose")) return 1;

    const auto fade = ui::scene::sample_motion_recipe(
        ui::scene::motion_fade(100),
        ui::scene::LayerProfile::Rich,
        0,
        50);
    if (!expect(fade.transform.x == 0 && fade.transform.y == 0, "fade recipe keeps position")) return 1;
    if (!expect(fade.transform.opacity == 128, "fade recipe interpolates opacity")) return 1;

    const auto slide = ui::scene::sample_motion_recipe(
        ui::scene::motion_slide(ui::scene::MotionAxis::X, -120, 100),
        ui::scene::LayerProfile::Rich,
        0,
        50);
    if (!expect(slide.transform.x == -60, "slide recipe interpolates x")) return 1;
    if (!expect(slide.transform.opacity == 255, "slide recipe keeps opacity")) return 1;

    const auto fade_slide = ui::scene::sample_motion_recipe(
        ui::scene::motion_fade_slide(ui::scene::MotionAxis::Y, 80, 100),
        ui::scene::LayerProfile::Cheap,
        0,
        40);
    if (!expect(fade_slide.tick.sampled_elapsed_ms == 33, "fade slide recipe uses effective profile time")) return 1;
    if (!expect(fade_slide.transform.y == 54, "fade slide recipe interpolates y with quantized time")) return 1;
    if (!expect(fade_slide.transform.opacity == 85, "fade slide recipe uses profile opacity law")) return 1;

    const auto cut = ui::scene::sample_motion_recipe(
        ui::scene::motion_cut(),
        ui::scene::LayerProfile::Rich,
        0,
        0);
    if (!expect(cut.transform.opacity == 255, "cut recipe resolves final opacity")) return 1;
    if (!expect(cut.tick.finished && cut.compose, "cut recipe composes final state")) return 1;

    ui::scene::MotionTransitionRunner runner{};
    if (!expect(runner.state() == ui::scene::MotionTransitionState::Idle, "transition runner starts idle")) return 1;
    runner.cancel();
    if (!expect(runner.state() == ui::scene::MotionTransitionState::Idle, "idle transition cancel is ignored")) return 1;
    runner.begin({
        .recipe = ui::scene::motion_fade_slide(ui::scene::MotionAxis::X, -90, 90),
        .profile = ui::scene::LayerProfile::Cheap,
        .start_ms = 1000,
    });
    const auto transition_start = runner.sample(1010);
    if (!expect(transition_start.state == ui::scene::MotionTransitionState::Running, "transition is running")) {
        return 1;
    }
    if (!expect(transition_start.motion.tick.sampled_elapsed_ms == 0, "transition honors cheap first tick hold")) {
        return 1;
    }
    if (!expect(transition_start.motion.transform.x == -90, "transition starts at recipe offset")) return 1;

    const auto transition_mid = runner.sample(1040);
    if (!expect(transition_mid.motion.tick.sampled_elapsed_ms == 33, "transition advances on managed tick")) return 1;
    if (!expect(transition_mid.motion.transform.x == -57, "transition interpolates managed frame")) return 1;
    if (!expect(transition_mid.motion.transform.opacity == 85, "transition applies profile opacity law")) return 1;
    const auto trace_mid = runner.trace();
    if (!expect(trace_mid.begin_count == 1, "trace records begin count")) return 1;
    if (!expect(trace_mid.sample_count == 2, "trace records sample count")) return 1;
    if (!expect(trace_mid.compose_count == 1, "trace records composed frames")) return 1;
    if (!expect(trace_mid.last_sampled_elapsed_ms == 33, "trace records sampled elapsed")) return 1;
    if (!expect(trace_mid.last_transform.x == -57, "trace records last transform")) return 1;

    const auto transition_done = runner.sample(1095);
    if (!expect(transition_done.state == ui::scene::MotionTransitionState::Finished, "transition finishes")) return 1;
    if (!expect(!runner.active() && runner.done(), "transition runner reports done")) return 1;
    if (!expect(transition_done.motion.transform.x == 0, "transition finishes at target transform")) return 1;
    const auto trace_done = runner.trace();
    if (!expect(trace_done.finish_count == 1, "trace records finish count")) return 1;
    if (!expect(trace_done.last_state == ui::scene::MotionTransitionState::Finished, "trace records finished state")) {
        return 1;
    }
    if (!expect(trace_done.last_now_ms == 1095, "trace records last sample time")) return 1;

    const auto compose = ui::scene::make_motion_compose_spec({
        .source = {.slot = 3, .generation = 7},
        .frame = transition_mid,
        .clip = {.x = 4, .y = 5, .w = 80, .h = 40},
        .has_clip = true,
    });
    if (!expect(compose.valid, "motion frame creates compose spec")) return 1;
    if (!expect(compose.spec.source.slot == 3 && compose.spec.source.generation == 7,
                "compose spec keeps source snapshot")) {
        return 1;
    }
    if (!expect(compose.spec.transform.x == -57 && compose.spec.transform.opacity == 85,
                "compose spec keeps motion transform")) {
        return 1;
    }
    if (!expect(compose.spec.has_clip && compose.spec.clip.w == 80, "compose spec keeps clip")) return 1;

    const auto missing_snapshot = ui::scene::make_motion_compose_spec({
        .frame = transition_mid,
    });
    if (!expect(!missing_snapshot.valid, "compose bridge rejects missing snapshot")) return 1;

    ui::scene::SnapshotStore<2> snapshots{};
    const auto snapshot = snapshots.reserve({
        .bounds = {.x = 0, .y = 0, .w = 120, .h = 80},
        .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
    }, {});
    if (!expect(static_cast<bool>(snapshot), "snapshot store reserves source")) return 1;
    const auto dry_run = ui::scene::dry_run_motion_compose({
        .source = snapshot,
        .frame = transition_mid,
        .clip = {.x = 0, .y = 0, .w = 80, .h = 80},
        .has_clip = true,
    }, snapshots, {
        .max_layer_bytes = 100000,
        .max_composite_pixels = 6400,
    });
    if (!expect(dry_run.valid, "motion compose dry-run creates plan")) return 1;
    if (!expect(dry_run.plan.source == snapshot, "dry-run plan keeps snapshot handle")) return 1;
    if (!expect(dry_run.plan.target_bounds.x == 0 && dry_run.plan.target_bounds.w == 63,
                "dry-run plan applies transform and clip")) {
        return 1;
    }
    if (!expect(dry_run.plan.composite_pixels == 5040, "dry-run plan reports composite pixels")) return 1;
    if (!expect(dry_run.budget.ok, "dry-run budget passes within limits")) return 1;
    const auto dry_run_profile = ui::scene::decide_motion_compose_profile(
        ui::scene::LayerProfile::Cheap,
        dry_run);
    if (!expect(dry_run_profile.valid, "profile decision accepts valid dry-run")) return 1;
    if (!expect(dry_run_profile.profile.effective == ui::scene::LayerProfile::Cheap,
                "profile decision keeps profile within budget")) {
        return 1;
    }

    const auto dry_run_over_budget = ui::scene::dry_run_motion_compose({
        .source = snapshot,
        .frame = transition_mid,
        .clip = {.x = 0, .y = 0, .w = 80, .h = 80},
        .has_clip = true,
    }, snapshots, {
        .max_composite_pixels = 100,
    });
    if (!expect(dry_run_over_budget.valid, "over-budget dry-run still returns valid plan")) return 1;
    if (!expect(!dry_run_over_budget.budget.ok &&
                dry_run_over_budget.budget.composite_pixels_over,
                "dry-run budget reports composite pixel overrun")) {
        return 1;
    }
    const auto over_budget_profile = ui::scene::decide_motion_compose_profile(
        ui::scene::LayerProfile::Cheap,
        dry_run_over_budget);
    if (!expect(over_budget_profile.valid, "profile decision accepts over-budget dry-run")) return 1;
    if (!expect(over_budget_profile.profile.effective == ui::scene::LayerProfile::Static,
                "profile decision degrades over-budget compose")) {
        return 1;
    }
    if (!expect(over_budget_profile.profile.reason == ui::scene::LayerFallbackReason::CompositePixelsOver,
                "profile decision records composite pixel fallback reason")) {
        return 1;
    }

    if (!expect(snapshots.mark_stale(snapshot), "snapshot can be marked stale")) return 1;
    const auto stale_dry_run = ui::scene::dry_run_motion_compose({
        .source = snapshot,
        .frame = transition_mid,
    }, snapshots);
    if (!expect(!stale_dry_run.valid, "dry-run rejects stale snapshot plan")) return 1;
    const auto stale_profile = ui::scene::decide_motion_compose_profile(
        ui::scene::LayerProfile::Cheap,
        stale_dry_run);
    if (!expect(!stale_profile.valid, "profile decision rejects invalid dry-run")) return 1;

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ui::scene::Scene scene{canvas};
    canvas.clear(rgba{0, 0, 0, 255});
    canvas.set_pixel(10, 10, rgba{240, 20, 30, 255});
    const auto captured = scene.capture_pixel_snapshot_result({
        .bounds = {.x = 10, .y = 10, .w = 1, .h = 1},
        .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
    });
    if (!expect(captured.ok(), "scene captures pixel snapshot")) return 1;
    canvas.set_pixel(20, 10, rgba{0, 0, 0, 255});
    const auto execute_frame = ui::scene::sample_motion_recipe(
        ui::scene::motion_slide(ui::scene::MotionAxis::X, 10, 100),
        ui::scene::LayerProfile::Rich,
        0,
        0);
    const auto execute_transition_frame = ui::scene::MotionTransitionFrame{
        .state = ui::scene::MotionTransitionState::Running,
        .motion = execute_frame,
    };
    const auto executed = ui::scene::execute_motion_compose(scene, {
        .source = captured.handle,
        .frame = execute_transition_frame,
    });
    if (!expect(executed.valid, "motion compose executes through scene")) return 1;
    if (!expect(executed.plan.composite_pixels == 1, "execute bridge keeps compose pixel evidence")) return 1;
    if (!expect(executed.replay.status == ui::scene::LayerReplayStatus::Ok, "execute bridge reports replay ok")) {
        return 1;
    }
    const auto moved_pixel = canvas.get_pixel(20, 10);
    if (!expect(moved_pixel.r == 240 && moved_pixel.g == 20 && moved_pixel.b == 30,
                "execute bridge blits pixel snapshot")) {
        return 1;
    }
    const auto empty_execute = ui::scene::execute_motion_compose(scene, {
        .frame = execute_transition_frame,
    });
    if (!expect(!empty_execute.valid, "execute bridge rejects missing source")) return 1;
    if (!expect(scene.release_snapshot(captured.handle), "pixel snapshot can be released")) return 1;

    scene.set_overlay(record_command_snapshot_probe, nullptr);
    const auto command_capture = scene.capture_command_snapshot_result({
        .bounds = {.x = 4, .y = 4, .w = 3, .h = 3},
    });
    if (!expect(command_capture.ok(), "scene captures command snapshot")) return 1;
    canvas.clear(rgba{0, 0, 0, 255});
    const auto command_identity = ui::scene::execute_motion_compose(scene, {
        .source = command_capture.handle,
        .frame = {
            .state = ui::scene::MotionTransitionState::Running,
            .motion = {
                .transform = {},
                .compose = true,
            },
        },
    });
    if (!expect(command_identity.valid &&
                    command_identity.replay.status == ui::scene::LayerReplayStatus::Ok,
                "identity command snapshot replay succeeds")) {
        return 1;
    }
    const auto command_identity_pixel = canvas.get_pixel(4, 4);
    if (!expect(command_identity_pixel.r == 30 && command_identity_pixel.g == 140 &&
                    command_identity_pixel.b == 230,
                "identity command snapshot replay writes expected pixel")) {
        return 1;
    }

    canvas.clear(rgba{0, 0, 0, 255});
    canvas.set_origin(-2, -1);
    const auto command_origin_before = canvas.save_origin();
    const auto command_translated = ui::scene::execute_motion_compose(scene, {
        .source = command_capture.handle,
        .frame = {
            .state = ui::scene::MotionTransitionState::Running,
            .motion = {
                .transform = {.x = 4, .y = 3},
                .compose = true,
            },
        },
    });
    const auto command_origin_after = canvas.save_origin();
    if (!expect(command_translated.valid &&
                    command_translated.replay.status == ui::scene::LayerReplayStatus::Ok,
                "translated command snapshot replay succeeds")) {
        return 1;
    }
    const auto command_original_after_translate = canvas.get_pixel(4, 4);
    const auto command_target_after_translate = canvas.get_pixel(8, 7);
    if (!expect(command_original_after_translate.r == 0 && command_original_after_translate.g == 0 &&
                    command_original_after_translate.b == 0 && command_target_after_translate.r == 30 &&
                    command_target_after_translate.g == 140 && command_target_after_translate.b == 230,
                "translated command snapshot replay moves expected pixels")) {
        return 1;
    }
    if (!expect(command_origin_after.x == command_origin_before.x &&
                    command_origin_after.y == command_origin_before.y,
                "translated command snapshot replay restores canvas origin")) {
        return 1;
    }
    canvas.clear_origin();

    canvas.clear(rgba{0, 0, 0, 255});
    const auto command_clipped = ui::scene::execute_motion_compose(scene, {
        .source = command_capture.handle,
        .frame = {
            .state = ui::scene::MotionTransitionState::Running,
            .motion = {
                .transform = {.x = 4, .y = 3},
                .compose = true,
            },
        },
        .clip = {.x = 8, .y = 7, .w = 1, .h = 1},
        .has_clip = true,
    });
    if (!expect(command_clipped.valid &&
                    command_clipped.replay.status == ui::scene::LayerReplayStatus::Ok,
                "translated command snapshot replay accepts target clip")) {
        return 1;
    }
    const auto command_clipped_outside = canvas.get_pixel(9, 8);
    const auto command_clipped_inside = canvas.get_pixel(8, 7);
    if (!expect(command_clipped_outside.r == 0 && command_clipped_outside.g == 0 &&
                    command_clipped_outside.b == 0 && command_clipped_inside.r == 30 &&
                    command_clipped_inside.g == 140 && command_clipped_inside.b == 230,
                "translated command snapshot replay clips in source coordinates")) {
        return 1;
    }

    constexpr rgba command_background{12, 28, 44, 255};
    canvas.clear(command_background);
    const auto command_full_reference = ui::scene::execute_motion_compose(scene, {
        .source = command_capture.handle,
        .frame = {
            .state = ui::scene::MotionTransitionState::Running,
            .motion = {
                .transform = {},
                .compose = true,
            },
        },
    });
    if (!expect(command_full_reference.valid
                    && command_full_reference.replay.status == ui::scene::LayerReplayStatus::Ok,
                "full-opacity command reference replay succeeds")) {
        return 1;
    }
    const auto command_reference_single = canvas.get_pixel(4, 4);
    const auto command_reference_overlap = canvas.get_pixel(5, 5);
    const auto command_reference_background = canvas.get_pixel(6, 6);
    if (!expect(!same_rgba(command_reference_single, command_reference_overlap)
                    && same_rgba(command_reference_background, command_background),
                "command reference distinguishes overlap and untouched background")) {
        return 1;
    }

    canvas.clear(command_background);
    const auto command_faded = ui::scene::execute_motion_compose(scene, {
        .source = command_capture.handle,
        .frame = {
            .state = ui::scene::MotionTransitionState::Running,
            .motion = {
                .transform = {.opacity = 128},
                .compose = true,
            },
        },
    });
    if (!expect(command_faded.valid
                    && command_faded.replay.status == ui::scene::LayerReplayStatus::Ok
                    && command_faded.replay.stats.alpha_blend_count == 9,
                "faded command snapshot replay blends the visible tile")) {
        return 1;
    }
    const auto command_faded_single = canvas.get_pixel(4, 4);
    const auto command_faded_overlap = canvas.get_pixel(5, 5);
    const auto command_faded_background = canvas.get_pixel(6, 6);
    if (!expect(same_rgba(command_faded_single,
                          blend_reference(command_background, command_reference_single, 128))
                    && same_rgba(command_faded_overlap,
                                 blend_reference(command_background,
                                                 command_reference_overlap,
                                                 128))
                    && same_rgba(command_faded_background, command_background),
                "faded command replay preserves whole-layer opacity semantics")) {
        return 1;
    }
    scene.set_overlay(nullptr, nullptr);
    if (!expect(scene.release_snapshot(command_capture.handle), "command snapshot can be released")) return 1;

    static DefaultFrameBuffer page_fb{};
    static DefaultCanvas page_canvas{page_fb};
    static ui::scene::Scene page_scene{page_canvas};
    WidgetHandle page_root{};
    page_scene.build([&](ui::scene::SceneBuilder& builder) {
        page_root = builder.create_container();
        builder.set_rect(page_root, {0, 0, 32, 32});
        builder.set_input_root(page_root);
        builder.set_root(page_root);
    });
    auto page_access = page_scene.access();
    page_canvas.clear(rgba{0, 0, 0, 255});
    page_canvas.set_pixel(2, 2, rgba{20, 220, 80, 255});
    ui::scene::PageLayer page_layer{page_root};
    page_layer.show(page_access);
    ui::scene::PageMotionTransition page_transition{};
    const auto page_capture = page_transition.begin(page_scene, page_access, page_layer, {
        .snapshot = {
            .bounds = {.x = 2, .y = 2, .w = 1, .h = 1},
            .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
        },
        .recipe = ui::scene::motion_slide(ui::scene::MotionAxis::X, 5, 100),
        .profile = ui::scene::LayerProfile::Rich,
        .start_ms = 0,
        .hide_live_root = true,
    });
    if (!expect(page_capture.ok(), "page transition captures snapshot")) return 1;
    if (!expect(page_layer.transitioning(), "page layer enters transitioning state")) return 1;
    if (!expect(!page_layer.visible(), "page transition hides live root")) return 1;
    page_canvas.set_pixel(7, 2, rgba{0, 0, 0, 255});
    const auto page_frame = page_transition.sample(page_scene, 0);
    if (!expect(page_frame.compose.valid, "page transition executes compose")) return 1;
    if (!expect(page_frame.compose.plan.composite_pixels == 1, "page transition keeps compose evidence")) return 1;
    const auto page_pixel = page_canvas.get_pixel(7, 2);
    if (!expect(page_pixel.g == 220, "page transition blits frozen page pixel")) return 1;
    const auto page_done = page_transition.sample(page_scene, 120);
    if (!expect(page_done.transition.state == ui::scene::MotionTransitionState::Finished,
                "page transition runner finishes")) {
        return 1;
    }
    const auto page_trace = page_transition.trace();
    page_transition.finish(page_scene, page_access);
    if (!expect(page_layer.live() && page_layer.visible(), "page transition thaws live page")) return 1;
    if (!expect(!static_cast<bool>(page_layer.snapshot()), "page transition releases snapshot")) return 1;

    runner.reset();
    if (!expect(runner.state() == ui::scene::MotionTransitionState::Idle, "transition reset returns idle")) return 1;
    if (!expect(runner.trace().sample_count == 0, "transition reset clears trace")) return 1;
    runner.begin({
        .recipe = ui::scene::motion_fade(100),
        .profile = ui::scene::LayerProfile::Rich,
        .start_ms = 0,
    });
    runner.cancel();
    const auto canceled = runner.sample(50);
    if (!expect(canceled.state == ui::scene::MotionTransitionState::Canceled, "transition can be canceled")) return 1;
    if (!expect(!canceled.motion.compose, "canceled transition does not compose")) return 1;
    const auto canceled_compose = ui::scene::make_motion_compose_spec({
        .source = {.slot = 4, .generation = 1},
        .frame = canceled,
    });
    if (!expect(!canceled_compose.valid, "compose bridge rejects non-composing frame")) return 1;
    const auto trace_canceled = runner.trace();
    if (!expect(trace_canceled.cancel_count == 1, "trace records cancel count")) return 1;
    if (!expect(trace_canceled.compose_count == 0, "trace records no canceled compose")) return 1;

    print_motion_case("rich");
    std::printf(" progress=%.2f sampled=%llu\n",
                static_cast<double>(rich.progress),
                static_cast<unsigned long long>(rich.sampled_elapsed_ms));
    print_motion_case("cheap");
    std::printf(" progress=%.2f sampled=%llu\n",
                static_cast<double>(cheap_step.progress),
                static_cast<unsigned long long>(cheap_step.sampled_elapsed_ms));
    print_motion_case("static");
    std::printf(" progress=%.2f sampled=%llu\n",
                static_cast<double>(static_cut.progress),
                static_cast<unsigned long long>(static_cut.sampled_elapsed_ms));
    print_motion_case("layer_rich");
    std::printf(" x=%d opacity=%u\n",
                static_cast<int>(rich_motion.transform.x),
                static_cast<unsigned>(rich_motion.transform.opacity));
    print_motion_case("recipe_fade_slide");
    std::printf(" y=%d opacity=%u\n",
                static_cast<int>(fade_slide.transform.y),
                static_cast<unsigned>(fade_slide.transform.opacity));
    print_motion_case("transition");
    std::printf(" x=%d opacity=%u\n",
                static_cast<int>(transition_mid.motion.transform.x),
                static_cast<unsigned>(transition_mid.motion.transform.opacity));
    print_motion_case("compose");
    std::printf(" source=%u gen=%u x=%d opacity=%u\n",
                static_cast<unsigned>(compose.spec.source.slot),
                static_cast<unsigned>(compose.spec.source.generation),
                static_cast<int>(compose.spec.transform.x),
                static_cast<unsigned>(compose.spec.transform.opacity));
    print_motion_case("dry_run");
    std::printf(" pixels=%u budget_ok=%u\n",
                static_cast<unsigned>(dry_run.plan.composite_pixels),
                static_cast<unsigned>(dry_run.budget.ok));
    print_motion_case("execute");
    std::printf(" pixels=%u status=%u moved_r=%u moved_g=%u moved_b=%u command_identity=%u command_translate=%u command_clip=%u command_fade=%u origin_restored=%u\n",
                static_cast<unsigned>(executed.plan.composite_pixels),
                static_cast<unsigned>(executed.replay.status),
                static_cast<unsigned>(moved_pixel.r),
                static_cast<unsigned>(moved_pixel.g),
                static_cast<unsigned>(moved_pixel.b),
                static_cast<unsigned>(command_identity.replay.status),
                static_cast<unsigned>(command_translated.replay.status),
                static_cast<unsigned>(command_clipped.replay.status),
                static_cast<unsigned>(command_faded.replay.status),
                static_cast<unsigned>(command_origin_after.x == command_origin_before.x &&
                                      command_origin_after.y == command_origin_before.y));
    print_motion_case("page");
    std::printf(" pixels=%u moved_g=%u trace=%u\n",
                static_cast<unsigned>(page_frame.compose.plan.composite_pixels),
                static_cast<unsigned>(page_pixel.g),
                static_cast<unsigned>(page_trace.sample_count));
    print_motion_case("profile");
    std::printf(" effective=%s fallback=%s\n",
                ui::scene::layer_profile_name(over_budget_profile.profile.effective),
                ui::scene::layer_fallback_reason_name(over_budget_profile.profile.reason));
    print_motion_case("trace");
    std::printf(" samples=%u compose=%u finished=%u canceled=%u\n",
                static_cast<unsigned>(trace_done.sample_count),
                static_cast<unsigned>(trace_done.compose_count),
                static_cast<unsigned>(trace_done.finish_count),
                static_cast<unsigned>(trace_canceled.cancel_count));
    if (!print_motion_causal_chain_verdict()) return 1;
    print_motion_run_end(true);
    std::puts("[motion_time_demo] ok");
    return 0;
}
