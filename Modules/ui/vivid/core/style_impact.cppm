module;
#include <cstdint>

export module charm.core.style_impact;

export
enum class StyleTokenDomain : std::uint8_t {
    None,
    Color,
    Spacing,
    Font,
    Radius,
    BorderWidth,
    Decoration,
};

export
enum class StyleInvalidationImpact : std::uint8_t {
    None = 0,
    PaintOnly = 1 << 0,
    Layout = 1 << 1,
    TextMetrics = 1 << 2,
    RenderCache = 1 << 3,
};

export
struct StyleImpactDecision {
    StyleTokenDomain domain{StyleTokenDomain::None};
    std::uint8_t impact_mask{static_cast<std::uint8_t>(StyleInvalidationImpact::None)};

    [[nodiscard]] bool has(StyleInvalidationImpact impact) const noexcept {
        return (impact_mask & static_cast<std::uint8_t>(impact)) != 0;
    }
};

export
inline constexpr std::uint8_t style_impact_mask(StyleInvalidationImpact impact) noexcept {
    return static_cast<std::uint8_t>(impact);
}

export
inline constexpr std::uint8_t style_impact_mask(StyleInvalidationImpact first,
                                                StyleInvalidationImpact second) noexcept {
    return static_cast<std::uint8_t>(style_impact_mask(first) | style_impact_mask(second));
}

export
inline constexpr StyleImpactDecision decide_style_token_impact(StyleTokenDomain domain) noexcept {
    switch (domain) {
    case StyleTokenDomain::None:
        return StyleImpactDecision{domain, style_impact_mask(StyleInvalidationImpact::None)};
    case StyleTokenDomain::Color:
    case StyleTokenDomain::Decoration:
        return StyleImpactDecision{domain, style_impact_mask(StyleInvalidationImpact::PaintOnly)};
    case StyleTokenDomain::Spacing:
    case StyleTokenDomain::BorderWidth:
    case StyleTokenDomain::Radius:
        return StyleImpactDecision{
            domain,
            style_impact_mask(StyleInvalidationImpact::PaintOnly, StyleInvalidationImpact::Layout),
        };
    case StyleTokenDomain::Font:
        return StyleImpactDecision{
            domain,
            style_impact_mask(StyleInvalidationImpact::PaintOnly)
                | style_impact_mask(StyleInvalidationImpact::TextMetrics)
                | style_impact_mask(StyleInvalidationImpact::Layout),
        };
    }
    return StyleImpactDecision{StyleTokenDomain::None, style_impact_mask(StyleInvalidationImpact::None)};
}

export
inline constexpr const char* style_token_domain_name(StyleTokenDomain domain) noexcept {
    switch (domain) {
    case StyleTokenDomain::None: return "none";
    case StyleTokenDomain::Color: return "color";
    case StyleTokenDomain::Spacing: return "spacing";
    case StyleTokenDomain::Font: return "font";
    case StyleTokenDomain::Radius: return "radius";
    case StyleTokenDomain::BorderWidth: return "border_width";
    case StyleTokenDomain::Decoration: return "decoration";
    }
    return "unknown";
}

export
inline constexpr const char* style_impact_primary_name(const StyleImpactDecision& decision) noexcept {
    if (decision.impact_mask == style_impact_mask(StyleInvalidationImpact::PaintOnly)) {
        return "paint_only";
    }
    if (decision.impact_mask == style_impact_mask(StyleInvalidationImpact::None)) {
        return "none";
    }
    if (decision.has(StyleInvalidationImpact::TextMetrics)) {
        return "text_metrics_layout";
    }
    if (decision.has(StyleInvalidationImpact::Layout)) {
        return "layout";
    }
    if (decision.has(StyleInvalidationImpact::RenderCache)) {
        return "render_cache";
    }
    return "mixed";
}
