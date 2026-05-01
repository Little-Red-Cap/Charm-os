module;

#include <cstdint>

export module charm.ui.scene.motion_plan;

export import charm.ui.scene.layer_runtime;
export import charm.ui.scene.motion_time;

export namespace ui::scene {
    struct LayerMotionSpec {
        LayerProfile profile{LayerProfile::Rich};
        std::uint64_t start_ms{0};
        std::uint64_t now_ms{0};
        std::uint32_t duration_ms{0};
        LayerTransform from{};
        LayerTransform to{};
    };

    struct LayerMotionFrame {
        MotionTick tick{};
        LayerTransform transform{};
        bool compose{false};
    };

    constexpr MotionTier motion_tier_from_layer_profile(LayerProfile profile) noexcept {
        switch (profile) {
        case LayerProfile::Rich: return MotionTier::Rich60Fps;
        case LayerProfile::Cheap: return MotionTier::Cheap30Fps;
        case LayerProfile::Static: return MotionTier::StaticCut;
        case LayerProfile::Eink: return MotionTier::EinkDissolve;
        case LayerProfile::None: return MotionTier::None;
        }
        return MotionTier::None;
    }

    constexpr std::int16_t interpolate_layer_motion_i16(std::int16_t from,
                                                        std::int16_t to,
                                                        std::uint64_t sampled_elapsed_ms,
                                                        std::uint32_t duration_ms) noexcept {
        if (duration_ms == 0 || sampled_elapsed_ms >= duration_ms) return to;
        const auto delta = static_cast<std::int32_t>(to) - static_cast<std::int32_t>(from);
        const auto numerator =
            static_cast<std::int64_t>(delta) * static_cast<std::int64_t>(sampled_elapsed_ms);
        const auto half_duration = static_cast<std::int64_t>(duration_ms / 2u);
        const auto rounded = numerator >= 0
            ? (numerator + half_duration) / static_cast<std::int64_t>(duration_ms)
            : (numerator - half_duration) / static_cast<std::int64_t>(duration_ms);
        return static_cast<std::int16_t>(static_cast<std::int32_t>(from) + static_cast<std::int32_t>(rounded));
    }

    constexpr std::uint8_t interpolate_layer_motion_u8(std::uint8_t from,
                                                       std::uint8_t to,
                                                       std::uint64_t sampled_elapsed_ms,
                                                       std::uint32_t duration_ms) noexcept {
        if (duration_ms == 0 || sampled_elapsed_ms >= duration_ms) return to;
        const auto delta = static_cast<std::int16_t>(to) - static_cast<std::int16_t>(from);
        const auto numerator =
            static_cast<std::int64_t>(delta) * static_cast<std::int64_t>(sampled_elapsed_ms);
        const auto half_duration = static_cast<std::int64_t>(duration_ms / 2u);
        const auto rounded = numerator >= 0
            ? (numerator + half_duration) / static_cast<std::int64_t>(duration_ms)
            : (numerator - half_duration) / static_cast<std::int64_t>(duration_ms);
        return static_cast<std::uint8_t>(static_cast<std::int16_t>(from) + static_cast<std::int16_t>(rounded));
    }

    constexpr LayerTransform sample_layer_motion_transform(
        LayerProfile profile,
        const LayerTransform& from,
        const LayerTransform& to,
        const MotionTick& tick,
        std::uint32_t duration_ms) noexcept {
        const auto sampled = tick.sampled_elapsed_ms;
        const auto requested_opacity =
            interpolate_layer_motion_u8(from.opacity, to.opacity, sampled, duration_ms);
        return {
            .x = interpolate_layer_motion_i16(from.x, to.x, sampled, duration_ms),
            .y = interpolate_layer_motion_i16(from.y, to.y, sampled, duration_ms),
            .opacity = resolve_layer_opacity(profile, requested_opacity),
        };
    }

    constexpr LayerMotionFrame sample_layer_motion(const LayerMotionSpec& spec) noexcept {
        const auto tier = motion_tier_from_layer_profile(spec.profile);
        const auto tick = sample_motion_time({
            .tier = tier,
            .start_ms = spec.start_ms,
            .now_ms = spec.now_ms,
            .duration_ms = spec.duration_ms,
        });
        const auto transform =
            sample_layer_motion_transform(spec.profile, spec.from, spec.to, tick, spec.duration_ms);
        return {
            .tick = tick,
            .transform = transform,
            .compose = tick.should_sample && transform.opacity != 0,
        };
    }
}
