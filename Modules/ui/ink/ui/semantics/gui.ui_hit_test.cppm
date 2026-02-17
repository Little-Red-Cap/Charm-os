//
// Minimal hit-test helpers for UI trees.
//

module;
#include <cstdint>
export module gui.ui_hit_test;

import gui.core;
import gui.ui_tree;

export namespace gui::ui {

    struct HitTestFocus {
        NodeId focused{kNullId};
    };

    inline void focus_set(HitTestFocus& f, NodeId id) noexcept { f.focused = id; }
    [[nodiscard]] inline bool is_focused(const HitTestFocus& f, NodeId id) noexcept { return f.focused == id; }

    template<int MaxNodes, int MaxDepth>
    [[nodiscard]] NodeIndex hit_test_node(const Tree<MaxNodes, MaxDepth>& tree,
                                          NodeIndex idx,
                                          std::int16_t x,
                                          std::int16_t y) noexcept
    {
        if (idx == kNullIndex) return kNullIndex;
        const Node& n = tree.node(idx);
        if (!contains(n.rect, x, y)) return kNullIndex;

        NodeIndex child = n.first_child;
        while (child != kNullIndex) {
            if (auto hit = hit_test_node(tree, child, x, y); hit != kNullIndex) {
                return hit;
            }
            child = tree.node(child).next_sibling;
        }
        return idx;
    }

    template<int MaxNodes, int MaxDepth>
    [[nodiscard]] NodeIndex hit_test(const Tree<MaxNodes, MaxDepth>& tree,
                                     std::int16_t x,
                                     std::int16_t y) noexcept
    {
        return hit_test_node(tree, tree.root(), x, y);
    }

    template<int MaxNodes, int MaxDepth>
    [[nodiscard]] NodeId hit_test_id(const Tree<MaxNodes, MaxDepth>& tree,
                                     std::int16_t x,
                                     std::int16_t y) noexcept
    {
        const NodeIndex idx = hit_test(tree, x, y);
        return (idx == kNullIndex) ? kNullId : tree.node(idx).id;
    }

} // namespace gui::ui
