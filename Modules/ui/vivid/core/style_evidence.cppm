module;
#include <cstdint>

export module charm.core.style_evidence;

export import charm.core.style_sheet;
export import charm.core.style_impact;

export
struct ResolvedStyleEvidence {
    std::uint32_t color_hash{0};
    std::uint32_t metrics_hash{0};
    std::uint32_t style_key{0};
};

export
struct StyleStateEvidence {
    WidgetKind kind{WidgetKind::None};
    std::uint8_t mask{0};
    std::uint8_t state_count{0};
    bool includes_hovered{false};
    bool includes_pressed{false};
    bool includes_disabled{false};
    bool includes_focused{false};
};

export
inline constexpr std::uint32_t style_evidence_hash_mix(std::uint32_t hash,
                                                       std::uint32_t value) noexcept {
    hash ^= value;
    hash *= 16777619u;
    return hash;
}

export
inline constexpr std::uint32_t hash_style_color(std::uint32_t hash, rgba color) noexcept {
    hash = style_evidence_hash_mix(hash, color.r);
    hash = style_evidence_hash_mix(hash, color.g);
    hash = style_evidence_hash_mix(hash, color.b);
    hash = style_evidence_hash_mix(hash, color.a);
    return hash;
}

export
inline std::uint32_t hash_resolved_style_colors(const ResolvedColors* colors) noexcept {
    std::uint32_t hash = 2166136261u;
    if (!colors) return hash;
    hash = hash_style_color(hash, colors->bg);
    hash = hash_style_color(hash, colors->border);
    hash = hash_style_color(hash, colors->font);
    hash = hash_style_color(hash, colors->accent);
    hash = hash_style_color(hash, colors->on_accent);
    hash = hash_style_color(hash, colors->border_focus);
    hash = hash_style_color(hash, colors->gradient_start);
    hash = hash_style_color(hash, colors->gradient_end);
    return hash;
}

inline std::uint32_t hash_resolved_style_decoration(const ResolvedDecoration* decoration) noexcept {
    std::uint32_t hash = 2166136261u;
    if (!decoration) return hash;
    hash = hash_style_color(hash, decoration->shadow_color);
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(decoration->shadow_offset_x));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(decoration->shadow_offset_y));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(decoration->shadow_spread));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(decoration->shadow_radius));
    hash = hash_style_color(hash, decoration->inner_stroke_color);
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(decoration->inner_stroke_width));
    hash = hash_style_color(hash, decoration->outline_color);
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(decoration->outline_width));
    hash = style_evidence_hash_mix(hash, decoration->shadow_enabled);
    hash = style_evidence_hash_mix(hash, decoration->inner_stroke_enabled);
    hash = style_evidence_hash_mix(hash, decoration->outline_enabled);
    hash = style_evidence_hash_mix(hash, resolved_decoration_gradient_enabled(*decoration) ? 1u : 0u);
    hash = style_evidence_hash_mix(hash, resolved_decoration_gradient_direction(*decoration));
    return hash;
}

export
inline std::uint32_t hash_resolved_style_metrics(const ResolvedMetrics* metrics) noexcept {
    std::uint32_t hash = 2166136261u;
    if (!metrics) return hash;
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->font_role));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->font_weight));
    hash = style_evidence_hash_mix(hash, metrics->font_explicit ? 1u : 0u);
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->border_width));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->corner_radius));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->padding));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->header_padding));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->content_padding));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->scrollbar_margin));
    hash = style_evidence_hash_mix(hash, static_cast<std::uint32_t>(metrics->scrollbar_thumb_min));
    return hash;
}

export
inline ResolvedStyleEvidence make_resolved_style_evidence(const ResolvedStyleView& style) noexcept {
    ResolvedStyleEvidence evidence{};
    evidence.color_hash = hash_resolved_style_colors(style.colors);
    evidence.color_hash = style_evidence_hash_mix(evidence.color_hash, hash_resolved_style_decoration(style.decoration));
    evidence.metrics_hash = hash_resolved_style_metrics(style.metrics);
    evidence.style_key = style_evidence_hash_mix(evidence.color_hash, evidence.metrics_hash);
    return evidence;
}

export
inline StyleStateEvidence make_style_state_evidence(WidgetKind kind) noexcept {
    const std::uint8_t mask = style_kind_state_mask(kind);
    const auto hovered = static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    const auto pressed = static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    const auto focused = static_cast<std::uint8_t>(StyleStateFlag::Focused);
    const auto disabled = static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    return StyleStateEvidence{
        .kind = kind,
        .mask = mask,
        .state_count = style_kind_state_count(kind),
        .includes_hovered = (mask & hovered) != 0,
        .includes_pressed = (mask & pressed) != 0,
        .includes_disabled = (mask & disabled) != 0,
        .includes_focused = (mask & focused) != 0,
    };
}

export
inline bool style_metrics_evidence_equal(const ResolvedStyleEvidence& lhs,
                                         const ResolvedStyleEvidence& rhs) noexcept {
    return lhs.metrics_hash == rhs.metrics_hash;
}

export
inline bool style_color_evidence_equal(const ResolvedStyleEvidence& lhs,
                                       const ResolvedStyleEvidence& rhs) noexcept {
    return lhs.color_hash == rhs.color_hash;
}

export
inline bool style_evidence_matches_impact(const ResolvedStyleEvidence& before,
                                          const ResolvedStyleEvidence& after,
                                          const StyleImpactDecision& impact) noexcept {
    const bool paint_changed = !style_color_evidence_equal(before, after);
    const bool metrics_changed = !style_metrics_evidence_equal(before, after);
    if (impact.has(StyleInvalidationImpact::TextMetrics) || impact.has(StyleInvalidationImpact::Layout)) {
        return paint_changed || metrics_changed;
    }
    if (impact.has(StyleInvalidationImpact::PaintOnly)) {
        return paint_changed && !metrics_changed;
    }
    return before.style_key == after.style_key;
}

export
inline bool style_state_evidence_matches_interactive_law(const StyleStateEvidence& evidence) noexcept {
    return evidence.includes_hovered
        && evidence.includes_pressed
        && evidence.includes_disabled
        && !evidence.includes_focused
        && evidence.state_count >= 1;
}
