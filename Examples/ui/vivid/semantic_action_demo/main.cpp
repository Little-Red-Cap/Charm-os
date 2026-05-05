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
    constexpr Rect kSceneBounds{0, 0, 320, 180};
    constexpr Rect kButtonBounds{16, 20, 112, 30};
    constexpr Rect kListItemBounds{16, 62, 148, 30};
    constexpr Rect kPanelBounds{16, 104, 148, 40};
    constexpr Rect kTextBounds{184, 20, 96, 24};
    constexpr vivid::evidence::RunLog kRunLog{"sact", "semantic_action_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle action{};
        WidgetHandle row{};
        WidgetHandle panel{};
        WidgetHandle text{};
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

    [[nodiscard]] bool has_activate(SemanticActionMask actions) noexcept {
        return semantic_action_present(actions, SemanticAction::Activate);
    }

    [[nodiscard]] std::uint16_t find_id(const ::ui::scene::SemanticTreeSnapshot& tree,
                                        const char* id) noexcept {
        for (std::size_t index = 0; index < tree.node_count; ++index) {
            if (text_equals(tree.nodes[index].id, id)) {
                return static_cast<std::uint16_t>(index);
            }
        }
        return kSemanticTreeNoFocusIndex;
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
        handles.panel = builder.create_container();
        handles.text = builder.create_label_static("Read only");

        builder.link(handles.root, handles.action);
        builder.link(handles.root, handles.row);
        builder.link(handles.root, handles.panel);
        builder.link(handles.root, handles.text);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.action, kButtonBounds);
        builder.set_rect(handles.row, kListItemBounds);
        builder.set_rect(handles.panel, kPanelBounds);
        builder.set_rect(handles.text, kTextBounds);
        builder.set_semantic_default(handles.action, "action.apply");
        builder.set_semantic_default(handles.row, "row.track_one");
        builder.set_semantic(handles.panel, SemanticRole::Container, "panel.info", "Info panel");
        builder.set_semantic(handles.text, SemanticRole::Text, "text.read_only", "Read only text");
        builder.set_root(handles.root);
    });

    const auto button = scene.semantic_action_snapshot(handles.action);
    run_log.case_begin("button_activate_default");
    std::printf(" id=%s actions=%u activate=%d source=role_default\n",
                button.id,
                button.actions,
                has_activate(button.actions) ? 1 : 0);
    if (!vivid::evidence::expect(button.found, "button semantic action snapshot exists")) return 1;
    if (!vivid::evidence::expect(has_activate(button.actions), "button default exposes activate")) return 1;

    const auto row = scene.semantic_action_snapshot(handles.row);
    run_log.case_begin("list_item_activate_default");
    std::printf(" id=%s actions=%u activate=%d source=role_default\n",
                row.id,
                row.actions,
                has_activate(row.actions) ? 1 : 0);
    if (!vivid::evidence::expect(row.found, "list item semantic action snapshot exists")) return 1;
    if (!vivid::evidence::expect(has_activate(row.actions), "list item default exposes activate")) return 1;

    const auto panel = scene.semantic_action_snapshot(handles.panel);
    const auto text = scene.semantic_action_snapshot(handles.text);
    run_log.case_begin("non_action_roles");
    std::printf(" panel_id=%s panel_actions=%u text_id=%s text_actions=%u activate=0\n",
                panel.id,
                panel.actions,
                text.id,
                text.actions);
    if (!vivid::evidence::expect(panel.found && text.found, "non-action semantic snapshots exist")) return 1;
    if (!vivid::evidence::expect(panel.actions == 0 && text.actions == 0,
                                 "container and text expose no default action")) {
        return 1;
    }

    auto access = scene.access();
    access.set_semantic_actions(handles.action, 0);
    const auto cleared = access.semantic_action_snapshot(handles.action);
    run_log.case_begin("explicit_action_clear");
    std::printf(" id=%s actions=%u activate=%d source=explicit_override\n",
                cleared.id,
                cleared.actions,
                has_activate(cleared.actions) ? 1 : 0);
    if (!vivid::evidence::expect(cleared.found && cleared.actions == 0,
                                 "explicit action override can clear activate")) {
        return 1;
    }

    access.set_semantic_actions(handles.panel, semantic_action_mask(SemanticAction::Activate));
    const auto promoted_panel = access.semantic_snapshot(handles.panel);
    const auto tree = scene.semantic_tree_snapshot(handles.root);
    const auto panel_index = find_id(tree, "panel.info");
    run_log.case_begin("tree_action_artifact");
    std::printf(" nodes=%zu panel_index=%u panel_snapshot_actions=%u panel_tree_actions=%u hash=%u\n",
                tree.node_count,
                panel_index,
                promoted_panel.actions,
                panel_index != kSemanticTreeNoFocusIndex ? tree.nodes[panel_index].actions : 0,
                tree.semantic_hash);
    if (!vivid::evidence::expect(panel_index != kSemanticTreeNoFocusIndex,
                                 "semantic tree contains promoted panel")) {
        return 1;
    }
    if (!vivid::evidence::expect(has_activate(promoted_panel.actions),
                                 "semantic snapshot sees explicit panel action")) {
        return 1;
    }
    if (!vivid::evidence::expect(has_activate(tree.nodes[panel_index].actions),
                                 "semantic tree node carries action artifact")) {
        return 1;
    }

    const auto stable_tree = scene.semantic_tree_snapshot(handles.root);
    access.set_semantic_actions(handles.panel, 0);
    const auto changed_tree = scene.semantic_tree_snapshot(handles.root);
    run_log.case_begin("action_hash_stable");
    std::printf(" stable_hash=%u repeat_hash=%u changed_hash=%u repeat_same=%d changed_diff=%d\n",
                tree.semantic_hash,
                stable_tree.semantic_hash,
                changed_tree.semantic_hash,
                stable_tree.semantic_hash == tree.semantic_hash ? 1 : 0,
                changed_tree.semantic_hash != tree.semantic_hash ? 1 : 0);
    if (!vivid::evidence::expect(stable_tree.semantic_hash == tree.semantic_hash,
                                 "semantic action hash is stable when actions do not change")) {
        return 1;
    }
    if (!vivid::evidence::expect(changed_tree.semantic_hash != tree.semantic_hash,
                                 "semantic action hash changes when action mask changes")) {
        return 1;
    }

    const vivid::evidence::CausalChainEvidence chain{
        .name = "semantic_action.artifact",
        .request_ok = button.found
            && has_activate(button.actions)
            && row.found
            && has_activate(row.actions)
            && panel.found
            && text.found
            && panel.actions == 0
            && text.actions == 0,
        .state_delta_ok = cleared.found
            && cleared.actions == 0
            && promoted_panel.found
            && has_activate(promoted_panel.actions),
        .invalidation_ok = tree.node_count > 0
            && panel_index != kSemanticTreeNoFocusIndex
            && has_activate(tree.nodes[panel_index].actions),
        .artifact_ok = stable_tree.semantic_hash == tree.semantic_hash
            && changed_tree.semantic_hash != tree.semantic_hash,
        .rejected_no_mutation = panel.actions == 0
            && text.actions == 0,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok(),
                                 "semantic action causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_action_demo] ok");
    return 0;
}
