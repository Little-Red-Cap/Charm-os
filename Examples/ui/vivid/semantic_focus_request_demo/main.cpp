#include <cstddef>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 360, 208};
    constexpr Rect kScopeBounds{12, 12, 210, 184};
    constexpr Rect kPrimaryBounds{24, 24, 132, 30};
    constexpr Rect kSecondaryBounds{24, 66, 132, 30};
    constexpr Rect kInfoBounds{24, 108, 132, 30};
    constexpr Rect kDisabledBounds{24, 150, 132, 30};
    constexpr Rect kOutsideBounds{240, 24, 96, 30};
    constexpr Rect kDuplicateBounds{240, 66, 96, 30};
    constexpr vivid::evidence::RunLog kRunLog{"sfr", "semantic_focus_request_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle primary{};
        WidgetHandle secondary{};
        WidgetHandle info{};
        WidgetHandle disabled{};
        WidgetHandle outside{};
        WidgetHandle duplicate{};
    };

    [[nodiscard]] bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs == rhs;
    }

    void click(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
    }

    void print_request(const SemanticFocusRequest& request) noexcept {
        std::printf(" status=%s admission=%s committed=%d focus_changed=%d focus_out=%d focus_in=%d events=%zu before=%s after=%s id=%s\n",
                    semantic_focus_request_status_name(request.status),
                    semantic_focus_admission_status_name(request.admission.status),
                    request.committed ? 1 : 0,
                    request.focus_changed ? 1 : 0,
                    request.emitted_focus_out ? 1 : 0,
                    request.emitted_focus_in ? 1 : 0,
                    request.events_after - request.events_before,
                    request.before_focus ? "set" : "none",
                    request.after_focus ? "set" : "none",
                    request.admission.id);
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
                out_primary = out_primary || same_handle(event.target, primary);
            } else if (event.event.type == Event::Type::FocusIn) {
                ++focus_in;
                in_secondary = in_secondary || same_handle(event.target, secondary);
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
    Handles handles{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        handles.root = builder.create_container();
        handles.scope = builder.create_container();
        handles.primary = builder.create_button_static("Primary");
        handles.secondary = builder.create_list_item("Secondary");
        handles.info = builder.create_container();
        handles.disabled = builder.create_button_static("Disabled");
        handles.outside = builder.create_button_static("Outside");
        handles.duplicate = builder.create_button_static("Duplicate");

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.primary);
        builder.link(handles.scope, handles.secondary);
        builder.link(handles.scope, handles.info);
        builder.link(handles.scope, handles.disabled);
        builder.link(handles.root, handles.outside);
        builder.link(handles.root, handles.duplicate);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.primary, kPrimaryBounds);
        builder.set_rect(handles.secondary, kSecondaryBounds);
        builder.set_rect(handles.info, kInfoBounds);
        builder.set_rect(handles.disabled, kDisabledBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_rect(handles.duplicate, kDuplicateBounds);
        builder.set_semantic_default(handles.primary, "action.primary");
        builder.set_semantic_default(handles.secondary, "row.secondary");
        builder.set_semantic(handles.info, SemanticRole::Container, "panel.info", "Info panel");
        builder.set_semantic_default(handles.disabled, "action.disabled");
        builder.set_semantic_default(handles.outside, "action.outside");
        builder.set_semantic_default(handles.duplicate, "action.primary", "Duplicate primary");
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.primary, true);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    access.set_focusable(handles.primary, true);
    access.set_focusable(handles.secondary, true);
    access.set_focusable(handles.disabled, true);
    access.set_focusable(handles.outside, true);
    access.set_focusable(handles.duplicate, true);
    access.set_enabled(handles.disabled, false);
    click(scene, kPrimaryBounds, 10);
    if (!vivid::evidence::expect(same_handle(access.input_focused(), handles.primary),
                                 "setup establishes primary input focus")) {
        return 1;
    }

    const auto request = scene.request_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("commit_transfer");
    print_request(request);
    if (!vivid::evidence::expect(request.status == SemanticFocusRequestStatus::Committed,
                                 "semantic focus request commits transfer")) {
        return 1;
    }
    if (!vivid::evidence::expect(request.committed && request.focus_changed,
                                 "semantic focus request changes input truth")) {
        return 1;
    }
    if (!vivid::evidence::expect(same_handle(request.before_focus, handles.primary)
                                 && same_handle(request.after_focus, handles.secondary),
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
    if (!vivid::evidence::expect(same_handle(access.input_focused(), handles.secondary),
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
    print_request(already);
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
    print_request(outside);
    if (!vivid::evidence::expect(outside.status == SemanticFocusRequestStatus::Rejected
                                 && outside.admission.status == SemanticFocusAdmissionStatus::OutsideActiveScope,
                                 "outside active scope request is rejected")) {
        return 1;
    }
    if (!vivid::evidence::expect(same_handle(access.input_focused(), handles.secondary),
                                 "outside rejection preserves current focus")) {
        return 1;
    }

    const auto disabled = scene.request_semantic_focus(handles.scope, "action.disabled");
    const auto not_focusable = scene.request_semantic_focus(handles.scope, "panel.info");
    run_log.case_begin("reject_disabled_or_not_focusable");
    std::printf(" disabled=%s not_focusable=%s focus_preserved=%d\n",
                semantic_focus_admission_status_name(disabled.admission.status),
                semantic_focus_admission_status_name(not_focusable.admission.status),
                same_handle(access.input_focused(), handles.secondary) ? 1 : 0);
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
                same_handle(access.input_focused(), handles.secondary) ? 1 : 0);
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
