// gui.motion.cppm
// Animation utilities (value objects, no allocation).

module;
#include <cassert>
#include <cstdint>

export module gui.motion;

export namespace gui::motion {
    enum class EaseKind : std::uint8_t {
        Linear = 0,
        Smoothstep = 1,
    };

    enum class SpringPreset : std::uint8_t {
        Default = 0,
        Critical = 1,
        CriticalFast = 2,
        Over = 3,
        Under = 4,
        Custom = 5,
    };

    struct Spring1D {
        float value{0.0f};
        float velocity{0.0f};
        float target{0.0f};
        float omega{10.0f}; // rad/s
        float zeta{0.7f};   // damping ratio
        bool valid{false};

        inline void reset(float v) noexcept
        {
            value = v;
            target = v;
            velocity = 0.0f;
            valid = true;
        }

        inline void set_params(float w, float z) noexcept
        {
            if (w < 0.1f) w = 0.1f;
            if (w > 40.0f) w = 40.0f;
            if (z < 0.0f) z = 0.0f;
            if (z > 2.0f) z = 2.0f;
            omega = w;
            zeta = z;
        }

        inline void step(float dt) noexcept
        {
            if (!valid) return;
            if (dt <= 0.0f) return;
            if (dt > 0.05f) dt = 0.05f;
            const float w = omega;
            const float z = zeta;
            const float x = value;
            const float v = velocity;
            const float f = (w * w) * (target - x) - (2.0f * z * w) * v;
            const float v1 = v + f * dt;
            const float x1 = x + v1 * dt;
            velocity = v1;
            value = x1;
        }

        inline void step_ms(std::uint32_t dt_ms) noexcept
        {
            step((float)dt_ms * 0.001f);
        }
    };

    [[nodiscard]] inline std::uint16_t ease_u16(std::uint16_t t, EaseKind kind) noexcept
    {
        if (kind == EaseKind::Linear) return t;
        const std::uint32_t tt = (std::uint32_t)t;
        const std::uint32_t t2 = (tt * tt) / 1024u;
        const std::uint32_t t3 = (t2 * tt) / 1024u;
        const std::uint32_t smooth = 3u * t2 - 2u * t3;
        return (std::uint16_t)((smooth > 1024u) ? 1024u : smooth);
    }

    struct Anim1D {
        std::int16_t value{0};
        std::int16_t from{0};
        std::int16_t target{0};
        std::uint32_t start_ms{0};
        std::uint16_t duration_ms{0};
        bool running{false};
        EaseKind ease{EaseKind::Smoothstep};

        // duration_ms == 0 => running == false and value/from/target equal
        inline void set_target(std::int16_t new_target,
                               std::uint32_t now_ms,
                               std::uint16_t dur_ms,
                               bool snap) noexcept
        {
            if (snap || dur_ms == 0) {
                value = new_target;
                from = new_target;
                target = new_target;
                start_ms = now_ms;
                duration_ms = 0;
                running = false;
                return;
            }

            // Avoid restarting when target stays the same and we're already running.
            if (running && new_target == target) {
                return;
            }

            if (new_target == value) {
                value = new_target;
                from = new_target;
                target = new_target;
                start_ms = now_ms;
                duration_ms = 0;
                running = false;
                return;
            }

            from = value;
            target = new_target;
            start_ms = now_ms;
            duration_ms = (dur_ms == 0) ? 1 : dur_ms;
            running = true;
        }

        inline void step(std::uint32_t now_ms) noexcept
        {
#ifndef NDEBUG
            assert(!running || duration_ms > 0);
            assert(duration_ms != 0 || !running);
#endif
            if (!running) return;

            const std::uint32_t elapsed = now_ms - start_ms;
            if (elapsed >= duration_ms) {
                value = target;
                running = false;
                return;
            }

            const int diff = (int)target - (int)from;
            const std::uint32_t t = (std::uint32_t)(elapsed * 1024u / duration_ms);
            const std::uint16_t te = ease_u16((std::uint16_t)t, ease);
            const int step = (int)((int64_t)diff * (int)te / 1024);
            value = (std::int16_t)((int)from + step);
        }
    };

    // Apply common animation policy to Anim1D (duration/snap/curve).
    inline void apply_anim(Anim1D& a,
                           std::int16_t target,
                           std::uint32_t now_ms,
                           std::uint16_t duration_ms,
                           bool snap,
                           EaseKind curve) noexcept
    {
        a.ease = curve;
        a.set_target(target, now_ms, duration_ms, snap);
        a.step(now_ms);
    }

