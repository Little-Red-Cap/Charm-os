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

    std::printf("[motion] rich progress=%.2f sampled=%llu\n",
                static_cast<double>(rich.progress),
                static_cast<unsigned long long>(rich.sampled_elapsed_ms));
    std::printf("[motion] cheap progress=%.2f sampled=%llu\n",
                static_cast<double>(cheap_step.progress),
                static_cast<unsigned long long>(cheap_step.sampled_elapsed_ms));
    std::printf("[motion] static progress=%.2f sampled=%llu\n",
                static_cast<double>(static_cut.progress),
                static_cast<unsigned long long>(static_cut.sampled_elapsed_ms));
    std::puts("[motion_time_demo] ok");
    return 0;
}
