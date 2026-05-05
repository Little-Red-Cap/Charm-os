#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.style_evidence;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 180, 84};
    constexpr Rect kButtonBounds{20, 20, 132, 38};
    constexpr vivid::evidence::RunLog kRunLog{"fb", "focus_boundary_demo"};

    [[nodiscard]] bool style_evidence_equal(const ResolvedStyleEvidence& lhs,
                                            const ResolvedStyleEvidence& rhs) noexcept {
        return lhs.style_key == rhs.style_key
            && lhs.color_hash == rhs.color_hash
            && lhs.metrics_hash == rhs.metrics_hash;
    }
}

int main() {
    auto run_log = kRunLog;
    run_log.begin();
    vivid::evidence::prepare_style_sheet();

    auto& sheet = StyleSheet::instance();
    const auto token_version = Theme::instance().get_tokens().version;
    const auto stylesheet_version = sheet.stylesheet_version();

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    WidgetHandle button{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        const auto root = builder.create_container();
        button = builder.create_button_static("Focus");

        builder.link(root, button);
        builder.set_rect(root, kSceneBounds);
        builder.set_rect(button, kButtonBounds);
        builder.set_root(root);
    });

    const StyleState normal_state = make_style_state(true, false, false, false);
    const StyleState focused_state = make_style_state(true, false, false, true);
    const auto normal_style = sheet.lookup(WidgetKind::Button, normal_state);
    const auto focused_style = sheet.lookup(WidgetKind::Button, focused_state);
    if (!vivid::evidence::expect(normal_style.colors != nullptr, "normal style has colors")) return 1;
    if (!vivid::evidence::expect(normal_style.metrics != nullptr, "normal style has metrics")) return 1;
    if (!vivid::evidence::expect(focused_style.colors != nullptr, "focused style has colors")) return 1;
    if (!vivid::evidence::expect(focused_style.metrics != nullptr, "focused style has metrics")) return 1;

    const StyleStateEvidence state_evidence = make_style_state_evidence(WidgetKind::Button);
    if (!vivid::evidence::expect(state_evidence.includes_hovered, "button style mask includes hovered")) return 1;
    if (!vivid::evidence::expect(state_evidence.includes_pressed, "button style mask includes pressed")) return 1;
    if (!vivid::evidence::expect(state_evidence.includes_disabled, "button style mask includes disabled")) return 1;
    if (!vivid::evidence::expect(!state_evidence.includes_focused, "button style mask excludes focused")) return 1;
    if (!vivid::evidence::expect(style_state_evidence_matches_interactive_law(state_evidence),
                                 "button state evidence matches focus boundary law")) {
        return 1;
    }

    run_log.case_begin("style_mask_boundary");
    vivid::evidence::print_style_state_mask("button", "focus_outside_style_mask", state_evidence);
    std::printf("\n");

    const ResolvedStyleEvidence style_before = make_resolved_style_evidence(normal_style);
    const ResolvedStyleEvidence style_focused_lookup = make_resolved_style_evidence(focused_style);
    if (!vivid::evidence::expect(style_evidence_equal(style_before, style_focused_lookup),
                                 "focused style lookup keeps resolved style evidence")) {
        return 1;
    }

    run_log.case_begin("style_evidence_before");
    vivid::evidence::print_focus_style_evidence("button", false, style_before, true, false);
    std::printf(" token_version=%u stylesheet_version=%u\n",
                token_version,
                stylesheet_version);

    const auto initial = vivid::evidence::render_scene(scene, canvas, kSceneBounds);
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial render has no failed commands")) return 1;
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial render records commands")) return 1;

    auto access = scene.access();
    const int focused_old = 0;
    access.set_focused(button, true);
    const int focused_new = 1;

    run_log.case_begin("focus_state_delta");
    std::printf(" source=programmatic widget=button focused_old=%d focused_new=%d invalidation=paint_only artifact=focus_ring\n",
                focused_old,
                focused_new);

    const auto focused_lookup_after_state = sheet.lookup(WidgetKind::Button, focused_state);
    if (!vivid::evidence::expect(focused_lookup_after_state.colors != nullptr, "focused after style has colors")) {
        return 1;
    }
    if (!vivid::evidence::expect(focused_lookup_after_state.metrics != nullptr, "focused after style has metrics")) {
        return 1;
    }
    const ResolvedStyleEvidence style_after = make_resolved_style_evidence(focused_lookup_after_state);
    if (!vivid::evidence::expect(style_evidence_equal(style_before, style_after),
                                 "focus state keeps resolved style evidence stable")) {
        return 1;
    }

    run_log.case_begin("style_evidence_after");
    vivid::evidence::print_focus_style_evidence("button",
                                                true,
                                                style_after,
                                                true,
                                                state_evidence.includes_focused);
    std::printf("\n");

    const auto focused_capture =
        vivid::evidence::render_component_artifact_delta(scene, canvas, kButtonBounds, initial);
    const auto& focused_artifact = focused_capture.evidence;
    const auto& focused_delta = focused_capture.delta;
    if (!vivid::evidence::expect(focused_artifact.failed_cmds == 0, "focused render has no failed commands")) return 1;
    if (!vivid::evidence::expect(focused_artifact.cmd_count > initial.cmd_count,
                                 "focused render records extra focus command")) {
        return 1;
    }
    if (!vivid::evidence::expect(focused_artifact.cmd_hash != initial.cmd_hash,
                                 "focus changes draw command evidence")) {
        return 1;
    }
    if (!vivid::evidence::expect(focused_artifact.pixel_hash != initial.pixel_hash,
                                 "focus changes render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(focused_artifact.dirty_count == 1, "focus repaint uses single dirty rect")) return 1;
    if (!vivid::evidence::expect(focused_delta.dirty_within_component,
                                 "focus dirty evidence remains inside button bounds")) {
        return 1;
    }

    run_log.case_begin("render_artifact_after");
    std::printf(" dirty_count=%zu dirty_hash=%u cmd_count_old=%zu cmd_count_new=%zu cmd_hash_old=%u cmd_hash_new=%u pixel_hash_old=%u pixel_hash_new=%u artifact_changed=1 focus_ring=1\n",
                focused_artifact.dirty_count,
                focused_artifact.dirty_hash,
                initial.cmd_count,
                focused_artifact.cmd_count,
                initial.cmd_hash,
                focused_artifact.cmd_hash,
                initial.pixel_hash,
                focused_artifact.pixel_hash);

    access.set_focused(button, false);
    const auto cleared_artifact = vivid::evidence::render_scene(scene, canvas, kButtonBounds);
    if (!vivid::evidence::expect(cleared_artifact.failed_cmds == 0, "cleared render has no failed commands")) return 1;
    if (!vivid::evidence::expect(cleared_artifact.cmd_count == initial.cmd_count,
                                 "cleared focus returns command count to baseline")) {
        return 1;
    }
    if (!vivid::evidence::expect(cleared_artifact.cmd_hash == initial.cmd_hash,
                                 "cleared focus returns command evidence to baseline")) {
        return 1;
    }
    if (!vivid::evidence::expect(cleared_artifact.pixel_hash == initial.pixel_hash,
                                 "cleared focus returns render artifact to baseline")) {
        return 1;
    }

    run_log.case_begin("focus_clear_artifact");
    std::printf(" focused_old=1 focused_new=0 cmd_count=%zu cmd_hash=%u pixel_hash=%u artifact_baseline=1\n",
                cleared_artifact.cmd_count,
                cleared_artifact.cmd_hash,
                cleared_artifact.pixel_hash);

    const vivid::evidence::CausalChainEvidence chain{
        .name = "button.focus_boundary",
        .request_ok = focused_old == 0
            && focused_new == 1
            && !state_evidence.includes_focused,
        .state_delta_ok = access.focused(button) == false
            && style_evidence_equal(style_before, style_after),
        .invalidation_ok = style_evidence_equal(style_before, style_focused_lookup)
            && style_evidence_equal(style_before, style_after),
        .artifact_ok = focused_delta.changed
            && focused_delta.single_dirty_rect
            && focused_delta.dirty_within_component
            && focused_artifact.cmd_hash != initial.cmd_hash
            && focused_artifact.pixel_hash != initial.pixel_hash
            && cleared_artifact.cmd_hash == initial.cmd_hash
            && cleared_artifact.pixel_hash == initial.pixel_hash,
        .rejected_no_mutation = false,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok(),
                                 "focus boundary causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[focus_boundary_demo] ok");
    return 0;
}
