#include <cstddef>
#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"
#include "../support/vivid_semantic_focus_fixture.hpp"

namespace {
    constexpr vivid::evidence::RunLog kRunLog{"sfq", "semantic_focus_query_demo"};

    [[nodiscard]] bool resolved(const SemanticFocusQuery& query) noexcept {
        return query.status == SemanticFocusQueryStatus::Resolved
            && query.found
            && query.focusable_now
            && query.allowed_by_scope;
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
    const WidgetHandle initial_focus = access.input_focused();
    const std::size_t initial_events = access.input_event_count();

    const auto primary = scene.query_semantic_focus(handles.scope, "action.primary");
    run_log.case_begin("resolve_inside_scope");
    vivid::evidence::print_focus_query_ledger(primary);
    std::printf("\n");
    if (!vivid::evidence::expect(resolved(primary), "primary focus query resolves")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::same_handle(primary.handle, handles.primary),
                                 "primary focus query returns scoped handle")) {
        return 1;
    }

    run_log.case_begin("query_no_focus_transfer");
    std::printf(" before_focus=%d after_focus=%d before_events=%zu after_events=%zu transfer=0\n",
                initial_focus ? 1 : 0,
                access.input_focused() ? 1 : 0,
                initial_events,
                access.input_event_count());
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), initial_focus),
                                 "query does not commit focus truth")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.input_event_count() == initial_events,
                                 "query does not emit focus events")) {
        return 1;
    }

    const auto not_focusable = scene.query_semantic_focus(handles.scope, "panel.info");
    run_log.case_begin("not_focusable");
    vivid::evidence::print_focus_query_ledger(not_focusable);
    std::printf("\n");
    if (!vivid::evidence::expect(not_focusable.status == SemanticFocusQueryStatus::NotFocusable,
                                 "container semantic target is not focusable")) {
        return 1;
    }

    const auto disabled = scene.query_semantic_focus(handles.scope, "action.disabled");
    run_log.case_begin("disabled_target");
    vivid::evidence::print_focus_query_ledger(disabled);
    std::printf("\n");
    if (!vivid::evidence::expect(disabled.status == SemanticFocusQueryStatus::Disabled,
                                 "disabled focus target is rejected")) {
        return 1;
    }

    const auto outside = scene.query_semantic_focus(handles.root, "action.outside");
    run_log.case_begin("outside_active_scope");
    vivid::evidence::print_focus_query_ledger(outside);
    std::printf("\n");
    if (!vivid::evidence::expect(outside.status == SemanticFocusQueryStatus::OutsideActiveScope,
                                 "active trapped scope rejects outside focus query")) {
        return 1;
    }

    const auto ambiguous = scene.query_semantic_focus(handles.root, "action.primary");
    run_log.case_begin("ambiguous_id");
    vivid::evidence::print_focus_query_ledger(ambiguous);
    std::printf("\n");
    if (!vivid::evidence::expect(ambiguous.status == SemanticFocusQueryStatus::AmbiguousId,
                                 "duplicate semantic focus id is ambiguous")) {
        return 1;
    }
    if (!vivid::evidence::expect(ambiguous.match_count == 2, "ambiguous focus query counts two matches")) return 1;

    const auto missing = scene.query_semantic_focus(handles.scope, "missing.id");
    const auto invalid_root = scene.query_semantic_focus({}, "action.primary");
    const auto missing_id = scene.query_semantic_focus(handles.scope, "");
    run_log.case_begin("invalid_request");
    std::printf(" missing=%s invalid_root=%s missing_id=%s found=%d\n",
                semantic_focus_query_status_name(missing.status),
                semantic_focus_query_status_name(invalid_root.status),
                semantic_focus_query_status_name(missing_id.status),
                missing.found ? 1 : 0);
    if (!vivid::evidence::expect(missing.status == SemanticFocusQueryStatus::NotFound,
                                 "missing focus id is explicit")) {
        return 1;
    }
    if (!vivid::evidence::expect(invalid_root.status == SemanticFocusQueryStatus::InvalidRoot,
                                 "invalid root is explicit")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing_id.status == SemanticFocusQueryStatus::MissingId,
                                 "missing query id is explicit")) {
        return 1;
    }

    run_log.case_begin("rejected_no_focus_transfer");
    const bool rejected_no_focus_transfer =
        vivid::evidence::same_handle(access.input_focused(), initial_focus)
        && access.input_event_count() == initial_events
        && !not_focusable.focusable_now
        && !disabled.focusable_now
        && !outside.focusable_now
        && !ambiguous.focusable_now
        && !missing.focusable_now
        && !invalid_root.focusable_now
        && !missing_id.focusable_now;
    std::printf(" focus_preserved=%d events_preserved=%d rejected_focusable_now=0 not_focusable=%s disabled=%s outside=%s ambiguous=%s missing=%s invalid_root=%s missing_id=%s\n",
                vivid::evidence::same_handle(access.input_focused(), initial_focus) ? 1 : 0,
                access.input_event_count() == initial_events ? 1 : 0,
                semantic_focus_query_status_name(not_focusable.status),
                semantic_focus_query_status_name(disabled.status),
                semantic_focus_query_status_name(outside.status),
                semantic_focus_query_status_name(ambiguous.status),
                semantic_focus_query_status_name(missing.status),
                semantic_focus_query_status_name(invalid_root.status),
                semantic_focus_query_status_name(missing_id.status));
    if (!vivid::evidence::expect(rejected_no_focus_transfer,
                                 "rejected focus queries remain lookup-only")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_focus_query_demo] ok");
    return 0;
}
