#include <cstddef>
#include <cstdio>

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
    constexpr vivid::evidence::RunLog kRunLog{"sfq", "semantic_focus_query_demo"};

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

    [[nodiscard]] bool resolved(const SemanticFocusQuery& query) noexcept {
        return query.status == SemanticFocusQueryStatus::Resolved
            && query.found
            && query.focusable_now
            && query.allowed_by_scope;
    }

    void print_query(const SemanticFocusQuery& query) noexcept {
        std::printf(" status=%s found=%d focusable=%d allowed=%d focusable_now=%d id=%s visited=%zu matches=%zu\n",
                    semantic_focus_query_status_name(query.status),
                    query.found ? 1 : 0,
                    query.focusable ? 1 : 0,
                    query.allowed_by_scope ? 1 : 0,
                    query.focusable_now ? 1 : 0,
                    query.id,
                    query.visited_count,
                    query.match_count);
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
    const WidgetHandle initial_focus = access.input_focused();
    const std::size_t initial_events = access.input_event_count();

    const auto primary = scene.query_semantic_focus(handles.scope, "action.primary");
    run_log.case_begin("resolve_inside_scope");
    print_query(primary);
    if (!vivid::evidence::expect(resolved(primary), "primary focus query resolves")) return 1;
    if (!vivid::evidence::expect(same_handle(primary.handle, handles.primary),
                                 "primary focus query returns scoped handle")) {
        return 1;
    }

    run_log.case_begin("query_no_focus_transfer");
    std::printf(" before_focus=%d after_focus=%d before_events=%zu after_events=%zu transfer=0\n",
                initial_focus ? 1 : 0,
                access.input_focused() ? 1 : 0,
                initial_events,
                access.input_event_count());
    if (!vivid::evidence::expect(same_handle(access.input_focused(), initial_focus),
                                 "query does not commit focus truth")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.input_event_count() == initial_events,
                                 "query does not emit focus events")) {
        return 1;
    }

    const auto not_focusable = scene.query_semantic_focus(handles.scope, "panel.info");
    run_log.case_begin("not_focusable");
    print_query(not_focusable);
    if (!vivid::evidence::expect(not_focusable.status == SemanticFocusQueryStatus::NotFocusable,
                                 "container semantic target is not focusable")) {
        return 1;
    }

    const auto disabled = scene.query_semantic_focus(handles.scope, "action.disabled");
    run_log.case_begin("disabled_target");
    print_query(disabled);
    if (!vivid::evidence::expect(disabled.status == SemanticFocusQueryStatus::Disabled,
                                 "disabled focus target is rejected")) {
        return 1;
    }

    const auto outside = scene.query_semantic_focus(handles.root, "action.outside");
    run_log.case_begin("outside_active_scope");
    print_query(outside);
    if (!vivid::evidence::expect(outside.status == SemanticFocusQueryStatus::OutsideActiveScope,
                                 "active trapped scope rejects outside focus query")) {
        return 1;
    }

    const auto ambiguous = scene.query_semantic_focus(handles.root, "action.primary");
    run_log.case_begin("ambiguous_id");
    print_query(ambiguous);
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

    run_log.end(true);
    std::puts("[semantic_focus_query_demo] ok");
    return 0;
}
