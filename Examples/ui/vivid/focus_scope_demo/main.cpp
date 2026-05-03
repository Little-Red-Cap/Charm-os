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

    [[nodiscard]] bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs.kind == rhs.kind
            && lhs.index == rhs.index
            && lhs.generation == rhs.generation;
    }

    [[nodiscard]] bool contains_in_demo(WidgetHandle node, WidgetHandle ancestor, void* ctx) noexcept {
        const auto* tree = static_cast<const ScopeTree*>(ctx);
        if (!tree) return false;
        if (same_handle(node, ancestor)) return true;
        if (same_handle(ancestor, tree->handles.scope)) {
            return same_handle(node, tree->handles.inside_a)
                || same_handle(node, tree->handles.inside_b);
        }
        if (same_handle(ancestor, tree->handles.root)) {
            return same_handle(node, tree->handles.scope)
                || same_handle(node, tree->handles.inside_a)
                || same_handle(node, tree->handles.inside_b)
                || same_handle(node, tree->handles.outside);
        }
        return false;
    }

    void click(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
    }

    void apply_focus_decision(::ui::scene::SceneAccess& access,
                              const FocusScopeDecision& decision,
                              WidgetHandle& current) noexcept {
        if (decision.allowed()) {
            if (current && !same_handle(current, decision.requested)) {
                access.set_focused(current, false);
            }
            access.set_focused(decision.requested, true);
            current = decision.requested;
            return;
        }
        const WidgetHandle fallback = decision.fallback ? decision.fallback : current;
        if (current && fallback && !same_handle(current, fallback)) {
            access.set_focused(current, false);
        }
        if (fallback) {
            access.set_focused(fallback, true);
            current = fallback;
        }
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
    access.set_focused(handles.inside_a, true);
    WidgetHandle scope_truth = handles.inside_a;
    const auto initial = vivid::evidence::render_scene(scene, canvas, kInsideABounds);
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial scope focus render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial scope focus render records commands")) return 1;

    run_log.case_begin("initial_focus");
    std::printf(" target=inside_a focus=1 dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u\n",
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

    apply_focus_decision(access, inside_decision, scope_truth);
    if (!vivid::evidence::expect(same_handle(scope_truth, handles.inside_b),
                                 "scope truth moves to inside_b after allowed transfer")) {
        return 1;
    }

    const auto inside_artifact = vivid::evidence::render_scene(scene, canvas, kInsideBBounds);
    if (!vivid::evidence::expect(inside_artifact.failed_cmds == 0, "inside transfer render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(inside_artifact.pixel_hash != initial.pixel_hash,
                                 "inside transfer moves focus artifact")) {
        return 1;
    }

    run_log.case_begin("inside_transfer_artifact");
    std::printf(" target=inside_b scope_truth=inside_b dirty_count=%zu dirty_hash=%u pixel_hash_old=%u pixel_hash_new=%u artifact_changed=1 focus_ring=1\n",
                inside_artifact.dirty_count,
                inside_artifact.dirty_hash,
                initial.pixel_hash,
                inside_artifact.pixel_hash);

    const auto outside_baseline = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
    if (!vivid::evidence::expect(outside_baseline.failed_cmds == 0, "outside baseline render has no failed commands")) {
        return 1;
    }

    FocusScopeSpec outside_spec = scope_spec;
    outside_spec.current = scope_truth;
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

    apply_focus_decision(access, outside_decision, scope_truth);
    if (!vivid::evidence::expect(same_handle(scope_truth, handles.inside_b),
                                 "focus remains trapped inside scope")) {
        return 1;
    }

    run_log.case_begin("scope_trap_truth");
    std::printf(" requested=outside focused=inside_b leaked=0 fallback=inside_b committed=1\n");

    const auto trapped_inside = vivid::evidence::render_scene(scene, canvas, kInsideBBounds);
    if (!vivid::evidence::expect(trapped_inside.failed_cmds == 0, "trapped inside render has no failed commands")) {
        return 1;
    }
    const auto outside_artifact = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
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
    if (!vivid::evidence::expect(vivid::evidence::dirty_stays_inside(canvas, kOutsideBounds),
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
