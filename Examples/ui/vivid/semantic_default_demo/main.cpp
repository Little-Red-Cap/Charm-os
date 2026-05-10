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
    constexpr Rect kSceneBounds{0, 0, 320, 180};
    constexpr Rect kButtonBounds{16, 20, 96, 30};
    constexpr Rect kListItemBounds{16, 62, 148, 30};
    constexpr Rect kMenuItemBounds{16, 104, 124, 30};
    constexpr Rect kDecorativeBounds{178, 20, 108, 24};
    constexpr Rect kCustomBounds{178, 62, 108, 30};
    constexpr vivid::evidence::RunLog kRunLog{"sdef", "semantic_default_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle action{};
        WidgetHandle row{};
        WidgetHandle menu{};
        WidgetHandle decorative{};
        WidgetHandle custom{};
    };

    [[nodiscard]] bool text_equals(const char* lhs, const char* rhs) noexcept {
        if (!lhs || !rhs) return lhs == rhs;
        while (*lhs && *rhs) {
            if (*lhs != *rhs) return false;
            ++lhs;
            ++rhs;
        }
        return *lhs == *rhs;
    }

    [[nodiscard]] bool snapshot_is(const SemanticFocusSnapshot& snapshot,
                                   const char* id,
                                   const char* role,
                                   const char* label) noexcept {
        return snapshot.found
            && text_equals(snapshot.id, id)
            && text_equals(snapshot.role, role)
            && text_equals(snapshot.label, label);
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
        handles.action = builder.create_button_static("Apply");
        handles.row = builder.create_list_item("Track One");
        handles.menu = builder.create_button_static("Open Menu");
        handles.decorative = builder.create_label_static("Decorative");
        handles.custom = builder.create_scroll_container();

        builder.link(handles.root, handles.action);
        builder.link(handles.root, handles.row);
        builder.link(handles.root, handles.menu);
        builder.link(handles.root, handles.decorative);
        builder.link(handles.root, handles.custom);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.action, kButtonBounds);
        builder.set_rect(handles.row, kListItemBounds);
        builder.set_rect(handles.menu, kMenuItemBounds);
        builder.set_rect(handles.decorative, kDecorativeBounds);
        builder.set_rect(handles.custom, kCustomBounds);
        builder.set_semantic_default(handles.action, "action.apply");
        builder.set_semantic_default(handles.row, "row.track_one");
        builder.set_semantic_default(handles.menu, "menu.open", "Open app menu");
        builder.set_semantic_default(handles.custom, "panel.custom", "Custom panel");
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.root, handles.action, true);
        builder.set_root(handles.root);
    });

    const auto action = scene.semantic_snapshot(handles.action);
    run_log.case_begin("button_default");
    std::printf(" id=%s role=%s label=%s stable_id=product label_source=text\n",
                action.id,
                action.role,
                action.label);
    if (!vivid::evidence::expect(snapshot_is(action, "action.apply", "button", "Apply"),
                                 "button default semantic resolves")) return 1;

    const auto row = scene.semantic_snapshot(handles.row);
    run_log.case_begin("list_item_default");
    std::printf(" id=%s role=%s label=%s stable_id=product label_source=text\n",
                row.id,
                row.role,
                row.label);
    if (!vivid::evidence::expect(snapshot_is(row, "row.track_one", "list_item", "Track One"),
                                 "list item default semantic resolves")) return 1;

    const auto menu = scene.semantic_snapshot(handles.menu);
    run_log.case_begin("label_override");
    std::printf(" id=%s role=%s label=%s label_source=override text=%s\n",
                menu.id,
                menu.role,
                menu.label,
                scene.text(handles.menu));
    if (!vivid::evidence::expect(snapshot_is(menu, "menu.open", "button", "Open app menu"),
                                 "override label wins over text")) return 1;

    const auto decorative = scene.semantic_snapshot(handles.decorative);
    run_log.case_begin("decorative_opt_in_boundary");
    std::printf(" widget=label text=%s semantic_found=%d auto_created=0\n",
                scene.text(handles.decorative),
                decorative.found ? 1 : 0);
    if (!vivid::evidence::expect(!decorative.found,
                                 "decorative label remains non semantic without opt-in")) return 1;

    auto access = scene.access();
    access.set_semantic(handles.menu, SemanticRole::ListItem, "menu.override", "Menu as row");
    const auto explicit_override = access.semantic_snapshot(handles.menu);
    run_log.case_begin("explicit_override");
    std::printf(" id=%s role=%s label=%s source=explicit_override\n",
                explicit_override.id,
                explicit_override.role,
                explicit_override.label);
    if (!vivid::evidence::expect(snapshot_is(explicit_override, "menu.override", "list_item", "Menu as row"),
                                 "explicit semantic can override default")) return 1;

    const auto tree = scene.semantic_tree_snapshot(handles.root);
    run_log.case_begin("tree_artifact_defaults");
    std::printf(" nodes=%zu hash=%u ids=%s,%s,%s,%s decorative_included=0\n",
                tree.node_count,
                tree.semantic_hash,
                tree.nodes[0].id,
                tree.nodes[1].id,
                tree.nodes[2].id,
                tree.nodes[3].id);
    if (!vivid::evidence::expect(tree.node_count == 4,
                                 "semantic default tree contains four opted-in nodes")) return 1;
    if (!vivid::evidence::expect(text_equals(tree.nodes[0].id, "action.apply"),
                                 "default tree preserves button id")) return 1;
    if (!vivid::evidence::expect(text_equals(tree.nodes[1].id, "row.track_one"),
                                 "default tree preserves row id")) return 1;
    if (!vivid::evidence::expect(text_equals(tree.nodes[2].id, "menu.override"),
                                 "default tree sees explicit override")) return 1;
    if (!vivid::evidence::expect(text_equals(tree.nodes[3].id, "panel.custom"),
                                 "default tree preserves container opt-in")) return 1;

    const vivid::evidence::CausalChainEvidence chain{
        .name = "semantic_default.artifact",
        .request_ok = snapshot_is(action, "action.apply", "button", "Apply")
            && snapshot_is(row, "row.track_one", "list_item", "Track One")
            && snapshot_is(menu, "menu.open", "button", "Open app menu")
            && snapshot_is(explicit_override, "menu.override", "list_item", "Menu as row"),
        .state_delta_ok = explicit_override.found
            && text_equals(explicit_override.id, "menu.override")
            && text_equals(explicit_override.role, "list_item")
            && text_equals(explicit_override.label, "Menu as row"),
        .invalidation_ok = !decorative.found,
        .artifact_ok = tree.node_count == 4
            && text_equals(tree.nodes[0].id, "action.apply")
            && text_equals(tree.nodes[1].id, "row.track_one")
            && text_equals(tree.nodes[2].id, "menu.override")
            && text_equals(tree.nodes[3].id, "panel.custom"),
        .rejected_no_mutation = !decorative.found,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok(),
                                 "semantic default causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_default_demo] ok");
    return 0;
}
