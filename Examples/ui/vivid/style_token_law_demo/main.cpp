#include <cstdint>
#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.style_impact;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kButtonBounds{16, 16, 128, 36};
    constexpr vivid::evidence::RunLog kRunLog{"stl", "style_token_law_demo"};

    [[nodiscard]] ThemeTokens make_tokens(rgba accent, rgba on_accent) noexcept {
        ThemeTokens tokens{};
        tokens.surface = rgba{244, 244, 248, 255};
        tokens.surface_variant = rgba{230, 232, 238, 255};
        tokens.on_surface = rgba{32, 32, 38, 255};
        tokens.on_surface_muted = rgba{120, 120, 128, 255};
        tokens.outline = rgba{180, 182, 190, 255};
        tokens.accent = accent;
        tokens.on_accent = on_accent;
        tokens.danger = rgba{200, 60, 60, 255};
        tokens.on_danger = rgba{255, 255, 255, 255};
        tokens.focus_ring = accent;
        return tokens;
    }

    [[nodiscard]] bool color_eq(rgba lhs, rgba rhs) noexcept {
        return lhs.r == rhs.r
            && lhs.g == rhs.g
            && lhs.b == rhs.b
            && lhs.a == rhs.a;
    }

    [[nodiscard]] std::uint32_t hash_color(std::uint32_t hash, rgba color) noexcept {
        hash = vivid::evidence::hash_mix(hash, color.r);
        hash = vivid::evidence::hash_mix(hash, color.g);
        hash = vivid::evidence::hash_mix(hash, color.b);
        hash = vivid::evidence::hash_mix(hash, color.a);
        return hash;
    }

    [[nodiscard]] std::uint32_t hash_resolved_style(const ResolvedStyleView& style) noexcept {
        std::uint32_t hash = 2166136261u;
        if (style.colors) {
            hash = hash_color(hash, style.colors->bg);
            hash = hash_color(hash, style.colors->border);
            hash = hash_color(hash, style.colors->font);
            hash = hash_color(hash, style.colors->accent);
            hash = hash_color(hash, style.colors->on_accent);
        }
        if (style.metrics) {
            hash = vivid::evidence::hash_mix(hash, static_cast<std::uint32_t>(style.metrics->border_width));
            hash = vivid::evidence::hash_mix(hash, static_cast<std::uint32_t>(style.metrics->corner_radius));
            hash = vivid::evidence::hash_mix(hash, static_cast<std::uint32_t>(style.metrics->padding));
            hash = vivid::evidence::hash_mix(hash, static_cast<std::uint32_t>(style.metrics->font_role));
            hash = vivid::evidence::hash_mix(hash, static_cast<std::uint32_t>(style.metrics->font_weight));
        }
        return hash;
    }

    [[nodiscard]] bool metrics_equal(const ResolvedMetrics& lhs, const ResolvedMetrics& rhs) noexcept {
        return lhs.font == rhs.font
            && lhs.font_role == rhs.font_role
            && lhs.font_weight == rhs.font_weight
            && lhs.font_explicit == rhs.font_explicit
            && lhs.border_width == rhs.border_width
            && lhs.corner_radius == rhs.corner_radius
            && lhs.padding == rhs.padding
            && lhs.header_padding == rhs.header_padding
            && lhs.content_padding == rhs.content_padding
            && lhs.scrollbar_margin == rhs.scrollbar_margin
            && lhs.scrollbar_thumb_min == rhs.scrollbar_thumb_min;
    }

    [[nodiscard]] bool apply_button_role_patch() noexcept {
        auto& sheet = StyleSheet::instance();
        sheet.clear();

        StyleRolePatch roles{};
        roles.has_bg_color = true;
        roles.has_bg_hover = true;
        roles.has_bg_pressed = true;
        roles.has_border_color = true;
        roles.has_font_color = true;
        roles.has_accent_color = true;
        roles.has_on_accent = true;
        roles.bg_color = StyleRole::Accent;
        roles.bg_hover = StyleRole::AccentHover;
        roles.bg_pressed = StyleRole::AccentPressed;
        roles.border_color = StyleRole::Accent;
        roles.font_color = StyleRole::OnAccent;
        roles.accent_color = StyleRole::Accent;
        roles.on_accent = StyleRole::OnAccent;

        const bool added = sheet.add_role_rule(StyleSelector{WidgetKind::Button, 0}, roles);
        sheet.rebuild_if_needed();
        return added;
    }

    [[nodiscard]] bool setup_tokens_and_style_sheet(const ThemeTokens& tokens) noexcept {
        Theme::instance().set_tokens_unsafe(tokens);
        vivid::evidence::prepare_style_sheet();
        return apply_button_role_patch();
    }

    void apply_tokens_only(const ThemeTokens& tokens) noexcept {
        Theme::instance().set_tokens_unsafe(tokens);
        StyleSheet::instance().rebuild_if_needed();
    }
}

