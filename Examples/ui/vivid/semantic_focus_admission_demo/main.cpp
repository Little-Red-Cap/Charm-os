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
    constexpr vivid::evidence::RunLog kRunLog{"sfa", "semantic_focus_admission_demo"};

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

    void click(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
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
    const WidgetHandle initial_focus = access.input_focused();
    const std::size_t initial_events = access.input_event_count();
    if (!vivid::evidence::expect(same_handle(initial_focus, handles.primary),
                                 "setup establishes primary input focus")) {
        return 1;
    }

    const auto secondary = scene.admit_semantic_focus(handles.scope, "row.secondary");
    run_log.case_begin("admit_transfer");
    print_admission(secondary);
    if (!vivid::evidence::expect(admitted_transfer(secondary), "secondary semantic focus transfer is admitted")) return 1;
    if (!vivid::evidence::expect(same_handle(secondary.handle, handles.secondary), "admission resolves destination handle")) return 1;

    run_log.case_begin("admission_no_commit");
    std::printf(" before_focus=primary after_focus=%s before_events=%zu after_events=%zu committed=0\n",
                same_handle(access.input_focused(), handles.primary) ? "primary" : "other",
                initial_events,
                access.input_event_count());
    if (!vivid::evidence::expect(same_handle(access.input_focused(), initial_focus),
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
