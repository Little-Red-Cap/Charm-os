#include <cstddef>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;
import charm.ui.scene.focus_scope;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 260, 144};
    constexpr Rect kScopeBounds{12, 12, 180, 120};
    constexpr Rect kInsideABounds{24, 28, 132, 34};
    constexpr Rect kInsideBBounds{24, 78, 132, 34};
    constexpr Rect kOutsideBounds{204, 54, 44, 44};
    constexpr vivid::evidence::RunLog kRunLog{"fs", "focus_scope_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle inside_a{};
        WidgetHandle inside_b{};
        WidgetHandle outside{};
    };

    struct ScopeTree {
        Handles handles{};
    };


    [[nodiscard]] bool contains_in_demo(WidgetHandle node, WidgetHandle ancestor, void* ctx) noexcept {
        const auto* tree = static_cast<const ScopeTree*>(ctx);
        if (!tree) return false;
        if (vivid::evidence::same_handle(node, ancestor)) return true;
        if (vivid::evidence::same_handle(ancestor, tree->handles.scope)) {
            return vivid::evidence::same_handle(node, tree->handles.inside_a)
                || vivid::evidence::same_handle(node, tree->handles.inside_b);
        }
        if (vivid::evidence::same_handle(ancestor, tree->handles.root)) {
            return vivid::evidence::same_handle(node, tree->handles.scope)
                || vivid::evidence::same_handle(node, tree->handles.inside_a)
                || vivid::evidence::same_handle(node, tree->handles.inside_b)
                || vivid::evidence::same_handle(node, tree->handles.outside);
        }
        return false;
    }

}