    struct AnimChannel {
        std::uint16_t base_ms{160};
        std::uint16_t jump_scale_pct{50}; // only used by list-like channels
        EaseKind curve{EaseKind::Smoothstep};
        std::uint16_t min_ms{40};
    };

    // Shared animation profile (channelized durations + easing).
    struct AnimProfile {
        AnimChannel list{ AnimChannel{160, 50, EaseKind::Smoothstep, 40} };
        AnimChannel win { AnimChannel{200, 100, EaseKind::Smoothstep, 0} };
        AnimChannel spot{ AnimChannel{120, 100, EaseKind::Smoothstep, 0} };
        AnimChannel highlight{ AnimChannel{160, 50, EaseKind::Smoothstep, 40} };
        SpringPreset spring_preset{SpringPreset::Default};
        bool spring_override{false};
        float spring_omega{10.0f};
        float spring_zeta{0.8f};
    };

    enum class AnimPreset : std::uint8_t {
        Default = 0,
        Snappy = 1,
        Soft = 2,
        Custom = 3,
    };

    struct AnimPresetInfo {
        AnimPreset preset{};
        const char* label{""};
    };

    struct SpringPresetInfo {
        SpringPreset preset{};
        const char* label{""};
    };

    inline constexpr AnimPresetInfo kAnimPresets[] = {
        { AnimPreset::Default, "Default" },
        { AnimPreset::Snappy,  "Snappy" },
        { AnimPreset::Soft,    "Soft" },
        { AnimPreset::Custom,  "Custom" },
    };

    inline constexpr SpringPresetInfo kSpringPresets[] = {
        { SpringPreset::Default,      "Default" },
        { SpringPreset::Critical,     "Critical" },
        { SpringPreset::CriticalFast, "Crit Fast" },
        { SpringPreset::Over,         "Over" },
        { SpringPreset::Under,        "Under" },
        { SpringPreset::Custom,       "Custom" },
    };

    [[nodiscard]] constexpr std::uint16_t anim_preset_count() noexcept
    {
        return (std::uint16_t)(sizeof(kAnimPresets) / sizeof(kAnimPresets[0]));
    }

    [[nodiscard]] constexpr std::uint16_t spring_preset_count() noexcept
    {
        return (std::uint16_t)(sizeof(kSpringPresets) / sizeof(kSpringPresets[0]));
    }

    [[nodiscard]] inline std::uint16_t anim_preset_index(AnimPreset preset) noexcept
    {
        for (std::uint16_t i = 0; i < anim_preset_count(); ++i) {
            if (kAnimPresets[i].preset == preset) return i;
        }
        return 0;
    }

    [[nodiscard]] inline std::uint16_t spring_preset_index(SpringPreset preset) noexcept
    {
        for (std::uint16_t i = 0; i < spring_preset_count(); ++i) {
            if (kSpringPresets[i].preset == preset) return i;
        }
        return 0;
    }

    [[nodiscard]] inline AnimPreset anim_preset_at(std::uint16_t idx) noexcept
    {
        if (idx >= anim_preset_count()) idx = 0;
        return kAnimPresets[idx].preset;
    }

    [[nodiscard]] inline SpringPreset spring_preset_at(std::uint16_t idx) noexcept
    {
        if (idx >= spring_preset_count()) idx = 0;
        return kSpringPresets[idx].preset;
    }

    inline void spring_preset_params(SpringPreset preset, float& omega, float& zeta) noexcept
    {
        if (preset == SpringPreset::Custom) return;
        struct Params { float omega; float zeta; };
        constexpr Params kParams[] = {
            {10.0f, 0.8f}, // Default
            {10.0f, 1.0f}, // Critical
            {16.0f, 1.0f}, // CriticalFast
            {10.0f, 1.4f}, // Over
            {10.0f, 0.6f}, // Under
        };
        const std::uint16_t idx = spring_preset_index(preset);
        if (idx >= (std::uint16_t)(sizeof(kParams) / sizeof(kParams[0]))) return;
        omega = kParams[idx].omega;
        zeta = kParams[idx].zeta;
    }

    [[nodiscard]] inline const char* anim_preset_label(AnimPreset preset) noexcept
    {
        const std::uint16_t idx = anim_preset_index(preset);
        return kAnimPresets[(idx < anim_preset_count()) ? idx : 0].label;
    }

    [[nodiscard]] inline AnimPreset next_anim_preset(AnimPreset preset) noexcept
    {
        const std::uint16_t idx = anim_preset_index(preset);
        return kAnimPresets[(idx + 1) % anim_preset_count()].preset;
    }

