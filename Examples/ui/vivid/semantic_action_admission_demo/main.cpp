#include <cstddef>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"
#include "../support/vivid_semantic_focus_fixture.hpp"

namespace {
    constexpr vivid::evidence::RunLog kRunLog{"saa", "semantic_action_admission_demo"};

    [[nodiscard]] bool admitted_activate(const SemanticActionAdmission& admission,
                                          WidgetHandle expected) noexcept {
        return admission.status == SemanticActionAdmissionStatus::Admitted
            && admission.intent_status == SemanticIntentStatus::Resolved
            && admission.admitted
            && admission.executable
            && admission.will_request_focus
            && admission.will_emit_click
            && vivid::evidence::same_handle(admission.handle, expected);
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
    const WidgetHandle initial_focus = access.input_focused();
    const std::size_t initial_events = access.input_event_count();
    if (!vivid::evidence::expect(vivid::evidence::same_handle(initial_focus, handles.primary),
                                 "setup establishes primary input focus")) {
        return 1;
    }

    const auto primary = scene.admit_semantic_action(handles.scope, "action.primary", SemanticAction::Activate);
    run_log.case_begin("admit_activate");
    vivid::evidence::print_action_admission_ledger(primary);
    std::printf("\n");
    if (!vivid::evidence::expect(admitted_activate(primary, handles.primary),
                                 "primary activate action is admitted with execution plan")) {
        return 1;
    }

    run_log.case_begin("admission_no_execute");
    std::printf(" focus=primary events_before=%zu events_after=%zu pressed=%d focused=%d checked=0 executed=0\n",
                initial_events,
                access.input_event_count(),
                access.pressed(handles.primary) ? 1 : 0,
                access.focused(handles.primary) ? 1 : 0);
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), initial_focus),
                                 "action admission does not mutate focus truth")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.input_event_count() == initial_events,
                                 "action admission does not emit input events")) {
        return 1;
    }
    if (!vivid::evidence::expect(!access.pressed(handles.primary),
                                 "action admission does not press target")) {
        return 1;
    }

    const auto secondary = scene.admit_semantic_action(handles.scope, "row.secondary", SemanticAction::Activate);
    run_log.case_begin("admit_list_item");
    vivid::evidence::print_action_admission_ledger(secondary);
    std::printf("\n");
    if (!vivid::evidence::expect(admitted_activate(secondary, handles.secondary),
                                 "list item activate action is admitted")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), initial_focus)
                                 && access.input_event_count() == initial_events,
                                 "list item admission remains planning-only")) {
        return 1;
    }

    const auto unsupported =
        scene.admit_semantic_action(handles.scope, "panel.info", SemanticAction::Activate);
    run_log.case_begin("reject_unsupported_action");
    vivid::evidence::print_action_admission_ledger(unsupported);
    std::printf("\n");
    if (!vivid::evidence::expect(unsupported.status == SemanticActionAdmissionStatus::UnsupportedAction
                                 && unsupported.intent_status == SemanticIntentStatus::UnsupportedAction
                                 && !unsupported.admitted
                                 && !unsupported.will_emit_click,
                                 "unsupported action is rejected before execution planning")) {
        return 1;
    }

    const auto disabled =
        scene.admit_semantic_action(handles.scope, "action.disabled", SemanticAction::Activate);
    run_log.case_begin("reject_disabled");
    vivid::evidence::print_action_admission_ledger(disabled);
    std::printf("\n");
    if (!vivid::evidence::expect(disabled.status == SemanticActionAdmissionStatus::Disabled
                                 && disabled.intent_status == SemanticIntentStatus::Disabled
                                 && !disabled.admitted,
                                 "disabled semantic action is rejected")) {
        return 1;
    }

    const auto ambiguous =
        scene.admit_semantic_action(handles.root, "action.primary", SemanticAction::Activate);
    const auto missing =
        scene.admit_semantic_action(handles.scope, "missing.id", SemanticAction::Activate);
    run_log.case_begin("reject_ambiguous_or_missing");
    std::printf(" ambiguous=%s missing=%s ambiguous_matches=%zu missing_found=%d\n",
                semantic_action_admission_status_name(ambiguous.status),
                semantic_action_admission_status_name(missing.status),
                ambiguous.match_count,
                missing.found ? 1 : 0);
    if (!vivid::evidence::expect(ambiguous.status == SemanticActionAdmissionStatus::AmbiguousId
                                 && ambiguous.match_count == 2
                                 && !ambiguous.admitted,
                                 "duplicate semantic action id is ambiguous")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing.status == SemanticActionAdmissionStatus::NotFound
                                 && !missing.admitted,
                                 "missing semantic action id is rejected")) {
        return 1;
    }

    const auto invalid_root =
        scene.admit_semantic_action({}, "action.primary", SemanticAction::Activate);
    const auto missing_id =
        scene.admit_semantic_action(handles.scope, "", SemanticAction::Activate);
    run_log.case_begin("reject_invalid_request");
    std::printf(" invalid_root=%s missing_id=%s focus_preserved=%d events_preserved=%d\n",
                semantic_action_admission_status_name(invalid_root.status),
                semantic_action_admission_status_name(missing_id.status),
                vivid::evidence::same_handle(access.input_focused(), initial_focus) ? 1 : 0,
                access.input_event_count() == initial_events ? 1 : 0);
    if (!vivid::evidence::expect(invalid_root.status == SemanticActionAdmissionStatus::InvalidRoot,
                                 "invalid root is explicit")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing_id.status == SemanticActionAdmissionStatus::MissingId,
                                 "missing request id is explicit")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), initial_focus)
                                 && access.input_event_count() == initial_events,
                                 "rejected action admission leaves input truth untouched")) {
        return 1;
    }

    run_log.case_begin("rejection_no_execute");
    const bool rejected_no_execute =
        vivid::evidence::same_handle(access.input_focused(), initial_focus)
        && access.input_event_count() == initial_events
        && !access.pressed(handles.primary)
        && !unsupported.admitted
        && !disabled.admitted
        && !ambiguous.admitted
        && !missing.admitted
        && !invalid_root.admitted
        && !missing_id.admitted
        && !unsupported.will_emit_click
        && !disabled.will_emit_click
        && !ambiguous.will_emit_click
        && !missing.will_emit_click
        && !invalid_root.will_emit_click
        && !missing_id.will_emit_click;
    std::printf(" focus_preserved=%d events_preserved=%d pressed=%d rejected_admitted=0 rejected_click_plan=0 unsupported=%s disabled=%s ambiguous=%s missing=%s invalid_root=%s missing_id=%s\n",
                vivid::evidence::same_handle(access.input_focused(), initial_focus) ? 1 : 0,
                access.input_event_count() == initial_events ? 1 : 0,
                access.pressed(handles.primary) ? 1 : 0,
                semantic_action_admission_status_name(unsupported.status),
                semantic_action_admission_status_name(disabled.status),
                semantic_action_admission_status_name(ambiguous.status),
                semantic_action_admission_status_name(missing.status),
                semantic_action_admission_status_name(invalid_root.status),
                semantic_action_admission_status_name(missing_id.status));
    if (!vivid::evidence::expect(rejected_no_execute,
                                 "rejected action admissions remain planning-only")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_action_admission_demo] ok");
    return 0;
}
