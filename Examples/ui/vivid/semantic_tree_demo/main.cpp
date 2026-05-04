#include <cstddef>
#include <cstdint>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 320, 196};
    constexpr Rect kScopeBounds{12, 12, 220, 166};
    constexpr Rect kTitleBounds{24, 22, 168, 24};
    constexpr Rect kPrimaryBounds{24, 56, 154, 32};
    constexpr Rect kGroupBounds{24, 98, 174, 64};
    constexpr Rect kSecondaryBounds{12, 10, 138, 24};
    constexpr Rect kBodyTextBounds{12, 38, 126, 18};
    constexpr Rect kOutsideBounds{252, 64, 44, 44};
    constexpr vivid::evidence::RunLog kRunLog{"stree", "semantic_tree_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle title{};
        WidgetHandle primary{};
        WidgetHandle group{};
        WidgetHandle secondary{};
        WidgetHandle body_text{};
        WidgetHandle outside{};
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
        handles.scope = builder.create_container();
        handles.title = builder.create_label_static("Semantic Tree");
        handles.primary = builder.create_scroll_container();
        handles.group = builder.create_container();
        handles.secondary = builder.create_scroll_container();
        handles.body_text = builder.create_label_static("Decorative copy");
        handles.outside = builder.create_scroll_container();

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.title);
        builder.link(handles.scope, handles.primary);
        builder.link(handles.scope, handles.group);
        builder.link(handles.group, handles.secondary);
        builder.link(handles.group, handles.body_text);
        builder.link(handles.root, handles.outside);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.title, kTitleBounds);
        builder.set_rect(handles.primary, kPrimaryBounds);
        builder.set_rect(handles.group, kGroupBounds);
        builder.set_rect(handles.secondary, kSecondaryBounds);
        builder.set_rect(handles.body_text, kBodyTextBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_semantic(handles.scope, SemanticRole::Container, "library", "Library panel");
        builder.set_semantic(handles.primary, SemanticRole::Button, "play", "Play now");
        builder.set_semantic(handles.group, SemanticRole::Container, "queue", "Queue group");
        builder.set_semantic(handles.secondary, SemanticRole::ListItem, "track-1", "Track one");
        builder.set_semantic(handles.outside, SemanticRole::Button, "outside", "Outside action");
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.primary, true);
        builder.set_root(handles.root);
    });

    const auto root_tree = scene.semantic_tree_snapshot(handles.root);
    const auto scope_tree = scene.semantic_tree_snapshot(handles.scope);

    run_log.case_begin("semantic_tree_model");
    std::printf(" root_nodes=%zu scope_nodes=%zu visited=%zu overflowed=%d hash=%u\n",
                root_tree.node_count,
                scope_tree.node_count,
                root_tree.visited_count,
                root_tree.overflowed ? 1 : 0,
                root_tree.semantic_hash);
    if (!vivid::evidence::expect(root_tree.node_count == 5, "root semantic tree includes five nodes")) return 1;
    if (!vivid::evidence::expect(scope_tree.node_count == 4, "scope semantic tree includes four nodes")) return 1;
    if (!vivid::evidence::expect(root_tree.total_semantic_count == 5, "root total semantic count stable")) return 1;
    if (!vivid::evidence::expect(!root_tree.overflowed, "root tree does not overflow")) return 1;

    run_log.case_begin("preorder_collect");
    std::printf(" order=%s,%s,%s,%s,%s depths=%u,%u,%u,%u,%u\n",
                root_tree.nodes[0].id,
                root_tree.nodes[1].id,
                root_tree.nodes[2].id,
                root_tree.nodes[3].id,
                root_tree.nodes[4].id,
                root_tree.nodes[0].depth,
                root_tree.nodes[1].depth,
                root_tree.nodes[2].depth,
                root_tree.nodes[3].depth,
                root_tree.nodes[4].depth);
    if (!vivid::evidence::expect(text_equals(root_tree.nodes[0].id, "library"), "preorder starts at scope")) return 1;
    if (!vivid::evidence::expect(text_equals(root_tree.nodes[1].id, "play"), "preorder visits primary")) return 1;
    if (!vivid::evidence::expect(text_equals(root_tree.nodes[2].id, "queue"), "preorder visits group")) return 1;
    if (!vivid::evidence::expect(text_equals(root_tree.nodes[3].id, "track-1"), "preorder visits nested list item")) return 1;
    if (!vivid::evidence::expect(text_equals(root_tree.nodes[4].id, "outside"), "preorder visits outside after scope subtree")) return 1;
    if (!vivid::evidence::expect(root_tree.nodes[3].depth == 3, "nested semantic depth is stable")) return 1;

    vivid::evidence::click_center(scene, scene.world_rect(handles.secondary), 10);
    const auto focused_tree = scene.semantic_tree_snapshot(handles.root);
    const auto focus_index = find_id(focused_tree, "track-1");

    run_log.case_begin("focus_marker");
    std::printf(" focus_found=%d focus_index=%u focus_id=%s node_focused=%d input_truth=track-1\n",
                focused_tree.focus_found ? 1 : 0,
                focused_tree.focus_index,
                focused_tree.focus_id,
                focus_index != kSemanticTreeNoFocusIndex && focused_tree.nodes[focus_index].focused ? 1 : 0);
    if (!vivid::evidence::expect(focused_tree.focus_found, "semantic tree marks focused node")) return 1;
    if (!vivid::evidence::expect(focus_index != kSemanticTreeNoFocusIndex, "focused semantic id exists")) return 1;
    if (!vivid::evidence::expect(focused_tree.focus_index == focus_index, "focus index points to semantic node")) return 1;
    if (!vivid::evidence::expect(text_equals(focused_tree.focus_id, "track-1"), "focus id is stable")) return 1;

    const auto title_entry = scene.semantic_snapshot(handles.title);
    const auto body_entry = scene.semantic_snapshot(handles.body_text);
    const auto title_index = find_id(focused_tree, "Semantic Tree");

    run_log.case_begin("decorative_excluded");
    std::printf(" title_semantic=%d body_semantic=%d title_in_tree=%d body_in_tree=%d\n",
                title_entry.found ? 1 : 0,
                body_entry.found ? 1 : 0,
                title_index != kSemanticTreeNoFocusIndex ? 1 : 0,
                find_id(focused_tree, "Decorative copy") != kSemanticTreeNoFocusIndex ? 1 : 0);
    if (!vivid::evidence::expect(!title_entry.found, "title has no semantic entry")) return 1;
    if (!vivid::evidence::expect(!body_entry.found, "body text has no semantic entry")) return 1;
    if (!vivid::evidence::expect(title_index == kSemanticTreeNoFocusIndex, "decorative title excluded from tree")) return 1;

    const auto focused_scope_tree = scene.semantic_tree_snapshot(handles.scope);
    const bool outside_in_root = find_id(focused_tree, "outside") != kSemanticTreeNoFocusIndex;
    const bool outside_in_scope = find_id(focused_scope_tree, "outside") != kSemanticTreeNoFocusIndex;

    run_log.case_begin("root_policy");
    std::printf(" root_includes_outside=%d scope_includes_outside=%d scope_nodes=%zu focus_scope_only_navigation=1\n",
                outside_in_root ? 1 : 0,
                outside_in_scope ? 1 : 0,
                focused_scope_tree.node_count);
    if (!vivid::evidence::expect(outside_in_root, "root tree includes outside semantic node")) return 1;
    if (!vivid::evidence::expect(!outside_in_scope, "scope tree excludes outside semantic node")) return 1;
    if (!vivid::evidence::expect(focused_scope_tree.node_count == 4, "scope tree policy is root-bound")) return 1;

    const auto tiny_tree = scene.semantic_tree_snapshot(handles.root, 3);
    const auto zero_tree = scene.semantic_tree_snapshot(handles.root, 0);
    const auto stable_tree = scene.semantic_tree_snapshot(handles.root);

    run_log.case_begin("overflow_and_hash");
    std::printf(" tiny_nodes=%zu total=%zu overflowed=%d zero_nodes=%zu zero_focus=%d stable_hash=%d hash=%u focus_hash=%u\n",
                tiny_tree.node_count,
                tiny_tree.total_semantic_count,
                tiny_tree.overflowed ? 1 : 0,
                zero_tree.node_count,
                zero_tree.focus_found ? 1 : 0,
                stable_tree.semantic_hash == focused_tree.semantic_hash ? 1 : 0,
                stable_tree.semantic_hash,
                focused_tree.semantic_hash);
    if (!vivid::evidence::expect(tiny_tree.node_count == 3, "tiny tree stores only capacity nodes")) return 1;
    if (!vivid::evidence::expect(tiny_tree.total_semantic_count == 5, "tiny tree still counts all semantic nodes")) return 1;
    if (!vivid::evidence::expect(tiny_tree.overflowed, "tiny tree reports overflow")) return 1;
    if (!vivid::evidence::expect(tiny_tree.focus_found && text_equals(tiny_tree.focus_id, "track-1"),
                                 "overflow tree preserves focus truth")) return 1;
    if (!vivid::evidence::expect(zero_tree.node_count == 0 && zero_tree.overflowed,
                                 "zero capacity tree reports overflow")) return 1;
    if (!vivid::evidence::expect(zero_tree.focus_found && text_equals(zero_tree.focus_id, "track-1"),
                                 "zero capacity tree preserves focus truth")) return 1;
    if (!vivid::evidence::expect(stable_tree.semantic_hash == focused_tree.semantic_hash, "semantic hash is stable")) return 1;

    run_log.end(true);
    std::puts("[semantic_tree_demo] ok");
    return 0;
}
