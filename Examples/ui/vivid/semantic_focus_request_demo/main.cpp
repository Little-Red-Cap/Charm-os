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
    constexpr vivid::evidence::RunLog kRunLog{"sfr", "semantic_focus_request_demo"};

    void print_request_ledger(const SemanticFocusRequest& request) noexcept {
        const SemanticFocusRequestLedger ledger = semantic_focus_request_ledger(request);
        std::printf(" ledger=focus_request stage=%s status=%s admission=%s query=%s admitted=%d transfer_needed=%d committed=%d focus_changed=%d focus_out=%d focus_in=%d events_before=%zu events_after=%zu focus_before=%s focus_after=%s id=%s\n",
                    semantic_focus_request_stage_name(ledger.stage),
                    semantic_focus_request_status_name(ledger.status),
                    semantic_focus_admission_status_name(ledger.admission_status),
                    semantic_focus_query_status_name(ledger.query_status),
                    ledger.admitted ? 1 : 0,
                    ledger.transfer_needed ? 1 : 0,
                    ledger.committed ? 1 : 0,
                    ledger.focus_changed ? 1 : 0,
                    ledger.emitted_focus_out ? 1 : 0,
                    ledger.emitted_focus_in ? 1 : 0,
                    ledger.events_before,
                    ledger.events_after,
                    ledger.focus_started_on_target ? "target" : "other",
                    ledger.focus_ended_on_target ? "target" : "other",
                    ledger.id);
    }

    void count_focus_events(::ui::scene::SceneAccess& access,
                            WidgetHandle primary,
                            WidgetHandle secondary,
                            int& focus_out,
                            int& focus_in,
                            bool& out_primary,
                            bool& in_secondary) noexcept {
        focus_out = 0;
        focus_in = 0;
        out_primary = false;
        in_secondary = false;
        for (std::size_t index = 0; index < access.input_event_count(); ++index) {
            const auto& event = access.input_event(index);
            if (event.event.type == Event::Type::FocusOut) {
                ++focus_out;
                out_primary = out_primary || vivid::evidence::same_handle(event.target, primary);
            } else if (event.event.type == Event::Type::FocusIn) {
                ++focus_in;
                in_secondary = in_secondary || vivid::evidence::same_handle(event.target, secondary);
            }
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
    vivid::evidence::SemanticFocusFixtureHandles handles{};
    vivid::evidence::build_semantic_focus_fixture(scene, handles);

    auto access = scene.access();
    vivid::evidence::configure_semantic_focus_fixture(access, handles);
    vivid::evidence::click_center(scene, vivid::evidence::kSemanticFocusPrimaryBounds, 10);
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), handles.primary),
                                 "setup establishes primary input focus")) {
        return 1;
    }

    const auto request = scene.request_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("commit_transfer");
    print_request_ledger(request);
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
    int focus_out = 0;
    int focus_in = 0;
    bool out_primary = false;
    bool in_secondary = false;
    count_focus_events(access, handles.primary, handles.secondary, focus_out, focus_in, out_primary, in_secondary);
    std::printf(" focus_out=%d focus_in=%d out_primary=%d in_secondary=%d event_count=%zu\n",
                focus_out,
                focus_in,
                out_primary ? 1 : 0,
                in_secondary ? 1 : 0,
                access.input_event_count());
    if (!vivid::evidence::expect(focus_out == 1 && focus_in == 1,
                                 "request emits one FocusOut and one FocusIn")) {
        return 1;
    }
    if (!vivid::evidence::expect(out_primary && in_secondary,
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

    const auto already = scene.request_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("already_focused_noop");
    print_request_ledger(already);
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
    print_request_ledger(outside);
    if (!vivid::evidence::expect(outside.status == SemanticFocusRequestStatus::Rejected
                                 && outside.admission.status == SemanticFocusAdmissionStatus::OutsideActiveScope,
                                 "outside active scope request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), handles.secondary),
                                 "outside rejection preserves current focus")) {
        return 1;
    }

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

    run_log.end(true);
    std::puts("[semantic_focus_request_demo] ok");
    return 0;
}
