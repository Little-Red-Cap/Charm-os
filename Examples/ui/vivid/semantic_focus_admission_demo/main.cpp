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

    void print_admission(const SemanticFocusAdmission& admission) noexcept {
        std::printf(" status=%s query=%s admitted=%d transfer_needed=%d focus_out=%d focus_in=%d found=%d focusable=%d allowed=%d id=%s visited=%zu matches=%zu\n",
                    semantic_focus_admission_status_name(admission.status),
                    semantic_focus_query_status_name(admission.query_status),
                    admission.admitted ? 1 : 0,
                    admission.transfer_needed ? 1 : 0,
                    admission.will_emit_focus_out ? 1 : 0,
                    admission.will_emit_focus_in ? 1 : 0,
                    admission.found ? 1 : 0,
                    admission.focusable ? 1 : 0,
                    admission.allowed_by_scope ? 1 : 0,
                    admission.id,
                    admission.visited_count,
                    admission.match_count);
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
    print_admission(secondary);
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
    print_admission(already);
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
    print_admission(not_focusable);
    if (!vivid::evidence::expect(not_focusable.status == SemanticFocusAdmissionStatus::NotFocusable,
                                 "non-focusable semantic target is rejected")) {
        return 1;
    }

    const auto disabled = scene.admit_semantic_focus(handles.scope, "action.disabled");
    run_log.case_begin("reject_disabled");
    print_admission(disabled);
    if (!vivid::evidence::expect(disabled.status == SemanticFocusAdmissionStatus::Disabled,
                                 "disabled semantic focus target is rejected")) {
        return 1;
    }

    const auto outside = scene.admit_semantic_focus(handles.root, "action.outside");
    run_log.case_begin("reject_outside_scope");
    print_admission(outside);
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

    run_log.end(true);
    std::puts("[semantic_focus_admission_demo] ok");
    return 0;
}
