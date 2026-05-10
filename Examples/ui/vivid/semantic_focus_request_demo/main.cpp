#include <cstddef>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_evidence;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"
#include "../support/vivid_semantic_focus_fixture.hpp"

namespace {
    constexpr vivid::evidence::RunLog kRunLog{"sfr", "semantic_focus_request_demo"};

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

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    vivid::evidence::SemanticFocusFixtureHandles handles{};
    vivid::evidence::build_semantic_focus_fixture(scene, handles);

    auto access = scene.access();
    vivid::evidence::configure_semantic_focus_fixture(access, handles);
    vivid::evidence::click_center(scene, vivid::evidence::kSemanticFocusPrimaryBounds, 10);
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), handles.primary),
                                 "setup establishes primary input focus")) {
        return 1;
    }

    auto& sheet = StyleSheet::instance();
    const StyleStateEvidence state_evidence = make_style_state_evidence(WidgetKind::ListItem);
    if (!vivid::evidence::expect(!state_evidence.includes_focused,
                                 "list item style state keeps focus outside style mask")) {
        return 1;
    }
    const StyleState normal_state = make_style_state(true, false, false, false);
    const StyleState focused_state = make_style_state(true, false, false, true);
    const auto normal_style = sheet.lookup(WidgetKind::ListItem, normal_state);
    const auto focused_lookup = sheet.lookup(WidgetKind::ListItem, focused_state);
    if (!vivid::evidence::expect(normal_style.colors != nullptr && normal_style.metrics != nullptr,
                                 "normal list item style resolves")) {
        return 1;
    }
    if (!vivid::evidence::expect(focused_lookup.colors != nullptr && focused_lookup.metrics != nullptr,
                                 "focused list item lookup resolves")) {
        return 1;
    }
    const ResolvedStyleEvidence style_before = make_resolved_style_evidence(normal_style);
    const ResolvedStyleEvidence style_focus_lookup = make_resolved_style_evidence(focused_lookup);
    if (!vivid::evidence::expect(style_evidence_equal(style_before, style_focus_lookup),
                                 "focused lookup keeps list item style evidence stable")) {
        return 1;
    }

    const auto primary_artifact =
        vivid::evidence::render_scene(scene, canvas, vivid::evidence::kSemanticFocusPrimaryBounds);
    if (!vivid::evidence::expect(primary_artifact.failed_cmds == 0,
                                 "primary focus baseline render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(primary_artifact.cmd_count > 0,
                                 "primary focus baseline records commands")) {
        return 1;
    }

    run_log.case_begin("pre_request_artifact");
    std::printf(" semantic_current=action.primary focus_ring=primary");
    vivid::evidence::print_render_evidence("before", primary_artifact);
    std::printf("\n");

    const auto request = scene.request_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("commit_transfer");
    vivid::evidence::print_focus_request_ledger(request);
    std::printf("\n");
    if (!vivid::evidence::expect(request.status == SemanticFocusRequestStatus::Committed,
                                 "semantic focus request commits transfer")) {
        return 1;
    }
    if (!vivid::evidence::expect(request.committed && request.focus_changed,
                                 "semantic focus request changes input truth")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(request.before_focus, handles.primary)
                                 && vivid::evidence::same_handle(request.after_focus, handles.secondary),
                                 "request records before and after focus handles")) {
        return 1;
    }

    run_log.case_begin("focus_event_trace");
    const auto trace = vivid::evidence::collect_focus_move(access, handles.primary, handles.secondary);
    std::printf(" focus_out=%d focus_in=%d out_primary=%d in_secondary=%d event_count=%zu\n",
                trace.focus_out,
                trace.focus_in,
                trace.focus_out_expected ? 1 : 0,
                trace.focus_in_expected ? 1 : 0,
                access.input_event_count());
    if (!vivid::evidence::expect(trace.focus_out == 1 && trace.focus_in == 1,
                                 "request emits one FocusOut and one FocusIn")) {
        return 1;
    }
    if (!vivid::evidence::expect(trace.focus_out_expected && trace.focus_in_expected,
                                 "request focus events target source and destination")) {
        return 1;
    }

    run_log.case_begin("semantic_truth_after_request");
    const auto semantic = scene.semantic_focus_snapshot();
    std::printf(" input_truth=secondary semantic_found=%d semantic_current=%s focus_ring=%d\n",
                semantic.found ? 1 : 0,
                semantic.id,
                access.focused(handles.secondary) ? 1 : 0);
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), handles.secondary),
                                 "input focus truth is secondary after request")) {
        return 1;
    }
    if (!vivid::evidence::expect(semantic.found && semantic.id && semantic.id[0] == 'r',
                                 "semantic focus snapshot resolves row secondary")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.focused(handles.secondary),
                                 "visual focused flag follows semantic request")) {
        return 1;
    }

    const auto style_after_lookup = sheet.lookup(WidgetKind::ListItem, focused_state);
    if (!vivid::evidence::expect(style_after_lookup.colors != nullptr && style_after_lookup.metrics != nullptr,
                                 "style after semantic focus request resolves")) {
        return 1;
    }
    const ResolvedStyleEvidence style_after = make_resolved_style_evidence(style_after_lookup);
    run_log.case_begin("style_boundary_after_request");
    vivid::evidence::print_focus_style_evidence("list_item",
                                                true,
                                                style_after,
                                                style_evidence_equal(style_before, style_after),
                                                state_evidence.includes_focused);
    std::printf("\n");
    if (!vivid::evidence::expect(style_evidence_equal(style_before, style_after),
                                 "semantic focus request keeps style evidence stable")) {
        return 1;
    }

    const auto secondary_capture = vivid::evidence::render_component_artifact_delta(
        scene,
        canvas,
        vivid::evidence::kSemanticFocusSecondaryBounds,
        primary_artifact);
    const auto& secondary_artifact = secondary_capture.evidence;
    const auto& secondary_delta = secondary_capture.delta;
    if (!vivid::evidence::expect(secondary_artifact.failed_cmds == 0,
                                 "semantic focus artifact render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(secondary_delta.changed,
                                 "semantic focus request changes render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(secondary_delta.single_dirty_rect,
                                 "semantic focus artifact uses a single dirty rect")) {
        return 1;
    }
    if (!vivid::evidence::expect(secondary_delta.dirty_within_component,
                                 "semantic focus artifact stays inside secondary bounds")) {
        return 1;
    }

    run_log.case_begin("focus_artifact_after_request");
    std::printf(" semantic_current=row.secondary focus_ring=secondary");
    vivid::evidence::print_render_artifact_verdict(secondary_delta,
                                                   "after",
                                                   secondary_artifact);
    std::printf("\n");

    const bool transfer_artifact_ok = secondary_delta.changed
        && secondary_delta.single_dirty_rect
        && secondary_delta.dirty_within_component;

    const auto already = scene.request_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("already_focused_noop");
    vivid::evidence::print_focus_request_ledger(already);
    std::printf("\n");
    if (!vivid::evidence::expect(already.status == SemanticFocusRequestStatus::AlreadyFocused,
                                 "already-focused request is explicit no-op")) {
        return 1;
    }
    if (!vivid::evidence::expect(!already.committed && !already.focus_changed && access.input_event_count() == 0,
                                 "already-focused request emits no events")) {
        return 1;
    }

    const auto outside = scene.request_semantic_focus(handles.root, "action.outside");
    run_log.case_begin("reject_outside_scope");
    vivid::evidence::print_focus_request_ledger(outside);
    std::printf("\n");
    if (!vivid::evidence::expect(outside.status == SemanticFocusRequestStatus::Rejected
                                 && outside.admission.status == SemanticFocusAdmissionStatus::OutsideActiveScope,
                                 "outside active scope request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), handles.secondary),
                                 "outside rejection preserves current focus")) {
        return 1;
    }
    const auto outside_capture = vivid::evidence::render_component_artifact_delta(
        scene,
        canvas,
        vivid::evidence::kSemanticFocusSecondaryBounds,
        secondary_artifact);
    const auto& outside_artifact = outside_capture.evidence;
    const auto& outside_delta = outside_capture.delta;
    if (!vivid::evidence::expect(outside_artifact.failed_cmds == 0,
                                 "outside rejection artifact render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(!outside_delta.changed,
                                 "outside rejection does not mutate focus artifact")) {
        return 1;
    }

    run_log.case_begin("rejected_no_artifact_mutation");
    std::printf(" request=outside_scope semantic_current=row.secondary focus_preserved=1");
    vivid::evidence::print_render_artifact_comparison(outside_delta,
                                                      secondary_artifact,
                                                      outside_artifact);
    std::printf("\n");

    const auto disabled = scene.request_semantic_focus(handles.scope, "action.disabled");
    const auto not_focusable = scene.request_semantic_focus(handles.scope, "panel.info");
    run_log.case_begin("reject_disabled_or_not_focusable");
    std::printf(" disabled=%s not_focusable=%s focus_preserved=%d\n",
                semantic_focus_admission_status_name(disabled.admission.status),
                semantic_focus_admission_status_name(not_focusable.admission.status),
                vivid::evidence::same_handle(access.input_focused(), handles.secondary) ? 1 : 0);
    if (!vivid::evidence::expect(disabled.status == SemanticFocusRequestStatus::Rejected
                                 && disabled.admission.status == SemanticFocusAdmissionStatus::Disabled,
                                 "disabled semantic focus request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(not_focusable.status == SemanticFocusRequestStatus::Rejected
                                 && not_focusable.admission.status == SemanticFocusAdmissionStatus::NotFocusable,
                                 "non-focusable semantic focus request is rejected")) {
        return 1;
    }

    const auto ambiguous = scene.request_semantic_focus(handles.root, "action.primary");
    const auto invalid_root = scene.request_semantic_focus({}, "action.primary");
    const auto missing_id = scene.request_semantic_focus(handles.scope, "");
    run_log.case_begin("invalid_or_ambiguous");
    std::printf(" ambiguous=%s invalid_root=%s missing_id=%s focus_preserved=%d\n",
                semantic_focus_admission_status_name(ambiguous.admission.status),
                semantic_focus_admission_status_name(invalid_root.admission.status),
                semantic_focus_admission_status_name(missing_id.admission.status),
                vivid::evidence::same_handle(access.input_focused(), handles.secondary) ? 1 : 0);
    if (!vivid::evidence::expect(ambiguous.status == SemanticFocusRequestStatus::Rejected
                                 && ambiguous.admission.status == SemanticFocusAdmissionStatus::AmbiguousId,
                                 "ambiguous semantic focus request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(invalid_root.status == SemanticFocusRequestStatus::Rejected
                                 && invalid_root.admission.status == SemanticFocusAdmissionStatus::InvalidRoot,
                                 "invalid root request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing_id.status == SemanticFocusRequestStatus::Rejected
                                 && missing_id.admission.status == SemanticFocusAdmissionStatus::MissingId,
                                 "missing request id is rejected")) {
        return 1;
    }

    const vivid::evidence::CausalChainEvidence chain{
        .name = "row.secondary.focus",
        .request_ok = request.status == SemanticFocusRequestStatus::Committed
            && request.committed
            && request.focus_changed,
        .state_delta_ok = vivid::evidence::same_handle(request.before_focus, handles.primary)
            && vivid::evidence::same_handle(request.after_focus, handles.secondary)
            && vivid::evidence::same_handle(access.input_focused(), handles.secondary),
        .invalidation_ok = style_evidence_equal(style_before, style_after),
        .artifact_ok = transfer_artifact_ok,
        .rejected_no_mutation = outside.status == SemanticFocusRequestStatus::Rejected
            && vivid::evidence::same_handle(access.input_focused(), handles.secondary)
            && !outside_delta.changed,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok() && chain.rejected_no_mutation,
                                 "semantic focus request causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_focus_request_demo] ok");
    return 0;
}
