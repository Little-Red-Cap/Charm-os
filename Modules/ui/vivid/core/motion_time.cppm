module;

#include <cstdint>

export module charm.ui.scene.motion_time;

export namespace ui::scene {
    enum class MotionTier : std::uint8_t {
        Rich60Fps,
        Cheap30Fps,
        StaticCut,
        EinkDissolve,
        None,
    };

    struct MotionTimeSpec {
        MotionTier tier{MotionTier::Rich60Fps};
        std::uint64_t start_ms{0};
        std::uint64_t now_ms{0};
        std::uint32_t duration_ms{0};
    };

    struct MotionTick {
        MotionTier tier{MotionTier::Rich60Fps};
        std::uint64_t elapsed_ms{0};
        std::uint64_t sampled_elapsed_ms{0};
        float progress{0.0f};
        bool active{false};
        bool finished{false};
        bool should_sample{false};
    };

    constexpr const char* motion_tier_name(MotionTier tier) noexcept {
        switch (tier) {
        case MotionTier::Rich60Fps: return "rich_60fps";
        case MotionTier::Cheap30Fps: return "cheap_30fps";
        case MotionTier::StaticCut: return "static_cut";
        case MotionTier::EinkDissolve: return "eink_dissolve";
        case MotionTier::None: return "none";
        }
        return "unknown";
    }

    constexpr std::uint32_t motion_frame_ms(MotionTier tier) noexcept {
        switch (tier) {
        case MotionTier::Rich60Fps: return 16;
        case MotionTier::Cheap30Fps: return 33;
        case MotionTier::StaticCut: return 0;
        case MotionTier::EinkDissolve: return 250;
        case MotionTier::None: return 0;
        }
        return 0;
    }

    constexpr std::uint64_t elapsed_motion_ms(const MotionTimeSpec& spec) noexcept {
        return spec.now_ms > spec.start_ms ? spec.now_ms - spec.start_ms : 0;
    }

    constexpr std::uint64_t quantize_motion_elapsed(MotionTier tier,
                                                    std::uint64_t elapsed_ms,
                                                    std::uint32_t duration_ms) noexcept {
        if (duration_ms == 0) return 0;
        switch (tier) {
        case MotionTier::Rich60Fps:
            return elapsed_ms > duration_ms ? duration_ms : elapsed_ms;
        case MotionTier::Cheap30Fps: {
            if (elapsed_ms >= duration_ms) return duration_ms;
            constexpr std::uint64_t step_ms = 33;
            const auto quantized = (elapsed_ms / step_ms) * step_ms;
            return quantized > duration_ms ? duration_ms : quantized;
        }
        case MotionTier::StaticCut:
            return duration_ms;
        case MotionTier::EinkDissolve:
            return elapsed_ms >= duration_ms ? duration_ms : 0;
        case MotionTier::None:
            return duration_ms;
        }
        return 0;
    }

    constexpr float motion_progress(std::uint64_t sampled_elapsed_ms,
                                    std::uint32_t duration_ms) noexcept {
        if (duration_ms == 0) return 1.0f;
        if (sampled_elapsed_ms >= duration_ms) return 1.0f;
        return static_cast<float>(sampled_elapsed_ms) / static_cast<float>(duration_ms);
    }

    constexpr MotionTick sample_motion_time(const MotionTimeSpec& spec) noexcept {
        const auto elapsed = elapsed_motion_ms(spec);
        const auto sampled = quantize_motion_elapsed(spec.tier, elapsed, spec.duration_ms);
        const auto progress = motion_progress(sampled, spec.duration_ms);
        const bool finished = progress >= 1.0f;
        return {
            .tier = spec.tier,
            .elapsed_ms = elapsed,
            .sampled_elapsed_ms = sampled,
            .progress = progress,
            .active = !finished && spec.tier != MotionTier::None,
            .finished = finished,
            .should_sample = spec.tier != MotionTier::None,
        };
    }
}