    [[nodiscard]] inline const char* spring_preset_label(SpringPreset preset) noexcept
    {
        const std::uint16_t idx = spring_preset_index(preset);
        return kSpringPresets[(idx < spring_preset_count()) ? idx : 0].label;
    }

    [[nodiscard]] inline SpringPreset next_spring_preset(SpringPreset preset) noexcept
    {
        const std::uint16_t idx = spring_preset_index(preset);
        return kSpringPresets[(idx + 1) % spring_preset_count()].preset;
    }

    [[nodiscard]] inline AnimProfile make_profile(AnimPreset preset) noexcept
    {
        switch (preset) {
        case AnimPreset::Snappy:
            return AnimProfile{
                AnimChannel{100, 50, EaseKind::Smoothstep, 30},
                AnimChannel{140, 100, EaseKind::Smoothstep, 0},
                AnimChannel{80, 100, EaseKind::Smoothstep, 0},
                AnimChannel{100, 50, EaseKind::Smoothstep, 30},
            };
        case AnimPreset::Soft:
            return AnimProfile{
                AnimChannel{240, 60, EaseKind::Smoothstep, 60},
                AnimChannel{320, 100, EaseKind::Smoothstep, 0},
                AnimChannel{200, 100, EaseKind::Smoothstep, 0},
                AnimChannel{240, 60, EaseKind::Smoothstep, 60},
            };
        case AnimPreset::Custom:
        case AnimPreset::Default:
        default:
            return AnimProfile{};
        }
    }

    inline void apply_preset(AnimProfile& profile, AnimPreset preset) noexcept
    {
        const auto spring_preset = profile.spring_preset;
        const auto spring_override = profile.spring_override;
        const auto spring_omega = profile.spring_omega;
        const auto spring_zeta = profile.spring_zeta;
        profile = make_profile(preset);
        profile.spring_preset = spring_preset;
        profile.spring_override = spring_override;
        profile.spring_omega = spring_omega;
        profile.spring_zeta = spring_zeta;
    }

    struct AnimParams {
        std::uint16_t duration{0};
        bool snap{true};
        EaseKind curve{EaseKind::Smoothstep};
    };

    enum class AnimChannelId : std::uint8_t {
        List = 0,
        Win = 1,
        Spot = 2,
        Highlight = 3,
    };

    [[nodiscard]] inline const AnimChannel& get_channel(const AnimProfile& profile,
                                                        AnimChannelId id) noexcept
    {
        switch (id) {
        case AnimChannelId::Win:       return profile.win;
        case AnimChannelId::Spot:      return profile.spot;
        case AnimChannelId::Highlight: return profile.highlight;
        case AnimChannelId::List:
        default:                       return profile.list;
        }
    }

    [[nodiscard]] inline AnimParams channel_params(const AnimProfile& profile,
                                                   AnimChannelId id,
                                                   bool enabled,
                                                   bool jump,
                                                   bool navigating) noexcept
    {
        AnimParams out{};
        const auto& ch = get_channel(profile, id);
        out.curve = ch.curve;
        if (!enabled) {
            out.duration = 0;
            out.snap = true;
            return out;
        }
        std::uint16_t d = ch.base_ms;
        if ((id == AnimChannelId::List || id == AnimChannelId::Highlight) && jump && ch.jump_scale_pct > 0) {
            d = (std::uint16_t)((d * ch.jump_scale_pct) / 100);
        }
        if (ch.min_ms > 0 && d < ch.min_ms) d = ch.min_ms;
        out.duration = d;
        out.snap = (!navigating) || (out.duration == 0);
        return out;
    }

    [[nodiscard]] inline AnimParams make_params(const AnimChannel& channel,
                                                bool enabled,
                                                std::uint16_t duration,
                                                bool snap) noexcept
    {
        AnimParams out{};
        out.curve = channel.curve;
        out.duration = enabled ? duration : 0;
        out.snap = (!enabled) ? true : snap;
        if (out.duration == 0) out.snap = true;
        return out;
    }

    [[nodiscard]] inline AnimParams list_params(const AnimProfile& profile,
                                                bool enabled,
                                                bool jump,
                                                bool navigating) noexcept
    {
        AnimParams out{};
        const auto& ch = profile.list;
        out.curve = ch.curve;
        if (!enabled) {
            out.duration = 0;
            out.snap = true;
            return out;
        }
        std::uint16_t d = ch.base_ms;
        if (jump && ch.jump_scale_pct > 0) {
            d = (std::uint16_t)((d * ch.jump_scale_pct) / 100);
        }
        if (ch.min_ms > 0 && d < ch.min_ms) d = ch.min_ms;
        out.duration = d;
        out.snap = (!navigating) || (out.duration == 0);
        return out;
    }

