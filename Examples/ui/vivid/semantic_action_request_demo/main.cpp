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
    constexpr Rect kSceneBounds{0, 0, 360, 196};
    constexpr Rect kScopeBounds{12, 12, 220, 172};
    constexpr Rect kApplyBounds{24, 24, 132, 30};
    constexpr Rect kToggleBounds{24, 68, 132, 30};
    constexpr Rect kInfoBounds{24, 112, 132, 34};
    constexpr Rect kOutsideBounds{248, 24, 88, 30};
    constexpr Rect kDuplicateBounds{248, 68, 88, 30};
    constexpr vivid::evidence::RunLog kRunLog{"sar", "semantic_action_request_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle apply{};
        WidgetHandle toggle{};
        WidgetHandle info{};
        WidgetHandle outside{};
        WidgetHandle duplicate{};
    };

    [[nodiscard]] bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs == rhs;
    }

    [[nodiscard]] std::size_t click_events_since(::ui::scene::SceneAccess& access,
                                                 WidgetHandle target,
                                                 std::size_t begin) noexcept {
        std::size_t count = 0;
        for (std::size_t index = begin; index < access.input_event_count(); ++index) {
            const auto& event = access.input_event(index);
            if (same_handle(event.target, target) && event.event.type == Event::Type::Click) {
                ++count;
            }
        }
        return count;
    }

    void print_request_ledger(const SemanticActionRequest& request) noexcept {
        const SemanticActionRequestLedger ledger = semantic_action_request_ledger(request);
        std::printf(" ledger=action_request stage=%s status=%s reason=%s intent=%s action_admission=%s focus=%s admitted=%d focus_ready=%d executed=%d click=%d events_before=%zu events_after=%zu focus_before=%s focus_after=%s id=%s\n",
                    semantic_action_request_stage_name(ledger.stage),
                    semantic_action_request_status_name(ledger.status),
                    semantic_action_request_reject_reason_name(ledger.reject_reason),
                    semantic_intent_status_name(ledger.intent_status),
                    semantic_action_admission_status_name(ledger.action_admission_status),
                    semantic_focus_request_status_name(ledger.focus_request_status),
                    ledger.admitted ? 1 : 0,
                    ledger.focus_ready ? 1 : 0,
                    ledger.executed ? 1 : 0,
                    ledger.emitted_click ? 1 : 0,
                    ledger.events_before,
                    ledger.events_after,
                    ledger.focus_started_on_target ? "target" : "other",
                    ledger.focus_ended_on_target ? "target" : "other",
                    ledger.id);
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
        handles.apply = builder.create_button_static("Apply");
        handles.toggle = builder.create_checkbox("Enable");
        handles.info = builder.create_container();
        handles.outside = builder.create_button_static("Outside");
        handles.duplicate = builder.create_button_static("Duplicate");

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.apply);
        builder.link(handles.scope, handles.toggle);
        builder.link(handles.scope, handles.info);
        builder.link(handles.root, handles.outside);
        builder.link(handles.root, handles.duplicate);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.apply, kApplyBounds);
        builder.set_rect(handles.toggle, kToggleBounds);
        builder.set_rect(handles.info, kInfoBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_rect(handles.duplicate, kDuplicateBounds);
        builder.set_semantic_default(handles.apply, "action.apply");
        builder.set_semantic_default(handles.toggle, "action.toggle");
        builder.set_semantic(handles.info, SemanticRole::Container, "panel.info", "Info panel");
        builder.set_semantic_default(handles.outside, "action.outside");
        builder.set_semantic_default(handles.duplicate, "action.apply", "Duplicate apply");
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.apply, true);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    access.set_focusable(handles.apply, true);
    access.set_focusable(handles.outside, true);
    access.set_focusable(handles.duplicate, true);
    const std::size_t initial_events = access.input_event_count();
    const auto resolution =
        scene.resolve_semantic_intent(handles.scope, "action.toggle", SemanticAction::Activate);
    run_log.case_begin("resolve_no_execute");
    std::printf(" intent=%s found=%d executable=%d checked=%d events_before=%zu events_after=%zu\n",
                semantic_intent_status_name(resolution.status),
                resolution.found ? 1 : 0,
                resolution.executable ? 1 : 0,
                access.checked(handles.toggle) ? 1 : 0,
                initial_events,
                access.input_event_count());
    if (!vivid::evidence::expect(resolution.status == SemanticIntentStatus::Resolved
                                 && resolution.executable
                                 && same_handle(resolution.handle, handles.toggle),
                                 "semantic action intent resolves target")) {
        return 1;
    }
    if (!vivid::evidence::expect(!access.checked(handles.toggle)
                                 && access.input_event_count() == initial_events,
                                 "semantic action resolution has no execution side effects")) {
        return 1;
    }

    const auto request = scene.request_semantic_action(handles.scope, "action.toggle", SemanticAction::Activate);
    run_log.case_begin("request_executes_activate");
    print_request_ledger(request);
    if (!vivid::evidence::expect(request.status == SemanticActionRequestStatus::Executed
                                 && request.reject_reason == SemanticActionRequestRejectReason::None
                                 && request.executed
                                 && request.emitted_click,
                                 "semantic action request executes activate")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.checked(handles.toggle),
                                 "activate request reuses normal click behavior")) {
        return 1;
    }
    if (!vivid::evidence::expect(same_handle(access.input_focused(), handles.toggle),
                                 "activate request prepares focus before execution")) {
        return 1;
    }

    const std::size_t events_after_first = access.input_event_count();
    const auto already = scene.request_semantic_action(handles.scope, "action.toggle", SemanticAction::Activate);
    run_log.case_begin("already_focused_execute");
    print_request_ledger(already);
    std::printf(" checked=%d clicks=%zu\n",
                access.checked(handles.toggle) ? 1 : 0,
                click_events_since(access, handles.toggle, already.events_before));
    if (!vivid::evidence::expect(already.status == SemanticActionRequestStatus::Executed
                                 && already.reject_reason == SemanticActionRequestRejectReason::None
                                 && already.focus_request.status == SemanticFocusRequestStatus::AlreadyFocused
                                 && already.emitted_click,
                                 "already-focused action still executes without focus transfer")) {
        return 1;
    }
    if (!vivid::evidence::expect(!access.checked(handles.toggle),
                                 "second activate toggles through normal click law")) {
        return 1;
    }

    const auto unsupported =
        scene.request_semantic_action(handles.scope, "panel.info", SemanticAction::Activate);
    run_log.case_begin("unsupported_rejected");
    print_request_ledger(unsupported);
    if (!vivid::evidence::expect(unsupported.status == SemanticActionRequestStatus::Rejected
                                 && unsupported.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
                                 && unsupported.admission.status == SemanticActionAdmissionStatus::UnsupportedAction
                                 && !unsupported.emitted_click,
                                 "unsupported semantic action is rejected before execution")) {
        return 1;
    }

    const auto outside =
        scene.request_semantic_action(handles.root, "action.outside", SemanticAction::Activate);
    run_log.case_begin("outside_scope_rejected");
    print_request_ledger(outside);
    if (!vivid::evidence::expect(outside.status == SemanticActionRequestStatus::Rejected
                                 && outside.reject_reason == SemanticActionRequestRejectReason::FocusRequestRejected
                                 && outside.admission.intent_status == SemanticIntentStatus::Resolved
                                 && outside.focus_request.status == SemanticFocusRequestStatus::Rejected
                                 && outside.focus_request.admission.status == SemanticFocusAdmissionStatus::OutsideActiveScope
                                 && !outside.emitted_click,
                                 "outside active scope semantic action is rejected by focus admission")) {
        return 1;
    }

    const auto ambiguous = scene.request_semantic_action(handles.root, "action.apply", SemanticAction::Activate);
    const auto missing_id = scene.request_semantic_action(handles.scope, "", SemanticAction::Activate);
    run_log.case_begin("invalid_requests");
    std::printf(" ambiguous=%s missing_id=%s reason_ambiguous=%s reason_missing=%s click_ambiguous=%d click_missing=%d focus_preserved=%d\n",
                semantic_intent_status_name(ambiguous.admission.intent_status),
                semantic_intent_status_name(missing_id.admission.intent_status),
                semantic_action_request_reject_reason_name(ambiguous.reject_reason),
                semantic_action_request_reject_reason_name(missing_id.reject_reason),
                ambiguous.emitted_click ? 1 : 0,
                missing_id.emitted_click ? 1 : 0,
                same_handle(access.input_focused(), handles.toggle) ? 1 : 0);
    if (!vivid::evidence::expect(ambiguous.status == SemanticActionRequestStatus::Rejected
                                 && ambiguous.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
                                 && ambiguous.admission.status == SemanticActionAdmissionStatus::AmbiguousId,
                                 "ambiguous action request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing_id.status == SemanticActionRequestStatus::Rejected
                                 && missing_id.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
                                 && missing_id.admission.status == SemanticActionAdmissionStatus::MissingId,
                                 "missing id action request is rejected")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_action_request_demo] ok");
    return 0;
}