int main() {
    auto run_log = kRunLog;
    run_log.begin();

    if (!setup_tokens_and_style_sheet(make_tokens(rgba{64, 120, 220, 255}, rgba{255, 255, 255, 255}))) {
        std::puts("[ERR] failed to add button role patch");
        return 1;
    }

    auto& sheet = StyleSheet::instance();
    const auto token_version_before = Theme::instance().get_tokens().version;
    const auto stylesheet_version = sheet.stylesheet_version();

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    WidgetHandle button{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        const auto root = builder.create_container();
        button = builder.create_button_static("Accent");

        builder.link(root, button);
        builder.set_rect(root, {0, 0, 160, 72});
        builder.set_rect(button, kButtonBounds);
        builder.set_root(root);
    });

    const StyleState normal_state = make_style_state(true, false, false, false);
    const auto normal_before = sheet.lookup(WidgetKind::Button, normal_state);
    if (!vivid::evidence::expect(normal_before.colors != nullptr, "resolved style has colors")) return 1;
    if (!vivid::evidence::expect(normal_before.metrics != nullptr, "resolved style has metrics")) return 1;
    if (!vivid::evidence::expect(color_eq(normal_before.colors->bg, Theme::instance().get_tokens().accent),
                                 "button bg resolves semantic accent token")) {
        return 1;
    }
    const rgba bg_before = normal_before.colors->bg;
    const ResolvedMetrics metrics_before = *normal_before.metrics;

    run_log.case_begin("semantic_token");
    std::printf(" token=accent role=button.bg_color token_version=%u stylesheet_version=%u bg_r=%u bg_g=%u bg_b=%u\n",
                token_version_before,
                stylesheet_version,
                bg_before.r,
                bg_before.g,
                bg_before.b);

    constexpr std::uint8_t hovered = static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    constexpr std::uint8_t pressed = static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    constexpr std::uint8_t focused = static_cast<std::uint8_t>(StyleStateFlag::Focused);
    constexpr std::uint8_t disabled = static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    const std::uint8_t button_mask = style_kind_state_mask(WidgetKind::Button);
    if (!vivid::evidence::expect((button_mask & hovered) != 0, "button mask includes hovered")) return 1;
    if (!vivid::evidence::expect((button_mask & pressed) != 0, "button mask includes pressed")) return 1;
    if (!vivid::evidence::expect((button_mask & disabled) != 0, "button mask includes disabled")) return 1;
    if (!vivid::evidence::expect((button_mask & focused) == 0, "button mask keeps focus separate")) return 1;

    run_log.case_begin("state_mask_law");
    std::printf(" widget=button mask=%u hovered=1 pressed=1 disabled=1 focused_in_style_mask=0 state_count=%u\n",
                button_mask,
                style_kind_state_count(WidgetKind::Button));

    const std::uint32_t style_key_before = hash_resolved_style(normal_before);
    run_log.case_begin("resolved_style_key");
    std::printf(" widget=button state=normal style_key=%u token_version=%u stylesheet_version=%u\n",
                style_key_before,
                token_version_before,
                stylesheet_version);

    const auto initial = vivid::evidence::render_scene(scene, canvas, Rect{0, 0, 160, 72});
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial style render has no failed commands")) return 1;
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial style render records commands")) return 1;

    run_log.case_begin("render_artifact_before");
    std::printf(" dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u\n",
                initial.dirty_count,
                initial.cmd_count,
                initial.cmd_hash,
                initial.pixel_hash);

    apply_tokens_only(make_tokens(rgba{220, 80, 40, 255}, rgba{255, 255, 255, 255}));
    const auto token_version_after = Theme::instance().get_tokens().version;
    const auto normal_after = sheet.lookup(WidgetKind::Button, normal_state);
    if (!vivid::evidence::expect(normal_after.colors != nullptr, "resolved style after has colors")) return 1;
    if (!vivid::evidence::expect(normal_after.metrics != nullptr, "resolved style after has metrics")) return 1;
    if (!vivid::evidence::expect(token_version_after > token_version_before, "token version increments")) return 1;
    if (!vivid::evidence::expect(sheet.stylesheet_version() == stylesheet_version,
                                 "token-only change keeps stylesheet version")) {
        return 1;
    }
    if (!vivid::evidence::expect(!color_eq(bg_before, normal_after.colors->bg),
                                 "accent token changes resolved color")) {
        return 1;
    }
    if (!vivid::evidence::expect(metrics_equal(metrics_before, *normal_after.metrics),
                                 "color token keeps metrics stable")) {
        return 1;
    }

    const std::uint32_t style_key_after = hash_resolved_style(normal_after);
    if (!vivid::evidence::expect(style_key_after != style_key_before, "style key changes after accent token")) {
        return 1;
    }
    const StyleImpactDecision impact = decide_style_token_impact(StyleTokenDomain::Color);
    if (!vivid::evidence::expect(impact.has(StyleInvalidationImpact::PaintOnly), "color token triggers paint")) {
        return 1;
    }
    if (!vivid::evidence::expect(!impact.has(StyleInvalidationImpact::Layout), "color token does not trigger layout")) {
        return 1;
    }
    if (!vivid::evidence::expect(!impact.has(StyleInvalidationImpact::TextMetrics),
                                 "color token does not trigger text metrics")) {
        return 1;
    }
    const StyleImpactDecision spacing_impact = decide_style_token_impact(StyleTokenDomain::Spacing);
    if (!vivid::evidence::expect(spacing_impact.has(StyleInvalidationImpact::PaintOnly),
                                 "spacing token preserves paint impact")) {
        return 1;
    }
    if (!vivid::evidence::expect(spacing_impact.has(StyleInvalidationImpact::Layout),
                                 "spacing token triggers layout")) {
        return 1;
    }
    const StyleImpactDecision font_impact = decide_style_token_impact(StyleTokenDomain::Font);
    if (!vivid::evidence::expect(font_impact.has(StyleInvalidationImpact::TextMetrics),
                                 "font token triggers text metrics")) {
        return 1;
    }
    if (!vivid::evidence::expect(font_impact.has(StyleInvalidationImpact::Layout),
                                 "font token triggers layout")) {
        return 1;
    }

    run_log.case_begin("token_change_impact");
    std::printf(" token=accent domain=%s old_version=%u new_version=%u stylesheet_version=%u impact=%s impact_mask=%u metrics_same=1 style_key_old=%u style_key_new=%u\n",
                style_token_domain_name(impact.domain),
                token_version_before,
                token_version_after,
                stylesheet_version,
                style_impact_primary_name(impact),
                impact.impact_mask,
                style_key_before,
                style_key_after);

    const auto updated = vivid::evidence::render_scene(scene, canvas, kButtonBounds);
    if (!vivid::evidence::expect(updated.failed_cmds == 0, "updated style render has no failed commands")) return 1;
    if (!vivid::evidence::expect(updated.cmd_count > 0, "updated style render records commands")) return 1;
    if (!vivid::evidence::expect(updated.pixel_hash != initial.pixel_hash,
                                 "token change affects render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(updated.dirty_count == 1, "token repaint uses single button dirty rect")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::dirty_stays_inside(canvas, kButtonBounds),
                                 "style dirty evidence remains inside button bounds")) {
        return 1;
    }

    run_log.case_begin("render_artifact_after");
    std::printf(" dirty_count=%zu dirty_hash=%u cmd_count=%zu cmd_bytes=%zu failed=%zu cmd_hash=%u pixel_hash=%u\n",
                updated.dirty_count,
                updated.dirty_hash,
                updated.cmd_count,
                updated.cmd_bytes,
                updated.failed_cmds,
                updated.cmd_hash,
                updated.pixel_hash);

    run_log.end(true);
    std::puts("[style_token_law_demo] ok");
    return 0;
}