    [[nodiscard]] inline AnimParams win_params(const AnimProfile& profile,
                                               bool enabled) noexcept
    {
        const auto& ch = profile.win;
        const std::uint16_t d = enabled ? ch.base_ms : 0;
        return make_params(ch, enabled, d, false);
    }

    [[nodiscard]] inline AnimParams spot_params(const AnimProfile& profile,
                                                bool enabled) noexcept
    {
        const auto& ch = profile.spot;
        const std::uint16_t d = enabled ? ch.base_ms : 0;
        return make_params(ch, enabled, d, false);
    }

    [[nodiscard]] inline AnimParams highlight_params(const AnimProfile& profile,
                                                     bool enabled,
                                                     bool jump,
                                                     bool navigating) noexcept
    {
        AnimParams out{};
        const auto& ch = profile.highlight;
        out.curve = ch.curve;
        if (!enabled) {
            out.duration = 0;
            out.snap = true;
            return out;
        }
        std::uint16_t d = ch.base_ms;
        if (jump && ch.jump_scale_pct > 0) {
            d = (std::uint16_t)((d * ch.jump_scale_pct) / 100);
        }
        if (ch.min_ms > 0 && d < ch.min_ms) d = ch.min_ms;
        out.duration = d;
        out.snap = (!navigating) || (out.duration == 0);
        return out;
    }

    struct LeadTrail1D {
        Anim1D lead{};
        Anim1D trail{};
        bool valid{false};

        inline void reset() noexcept
        {
            valid = false;
            lead = {};
            trail = {};
        }

        inline void set_targets(std::int16_t lead_target,
                                std::int16_t trail_target,
                                std::uint32_t now_ms,
                                std::uint16_t lead_ms,
                                std::uint16_t trail_ms,
                                bool snap) noexcept
        {
            if (!valid) {
                valid = true;
                lead.set_target(lead_target, now_ms, 0, true);
                trail.set_target(trail_target, now_ms, 0, true);
            }

            lead.set_target(lead_target, now_ms, lead_ms, snap);
            trail.set_target(trail_target, now_ms, trail_ms, snap);
        }

        inline void step(std::uint32_t now_ms) noexcept
        {
            if (!valid) return;
            lead.step(now_ms);
            trail.step(now_ms);
        }

        [[nodiscard]] inline std::int16_t top() const noexcept { return lead.value; }
        [[nodiscard]] inline std::int16_t bottom() const noexcept { return trail.value; }
    };

    struct LeadTrailFollow1D {
        LeadTrail1D anim{};
        std::int16_t last_target_top{0};
        bool target_valid{false};

        inline void reset() noexcept
        {
            anim.reset();
            last_target_top = 0;
            target_valid = false;
        }

        inline void set_curve(EaseKind kind) noexcept
        {
            anim.lead.ease = kind;
            anim.trail.ease = kind;
        }

        inline void set_target(std::int16_t target_top,
                               std::int16_t target_bottom,
                               std::uint32_t now_ms,
                               std::uint16_t fast_ms,
                               std::uint16_t slow_ms,
                               bool snap) noexcept
        {
            int dy = 0;
            if (target_valid) {
                dy = target_top - last_target_top;
            } else {
                target_valid = true;
            }
            last_target_top = target_top;

            const bool lead_bottom = (dy > 0);
            const std::uint16_t lead_ms = lead_bottom ? slow_ms : fast_ms;
            const std::uint16_t trail_ms = lead_bottom ? fast_ms : slow_ms;
            anim.set_targets(target_top, target_bottom, now_ms, lead_ms, trail_ms, snap);
        }

        inline void step(std::uint32_t now_ms) noexcept
        {
            anim.step(now_ms);
        }

        [[nodiscard]] inline std::int16_t top() const noexcept { return anim.top(); }
        [[nodiscard]] inline std::int16_t bottom() const noexcept { return anim.bottom(); }
        [[nodiscard]] inline bool running() const noexcept { return anim.lead.running || anim.trail.running; }
    };

    // Apply common animation policy to LeadTrailFollow1D.
    inline void apply_lead_trail(LeadTrailFollow1D& a,
                                 std::int16_t target_top,
                                 std::int16_t target_bottom,
                                 std::uint32_t now_ms,
                                 std::uint16_t fast_ms,
                                 std::uint16_t slow_ms,
                                 bool snap,
                                 EaseKind curve) noexcept
    {
        a.set_curve(curve);
        a.set_target(target_top, target_bottom, now_ms, fast_ms, slow_ms, snap);
        a.step(now_ms);
    }

} // namespace gui::motion
