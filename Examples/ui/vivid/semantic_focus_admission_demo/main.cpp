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
    constexpr vivid::evidence::RunLog kRunLog{"sfa", "semantic_focus_admission_demo"};

    [[nodiscard]] bool admitted_transfer(const SemanticFocusAdmission& admission) noexcept {
        return admission.status == SemanticFocusAdmissionStatus::Admitted
            && admission.admitted
            && admission.transfer_needed
            && admission.will_emit_focus_out
            && admission.will_emit_focus_in;
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

    const auto secondary = scene.admit_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("admit_transfer");
    vivid::evidence::print_focus_admission_ledger(secondary);
    std::printf("\n");
    if (!vivid::evidence::expect(admitted_transfer(secondary), "secondary semantic focus transfer is admitted")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::same_handle(secondary.handle, handles.secondary), "admission resolves destination handle")) return 1;

    run_log.case_begin("admission_no_commit");
    std::printf(" before_focus=primary after_focus=%s before_events=%zu after_events=%zu committed=0\n",
                vivid::evidence::same_handle(access.input_focused(), handles.primary) ? "primary" : "other",
                initial_events,
                access.input_event_count());
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), initial_focus),
                                 "admission does not mutate focus truth")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.input_event_count() == initial_events,
                                 "admission does not emit focus events")) {
        return 1;
    }

    const auto already = scene.admit_semantic_focus(handles.scope, "action.primary");
    run_log.case_begin("already_focused");
    vivid::evidence::print_focus_admission_ledger(already);
    std::printf("\n");
    if (!vivid::evidence::expect(already.status == SemanticFocusAdmissionStatus::AlreadyFocused,
                                 "current focus is admitted but transfer is unnecessary")) {
        return 1;
    }
    if (!vivid::evidence::expect(already.admitted && !already.transfer_needed,
                                 "already focused admission has no transfer plan")) {
        return 1;
    }

    const auto not_focusable = scene.admit_semantic_focus(handles.scope, "panel.info");
    run_log.case_begin("reject_not_focusable");
    vivid::evidence::print_focus_admission_ledger(not_focusable);
    std::printf("\n");
    if (!vivid::evidence::expect(not_focusable.status == SemanticFocusAdmissionStatus::NotFocusable,
                                 "non-focusable semantic target is rejected")) {
        return 1;
    }

    const auto disabled = scene.admit_semantic_focus(handles.scope, "action.disabled");
    run_log.case_begin("reject_disabled");
    vivid::evidence::print_focus_admission_ledger(disabled);
    std::printf("\n");
    if (!vivid::evidence::expect(disabled.status == SemanticFocusAdmissionStatus::Disabled,
                                 "disabled semantic focus target is rejected")) {
        return 1;
    }

    const auto outside = scene.admit_semantic_focus(handles.root, "action.outside");
    run_log.case_begin("reject_outside_scope");
    vivid::evidence::print_focus_admission_ledger(outside);
    std::printf("\n");
    if (!vivid::evidence::expect(outside.status == SemanticFocusAdmissionStatus::OutsideActiveScope,
                                 "active trapped scope rejects outside admission")) {
        return 1;
    }

    const auto ambiguous = scene.admit_semantic_focus(handles.root, "action.primary");
    const auto missing = scene.admit_semantic_focus(handles.scope, "missing.id");
    const auto invalid_root = scene.admit_semantic_focus({}, "action.primary");
    const auto missing_id = scene.admit_semantic_focus(handles.scope, "");
    run_log.case_begin("invalid_or_ambiguous");
    std::printf(" ambiguous=%s missing=%s invalid_root=%s missing_id=%s ambiguous_matches=%zu\n",
                semantic_focus_admission_status_name(ambiguous.status),
                semantic_focus_admission_status_name(missing.status),
                semantic_focus_admission_status_name(invalid_root.status),
                semantic_focus_admission_status_name(missing_id.status),
                ambiguous.match_count);
    if (!vivid::evidence::expect(ambiguous.status == SemanticFocusAdmissionStatus::AmbiguousId,
                                 "duplicate semantic focus id is ambiguous")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing.status == SemanticFocusAdmissionStatus::NotFound,
                                 "missing semantic focus id is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(invalid_root.status == SemanticFocusAdmissionStatus::InvalidRoot,
                                 "invalid root is explicit")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing_id.status == SemanticFocusAdmissionStatus::MissingId,
                                 "missing request id is explicit")) {
        return 1;
    }

    run_log.case_begin("rejection_no_commit");
    const bool rejected_no_commit =
        vivid::evidence::same_handle(access.input_focused(), initial_focus)
        && access.input_event_count() == initial_events
        && !not_focusable.admitted
        && !disabled.admitted
        && !outside.admitted
        && !ambiguous.admitted
        && !missing.admitted
        && !invalid_root.admitted
        && !missing_id.admitted;
    std::printf(" focus_preserved=%d events_preserved=%d rejected_admitted=0 not_focusable=%s disabled=%s outside=%s ambiguous=%s missing=%s invalid_root=%s missing_id=%s\n",
                vivid::evidence::same_handle(access.input_focused(), initial_focus) ? 1 : 0,
                access.input_event_count() == initial_events ? 1 : 0,
                semantic_focus_admission_status_name(not_focusable.status),
                semantic_focus_admission_status_name(disabled.status),
                semantic_focus_admission_status_name(outside.status),
                semantic_focus_admission_status_name(ambiguous.status),
                semantic_focus_admission_status_name(missing.status),
                semantic_focus_admission_status_name(invalid_root.status),
                semantic_focus_admission_status_name(missing_id.status));
    if (!vivid::evidence::expect(rejected_no_commit,
                                 "rejected focus admissions remain planning-only")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_focus_admission_demo] ok");
    return 0;
}
