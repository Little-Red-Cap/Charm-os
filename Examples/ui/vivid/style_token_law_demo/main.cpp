#include <cstdint>
#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.style_evidence;
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

    const StyleStateEvidence state_evidence = make_style_state_evidence(WidgetKind::Button);
    if (!vivid::evidence::expect(state_evidence.includes_hovered, "button mask includes hovered")) return 1;
    if (!vivid::evidence::expect(state_evidence.includes_pressed, "button mask includes pressed")) return 1;
    if (!vivid::evidence::expect(state_evidence.includes_disabled, "button mask includes disabled")) return 1;
    if (!vivid::evidence::expect(!state_evidence.includes_focused, "button mask keeps focus separate")) return 1;
    if (!vivid::evidence::expect(style_state_evidence_matches_interactive_law(state_evidence),
                                 "button state evidence matches interactive law")) {
        return 1;
    }

    run_log.case_begin("state_mask_law");
    vivid::evidence::print_style_state_mask("button", "interactive_without_focus", state_evidence);
    std::printf("\n");

    const ResolvedStyleEvidence style_evidence_before = make_resolved_style_evidence(normal_before);
    run_log.case_begin("resolved_style_key");
    vivid::evidence::print_resolved_style_evidence("button", "normal", style_evidence_before);
    std::printf(" token_version=%u stylesheet_version=%u\n",
                token_version_before,
                stylesheet_version);

    const auto initial = vivid::evidence::render_scene(scene, canvas, Rect{0, 0, 160, 72});
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial style render has no failed commands")) return 1;
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial style render records commands")) return 1;

    run_log.case_begin("render_artifact_before");
    vivid::evidence::print_render_evidence("before", initial);
    std::printf("\n");

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

    const ResolvedStyleEvidence style_evidence_after = make_resolved_style_evidence(normal_after);
    if (!vivid::evidence::expect(style_evidence_after.style_key != style_evidence_before.style_key,
                                 "style key changes after accent token")) {
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
    if (!vivid::evidence::expect(!style_color_evidence_equal(style_evidence_before, style_evidence_after),
                                 "color evidence changes")) {
        return 1;
    }
    if (!vivid::evidence::expect(style_metrics_evidence_equal(style_evidence_before, style_evidence_after),
                                 "metrics evidence remains stable")) {
        return 1;
    }
    if (!vivid::evidence::expect(style_evidence_matches_impact(style_evidence_before,
                                                               style_evidence_after,
                                                               impact),
                                 "style evidence matches impact decision")) {
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
    std::printf(" token=accent domain=%s old_version=%u new_version=%u stylesheet_version=%u impact=%s impact_mask=%u color_hash_old=%u color_hash_new=%u metrics_hash_old=%u metrics_hash_new=%u color_changed=1 metrics_same=1 style_key_old=%u style_key_new=%u\n",
                style_token_domain_name(impact.domain),
                token_version_before,
                token_version_after,
                stylesheet_version,
                style_impact_primary_name(impact),
                impact.impact_mask,
                style_evidence_before.color_hash,
                style_evidence_after.color_hash,
                style_evidence_before.metrics_hash,
                style_evidence_after.metrics_hash,
                style_evidence_before.style_key,
                style_evidence_after.style_key);

    const auto updated_capture =
        vivid::evidence::render_component_artifact_delta(scene, canvas, kButtonBounds, initial);
    const auto& updated = updated_capture.evidence;
    const auto& artifact_delta = updated_capture.delta;
    if (!vivid::evidence::expect(updated.failed_cmds == 0, "updated style render has no failed commands")) return 1;
    if (!vivid::evidence::expect(updated.cmd_count > 0, "updated style render records commands")) return 1;
    if (!vivid::evidence::expect(updated.pixel_hash != initial.pixel_hash,
                                 "token change affects render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(updated.dirty_count == 1, "token repaint uses single button dirty rect")) return 1;
    if (!vivid::evidence::expect(artifact_delta.dirty_within_component,
                                 "style dirty evidence remains inside button bounds")) {
        return 1;
    }

    run_log.case_begin("render_artifact_after");
    vivid::evidence::print_render_artifact_verdict(artifact_delta, "after", updated);
    std::printf("\n");

    const vivid::evidence::CausalChainEvidence chain{
        .name = "button.accent_token",
        .request_ok = color_eq(bg_before, rgba{64, 120, 220, 255})
            && color_eq(normal_after.colors->bg, rgba{220, 80, 40, 255})
            && token_version_after > token_version_before
            && sheet.stylesheet_version() == stylesheet_version,
        .state_delta_ok = !style_color_evidence_equal(style_evidence_before, style_evidence_after)
            && style_metrics_evidence_equal(style_evidence_before, style_evidence_after)
            && style_evidence_after.style_key != style_evidence_before.style_key,
        .invalidation_ok = impact.has(StyleInvalidationImpact::PaintOnly)
            && !impact.has(StyleInvalidationImpact::Layout)
            && !impact.has(StyleInvalidationImpact::TextMetrics)
            && style_evidence_matches_impact(style_evidence_before, style_evidence_after, impact),
        .artifact_ok = artifact_delta.changed
            && artifact_delta.single_dirty_rect
            && artifact_delta.dirty_within_component
            && updated.pixel_hash != initial.pixel_hash,
        .rejected_no_mutation = false,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok(),
                                 "style token causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[style_token_law_demo] ok");
    return 0;
}