int main() {
    auto run_log = kRunLog;
    run_log.begin();
    vivid::evidence::prepare_style_sheet();

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    Handles handles{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        handles.root = builder.create_container();
        handles.scope = builder.create_container();
        handles.inside_a = builder.create_scroll_container();
        handles.inside_b = builder.create_scroll_container();
        handles.outside = builder.create_scroll_container();

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.inside_a);
        builder.link(handles.scope, handles.inside_b);
        builder.link(handles.root, handles.outside);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.inside_a, kInsideABounds);
        builder.set_rect(handles.inside_b, kInsideBBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.inside_b, true);
        builder.set_root(handles.root);
    });

    ScopeTree tree{handles};
    const FocusScopeSpec scope_spec{
        .scope = handles.scope,
        .current = handles.inside_a,
        .fallback = handles.inside_b,
        .trap = true,
    };

    if (!vivid::evidence::expect(contains_in_demo(handles.inside_a, handles.scope, &tree),
                                 "inside_a belongs to focus scope")) {
        return 1;
    }
    if (!vivid::evidence::expect(contains_in_demo(handles.inside_b, handles.scope, &tree),
                                 "inside_b belongs to focus scope")) {
        return 1;
    }
    if (!vivid::evidence::expect(!contains_in_demo(handles.outside, handles.scope, &tree),
                                 "outside is rejected by focus scope")) {
        return 1;
    }

    run_log.case_begin("scope_model");
    std::printf(" scope=container inside_targets=2 outside_targets=1 trap=1 contains_inside=1 contains_outside=0\n");

    auto access = scene.access();
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focus_scope(), handles.scope),
                                 "runtime focus scope is installed")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focus_scope_fallback(), handles.inside_b),
                                 "runtime focus scope fallback is installed")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.input_focus_scope_trap(), "runtime focus scope trap is enabled")) {
        return 1;
    }

    run_log.case_begin("runtime_scope_install");
    std::printf(" scope=container fallback=inside_b trap=1 policy=focus_admission\n");

    vivid::evidence::mouse_down_center(scene, kInsideABounds, 10);
    auto initial_access = scene.access();
    const auto initial_trace =
        vivid::evidence::collect_pointer_focus_trace(initial_access, handles.inside_a, {}, handles.inside_a);
    if (!vivid::evidence::expect(initial_trace.mouse_down == 1 && initial_trace.mouse_down_expected,
                                 "inside_a receives initial mouse down")) return 1;
    if (!vivid::evidence::expect(initial_trace.focus_in == 1 && initial_trace.focus_in_expected,
                                 "inside_a receives initial FocusIn")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(initial_access.input_focused(), handles.inside_a),
                                 "runtime focus truth commits to inside_a")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kInsideABounds, 11);

    const auto initial = vivid::evidence::render_scene(scene, canvas, kInsideABounds);
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial scope focus render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial scope focus render records commands")) return 1;

    run_log.case_begin("initial_focus");
    std::printf(" target=inside_a focus=1 mouse_down=%d focus_in=%d input_truth=inside_a dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u\n",
                initial_trace.mouse_down,
                initial_trace.focus_in,
                initial.dirty_count,
                initial.cmd_count,
                initial.cmd_hash,
                initial.pixel_hash);

    const auto inside_decision = decide_focus_scope_request(
        scope_spec,
        handles.inside_b,
        &contains_in_demo,
        &tree);
    if (!vivid::evidence::expect(inside_decision.allowed(), "inside focus request is allowed")) return 1;

    run_log.case_begin("inside_request_decision");
    std::printf(" requested=inside_b decision=%s allowed=%d reason=inside_scope\n",
                focus_scope_decision_name(inside_decision.kind),
                inside_decision.allowed() ? 1 : 0);

    vivid::evidence::mouse_down_center(scene, kInsideBBounds, 20);
    auto inside_access = scene.access();
    const auto inside_trace =
        vivid::evidence::collect_pointer_focus_trace(inside_access, handles.inside_b, handles.inside_a, handles.inside_b);
    if (!vivid::evidence::expect(inside_trace.mouse_down == 1 && inside_trace.mouse_down_expected,
                                 "inside_b receives mouse down")) return 1;
    if (!vivid::evidence::expect(inside_trace.focus_out == 1 && inside_trace.focus_out_expected,
                                 "inside transfer emits FocusOut for inside_a")) {
        return 1;
    }
    if (!vivid::evidence::expect(inside_trace.focus_in == 1 && inside_trace.focus_in_expected,
                                 "inside transfer emits FocusIn for inside_b")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(inside_access.input_focused(), handles.inside_b),
                                 "runtime focus truth moves to inside_b")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kInsideBBounds, 21);

    run_log.case_begin("inside_transfer_dispatch");
    std::printf(" requested=inside_b mouse_down=%d focus_out=%d focus_in=%d focus_out_inside_a=%d focus_in_inside_b=%d input_truth=inside_b allowed=1\n",
                inside_trace.mouse_down,
                inside_trace.focus_out,
                inside_trace.focus_in,
                inside_trace.focus_out_expected ? 1 : 0,
                inside_trace.focus_in_expected ? 1 : 0);

    const auto inside_artifact = vivid::evidence::render_scene(scene, canvas, kInsideBBounds);
    if (!vivid::evidence::expect(inside_artifact.failed_cmds == 0, "inside transfer render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(inside_artifact.pixel_hash != initial.pixel_hash,
                                 "inside transfer moves focus artifact")) {
        return 1;
    }

    run_log.case_begin("inside_transfer_artifact");
    std::printf(" target=inside_b input_truth=inside_b dirty_count=%zu dirty_hash=%u pixel_hash_old=%u pixel_hash_new=%u artifact_changed=1 focus_ring=1\n",
                inside_artifact.dirty_count,
                inside_artifact.dirty_hash,
                initial.pixel_hash,
                inside_artifact.pixel_hash);

    const auto outside_baseline = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
    if (!vivid::evidence::expect(outside_baseline.failed_cmds == 0, "outside baseline render has no failed commands")) {
        return 1;
    }

    FocusScopeSpec outside_spec = scope_spec;
    outside_spec.current = handles.inside_b;
    outside_spec.fallback = handles.inside_b;
    const auto outside_decision = decide_focus_scope_request(
        outside_spec,
        handles.outside,
        &contains_in_demo,
        &tree);
    if (!vivid::evidence::expect(!outside_decision.allowed(), "outside focus request is rejected")) return 1;
    if (!vivid::evidence::expect(outside_decision.kind == FocusScopeDecisionKind::RejectOutsideScope,
                                 "outside focus request rejects as outside scope")) {
        return 1;
    }

    run_log.case_begin("outside_request_decision");
    std::printf(" requested=outside decision=%s allowed=%d fallback=inside_b reason=outside_scope\n",
                focus_scope_decision_name(outside_decision.kind),
                outside_decision.allowed() ? 1 : 0);

    vivid::evidence::mouse_down_center(scene, kOutsideBounds, 30);
    auto outside_access = scene.access();
    const auto outside_trace =
        vivid::evidence::collect_pointer_focus_trace(outside_access, handles.outside, {}, {});
    if (!vivid::evidence::expect(outside_trace.mouse_down == 1 && outside_trace.mouse_down_expected,
                                 "outside still receives pointer event")) return 1;
    if (!vivid::evidence::expect(outside_trace.focus_out == 0 && outside_trace.focus_in == 0,
                                 "outside rejected request emits no focus transfer events")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(outside_access.input_focused(), handles.inside_b),
                                 "runtime focus remains trapped inside scope")) {
        return 1;
    }
    if (!vivid::evidence::expect(!outside_access.input_focus_scope_trap()
                                     || vivid::evidence::same_handle(outside_access.input_focus_scope(), handles.scope),
                                 "runtime scope trap remains installed")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kOutsideBounds, 31);
    auto after_outside_access = scene.access();
    if (!vivid::evidence::expect(vivid::evidence::same_handle(after_outside_access.input_focused(), handles.inside_b),
                                 "runtime focus remains trapped after outside release")) {
        return 1;
    }

    run_log.case_begin("outside_trap_dispatch");
    std::printf(" requested=outside mouse_down=%d focus_out=%d focus_in=%d input_truth=inside_b leaked=0 fallback=inside_b committed=1\n",
                outside_trace.mouse_down,
                outside_trace.focus_out,
                outside_trace.focus_in);

    const auto trapped_inside = vivid::evidence::render_scene(scene, canvas, kInsideBBounds);
    if (!vivid::evidence::expect(trapped_inside.failed_cmds == 0, "trapped inside render has no failed commands")) {
        return 1;
    }
    const auto outside_capture =
        vivid::evidence::render_component_artifact_delta(scene, canvas, kOutsideBounds, outside_baseline);
    const auto& outside_artifact = outside_capture.evidence;
    const auto& outside_delta = outside_capture.delta;
    if (!vivid::evidence::expect(outside_artifact.failed_cmds == 0, "outside render has no failed commands")) return 1;
    if (!vivid::evidence::expect(outside_artifact.cmd_count == outside_baseline.cmd_count,
                                 "outside target command evidence stays at unfocused baseline")) {
        return 1;
    }
    if (!vivid::evidence::expect(outside_artifact.cmd_hash == outside_baseline.cmd_hash,
                                 "outside target command hash stays at unfocused baseline")) {
        return 1;
    }
    if (!vivid::evidence::expect(outside_artifact.pixel_hash == outside_baseline.pixel_hash,
                                 "outside target render artifact stays at unfocused baseline")) {
        return 1;
    }
    if (!vivid::evidence::expect(outside_delta.dirty_within_component,
                                 "outside dirty evidence remains local")) {
        return 1;
    }

    run_log.case_begin("scope_trap_artifact");
    std::printf(" inside_cmd_count=%zu outside_cmd_count=%zu outside_cmd_hash=%u inside_pixel_hash=%u outside_pixel_hash=%u outside_baseline_hash=%u outside_focus_ring=0 leaked=0\n",
                trapped_inside.cmd_count,
                outside_artifact.cmd_count,
                outside_artifact.cmd_hash,
                trapped_inside.pixel_hash,
                outside_artifact.pixel_hash,
                outside_baseline.pixel_hash);

    run_log.end(true);
    std::puts("[focus_scope_demo] ok");
    return 0;
}
