#include <cstdio>
#include <cstdint>

import charm.ui.vivid;

namespace {
    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
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

    if (!expect(snapshots.mark_stale(snapshot), "snapshot can be marked stale")) return 1;
    const auto stale_dry_run = ui::scene::dry_run_motion_compose({
        .source = snapshot,
        .frame = transition_mid,
    }, snapshots);
    if (!expect(!stale_dry_run.valid, "dry-run rejects stale snapshot plan")) return 1;

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

    std::printf("[motion] rich progress=%.2f sampled=%llu\n",
                static_cast<double>(rich.progress),
                static_cast<unsigned long long>(rich.sampled_elapsed_ms));
    std::printf("[motion] cheap progress=%.2f sampled=%llu\n",
                static_cast<double>(cheap_step.progress),
                static_cast<unsigned long long>(cheap_step.sampled_elapsed_ms));
    std::printf("[motion] static progress=%.2f sampled=%llu\n",
                static_cast<double>(static_cut.progress),
                static_cast<unsigned long long>(static_cut.sampled_elapsed_ms));
    std::printf("[motion] layer rich x=%d opacity=%u\n",
                static_cast<int>(rich_motion.transform.x),
                static_cast<unsigned>(rich_motion.transform.opacity));
    std::printf("[motion] recipe fade_slide y=%d opacity=%u\n",
                static_cast<int>(fade_slide.transform.y),
                static_cast<unsigned>(fade_slide.transform.opacity));
    std::printf("[motion] transition x=%d opacity=%u\n",
                static_cast<int>(transition_mid.motion.transform.x),
                static_cast<unsigned>(transition_mid.motion.transform.opacity));
    std::printf("[motion] compose source=%u gen=%u x=%d opacity=%u\n",
                static_cast<unsigned>(compose.spec.source.slot),
                static_cast<unsigned>(compose.spec.source.generation),
                static_cast<int>(compose.spec.transform.x),
                static_cast<unsigned>(compose.spec.transform.opacity));
    std::printf("[motion] dry_run pixels=%u budget_ok=%u\n",
                static_cast<unsigned>(dry_run.plan.composite_pixels),
                static_cast<unsigned>(dry_run.budget.ok));
    std::printf("[motion] trace samples=%u compose=%u finished=%u canceled=%u\n",
                static_cast<unsigned>(trace_done.sample_count),
                static_cast<unsigned>(trace_done.compose_count),
                static_cast<unsigned>(trace_done.finish_count),
                static_cast<unsigned>(trace_canceled.cancel_count));
    std::puts("[motion_time_demo] ok");
    return 0;
}
