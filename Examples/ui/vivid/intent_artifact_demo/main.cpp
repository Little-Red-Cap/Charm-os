#include <cstddef>
#include <cstdint>
#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 280, 120};
    constexpr Rect kComponentBounds{12, 12, 244, 72};
    constexpr Rect kTitleBounds{24, 20, 132, 20};
    constexpr Rect kToggleBounds{176, 28, 64, 30};
    constexpr vivid::evidence::RunLog kRunLog{"ia", "intent_artifact_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle row{};
        WidgetHandle title{};
        WidgetHandle toggle{};
    };

    [[nodiscard]] bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs == rhs;
    }

    void build_scene(::ui::scene::Scene& scene, Handles& handles) {
        scene.build([&](::ui::scene::SceneBuilder& builder) {
            handles.root = builder.create_container();
            handles.row = builder.create_container();
            handles.title = builder.create_label_static("Wi-Fi");
            handles.toggle = builder.create_checkbox("");

            builder.link(handles.root, handles.row);
            builder.link(handles.row, handles.title);
            builder.link(handles.row, handles.toggle);

            builder.set_rect(handles.root, kSceneBounds);
            builder.set_rect(handles.row, kComponentBounds);
            builder.set_rect(handles.title, kTitleBounds);
            builder.set_rect(handles.toggle, kToggleBounds);
            builder.set_semantic(handles.row, SemanticRole::Container, "settings.wifi", "Wi-Fi setting");
            builder.set_semantic_default(handles.toggle, "settings.wifi.toggle", "Wi-Fi toggle");
            builder.set_input_root(handles.root);
            builder.set_focus_scope(handles.row, handles.toggle, true);
            builder.set_root(handles.root);
        });
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
    build_scene(scene, handles);

    auto access = scene.access();
    access.set_focusable(handles.toggle, true);
    const auto focus_setup = scene.request_semantic_focus(handles.row, "settings.wifi.toggle");
    if (!vivid::evidence::expect(focus_setup.status == SemanticFocusRequestStatus::Committed
                                 && same_handle(access.input_focused(), handles.toggle),
                                 "setup focuses toggle through semantic focus request")) {
        return 1;
    }

    const auto baseline = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(baseline.failed_cmds == 0, "baseline render has no failed commands")) return 1;
    if (!vivid::evidence::expect(!access.checked(handles.toggle), "baseline toggle is unchecked")) return 1;

    run_log.case_begin("baseline_artifact");
    std::printf(" checked=%d focus=toggle", access.checked(handles.toggle) ? 1 : 0);
    vivid::evidence::print_render_evidence("base", baseline);
    std::printf("\n");

    const bool before_checked = access.checked(handles.toggle);
    const auto positive_request =
        scene.request_semantic_action(handles.row, "settings.wifi.toggle", SemanticAction::Activate);
    const bool after_checked = access.checked(handles.toggle);

    run_log.case_begin("request_ledger");
    vivid::evidence::print_action_request_ledger(positive_request);
    std::printf("\n");
    if (!vivid::evidence::expect(positive_request.status == SemanticActionRequestStatus::Executed
                                 && positive_request.reject_reason == SemanticActionRequestRejectReason::None
                                 && positive_request.emitted_click
                                 && positive_request.focus_ready,
                                 "semantic action request commits through normal action path")) {
        return 1;
    }

    run_log.case_begin("state_delta");
    vivid::evidence::print_state_delta({
        .id = "settings.wifi.toggle",
        .key = "checked",
        .source = "semantic_action_request",
        .old_value = before_checked ? 1 : 0,
        .new_value = after_checked ? 1 : 0,
    });
    std::printf("\n");
    if (!vivid::evidence::expect(!before_checked && after_checked,
                                 "semantic activate toggles checked truth")) {
        return 1;
    }

    run_log.case_begin("invalidation");
    vivid::evidence::print_invalidation({
        .kind = "paint_only",
        .dirty_scope = "component",
        .component_bounds = kComponentBounds,
        .layout_changed = false,
    });
    std::printf("\n");

    const auto changed = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(changed.failed_cmds == 0, "changed render has no failed commands")) return 1;
    const bool changed_dirty_inside = vivid::evidence::dirty_stays_inside(canvas, kComponentBounds);
    const auto changed_delta =
        vivid::evidence::make_render_artifact_delta(baseline, changed, changed_dirty_inside);
    if (!vivid::evidence::expect(changed_delta.changed,
                                 "state delta changes render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(changed_delta.single_dirty_rect,
                                 "changed render keeps one dirty rect")) {
        return 1;
    }
    if (!vivid::evidence::expect(changed_delta.dirty_within_component,
                                 "changed dirty evidence stays inside component")) {
        return 1;
    }

    run_log.case_begin("render_artifact");
    vivid::evidence::print_render_artifact_verdict(changed_delta, "after", changed);
    std::printf("\n");

    access.set_enabled(handles.toggle, false);
    const auto disabled_baseline = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(disabled_baseline.failed_cmds == 0,
                                 "disabled baseline render has no failed commands")) {
        return 1;
    }
    const bool disabled_before_checked = access.checked(handles.toggle);
    const auto rejected =
        scene.request_semantic_action(handles.row, "settings.wifi.toggle", SemanticAction::Activate);
    const bool disabled_after_checked = access.checked(handles.toggle);

    run_log.case_begin("rejected_request_ledger");
    vivid::evidence::print_action_request_ledger(rejected);
    std::printf("\n");
    if (!vivid::evidence::expect(rejected.status == SemanticActionRequestStatus::Rejected
                                 && rejected.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
                                 && rejected.admission.status == SemanticActionAdmissionStatus::Disabled
                                 && !rejected.emitted_click,
                                 "disabled semantic target is rejected before execution")) {
        return 1;
    }

    run_log.case_begin("rejected_no_state_delta");
    vivid::evidence::print_state_delta({
        .id = "settings.wifi.toggle",
        .key = "checked",
        .source = "semantic_action_request",
        .old_value = disabled_before_checked ? 1 : 0,
        .new_value = disabled_after_checked ? 1 : 0,
        .reason = "admission_rejected",
    });
    std::printf("\n");
    if (!vivid::evidence::expect(disabled_before_checked == disabled_after_checked,
                                 "rejected request preserves checked truth")) {
        return 1;
    }

    const auto rejected_artifact = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(rejected_artifact.failed_cmds == 0,
                                 "rejected render has no failed commands")) {
        return 1;
    }
    const bool rejected_dirty_inside = vivid::evidence::dirty_stays_inside(canvas, kComponentBounds);
    const auto rejected_delta =
        vivid::evidence::make_render_artifact_delta(disabled_baseline, rejected_artifact, rejected_dirty_inside);
    if (!vivid::evidence::expect(!rejected_delta.changed,
                                 "rejected request preserves render artifact")) {
        return 1;
    }

    run_log.case_begin("rejected_artifact");
    vivid::evidence::print_render_artifact_comparison(rejected_delta,
                                                      disabled_baseline,
                                                      rejected_artifact);
    std::printf("\n");

    const vivid::evidence::CausalChainEvidence chain{
        .name = "settings.wifi.toggle.activate",
        .request_ok = positive_request.status == SemanticActionRequestStatus::Executed
            && positive_request.reject_reason == SemanticActionRequestRejectReason::None
            && positive_request.emitted_click,
        .state_delta_ok = !before_checked && after_checked,
        .invalidation_ok = true,
        .artifact_ok = changed_delta.changed
            && changed_delta.dirty_within_component
            && changed_delta.single_dirty_rect,
        .rejected_no_mutation = rejected.status == SemanticActionRequestStatus::Rejected
            && rejected.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
            && !rejected.emitted_click
            && !rejected_delta.changed,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok() && chain.rejected_no_mutation,
                                 "intent-to-artifact causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[intent_artifact_demo] ok");
    return 0;
}
