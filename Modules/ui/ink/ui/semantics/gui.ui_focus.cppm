//
// Minimal focus list helper (fixed capacity).
//

module;
#include <cstdint>
export module gui.ui_focus;

import gui.ui_tree;
import gui.ui_semantics;
import service.fixed_list;
import util.core;

export namespace gui::ui {

    template<int Max>
    struct FocusList {
        service::FixedList<NodeId, static_cast<util::usize>(Max)> ids{};
        int index{0};

        void reset() noexcept {
            ids.clear();
            index = 0;
        }

        void add(NodeId id) noexcept {
            (void)ids.push_back(id);
        }

        [[nodiscard]] int count() const noexcept {
            return static_cast<int>(ids.size());
        }

        [[nodiscard]] NodeId at(int i) const noexcept {
            return ids[static_cast<util::usize>(i)];
        }

        [[nodiscard]] NodeId current() const noexcept {
            const int n = count();
            if (n <= 0) return kNullId;
            const int i = (index < 0) ? 0 : (index >= n ? (n - 1) : index);
            return at(i);
        }

        void move(int delta, bool ring = true) noexcept {
            const int n = count();
            if (n <= 0 || delta == 0) return;
            int i = index + delta;
            if (ring) {
                i %= n;
                if (i < 0) i += n;
            } else {
                if (i < 0) i = 0;
                if (i >= n) i = n - 1;
            }
            index = i;
        }
    };

    template<int MaxNodes, int MaxDepth, int Max>
    void collect_focusables(const Tree<MaxNodes, MaxDepth>& tree,
                            NodeIndex idx,
                            FocusList<Max>& list) noexcept
    {
        if (idx == kNullIndex) return;
        const Node& n = tree.node(idx);
        if (is_focusable(n)) {
            list.add(n.id);
        }
        NodeIndex child = n.first_child;
        while (child != kNullIndex) {
            collect_focusables(tree, child, list);
            child = tree.node(child).next_sibling;
        }
    }

    template<int MaxNodes, int MaxDepth, int Max>
    void collect_focusables(const Tree<MaxNodes, MaxDepth>& tree,
                            FocusList<Max>& list) noexcept
    {
        collect_focusables(tree, tree.root(), list);
    }

    template<int MaxNodes, int MaxDepth>
    [[nodiscard]] NodeIndex find_node_by_id(const Tree<MaxNodes, MaxDepth>& tree, NodeId id) noexcept
    {
        const int n = tree.count();
        for (int i = 0; i < n; ++i) {
            if (tree.node((NodeIndex)i).id == id) return (NodeIndex)i;
        }
        return kNullIndex;
    }

    template<int MaxNodes, int MaxDepth, int Max>
    void collect_focus_domain(const Tree<MaxNodes, MaxDepth>& tree,
                              NodeId root_id,
                              FocusList<Max>& list) noexcept
    {
        const NodeIndex root = find_node_by_id(tree, root_id);
        if (root == kNullIndex) return;
        collect_focusables(tree, root, list);
    }

    template<int Max>
    [[nodiscard]] int index_of(const FocusList<Max>& list, NodeId id) noexcept
    {
        const int n = list.count();
        for (int i = 0; i < n; ++i) {
            if (list.at(i) == id) return i;
        }
        return -1;
    }

    template<int Max>
    void fill_linear_domain(NodeId domain_id,
                            std::int16_t count,
                            FocusList<Max>& domain) noexcept
    {
        domain.reset();
        if (count <= 0) return;
        const std::int16_t max_count = (count > Max) ? (std::int16_t)Max : count;
        for (std::int16_t i = 0; i < max_count; ++i) {
            domain.add(list_id(domain_id, (std::uint16_t)(i + 1)));
        }
    }

    enum class FocusSyncReason : std::uint8_t {
        None = 0,
        DomainChanged = 1,
        TargetInvalid = 2,
        IndexOob = 3,
        EmptyDomain = 4,
    };

    struct FocusSyncResult {
        FocusSyncReason reason{FocusSyncReason::None};
        bool changed{false};
    };

    template<int MaxNodes, int MaxDepth, int MaxFocus>
    FocusSyncResult sync_focus_domain(const Tree<MaxNodes, MaxDepth>& tree,
                                      NodeId domain_id,
                                      FocusList<MaxFocus>& domain,
                                      FocusState& focus) noexcept
    {
        (void)tree;
        FocusSyncResult out{};

        const NodeId prev_domain = focus.domain_id;
        const std::int16_t prev_count = focus.count;
        const std::int16_t prev_index = focus.index;
        const NodeId prev_target = focus.target_id;

        const std::int16_t count = (std::int16_t)domain.count();
        const bool empty = (count <= 0);
        const bool domain_changed = (focus.domain_id != domain_id) || (focus.count != count);
        const int target_idx = empty ? -1 : index_of(domain, focus.target_id);
        const bool target_invalid = (!empty && target_idx < 0);
        const bool index_oob = (!empty && (focus.index < 0 || focus.index >= count));

        if (empty) {
            out.reason = FocusSyncReason::EmptyDomain;
            focus.domain_id = domain_id;
            focus.count = 0;
            focus.index = 0;
            focus.target_id = kNullId;
        } else if (domain_changed || target_invalid || index_oob) {
            if (domain_changed) out.reason = FocusSyncReason::DomainChanged;
            else if (target_invalid) out.reason = FocusSyncReason::TargetInvalid;
            else out.reason = FocusSyncReason::IndexOob;

            int new_index = 0;
            if (target_idx >= 0) {
                new_index = target_idx;
            } else if (focus.index >= 0 && focus.index < count) {
                new_index = focus.index;
            }

            focus.domain_id = domain_id;
            focus.count = count;
            focus.index = (std::int16_t)new_index;
            focus.target_id = domain.at(new_index);
        }

        out.changed = (focus.domain_id != prev_domain)
            || (focus.count != prev_count)
            || (focus.index != prev_index)
            || (focus.target_id != prev_target);
        return out;
    }

    template<int MaxFocus>
    FocusSyncResult sync_focus_domain(NodeId domain_id,
                                      const FocusList<MaxFocus>& domain,
                                      FocusState& focus) noexcept
    {
        FocusSyncResult out{};

        const NodeId prev_domain = focus.domain_id;
        const std::int16_t prev_count = focus.count;
        const std::int16_t prev_index = focus.index;
        const NodeId prev_target = focus.target_id;

        const std::int16_t count = (std::int16_t)domain.count();
        const bool empty = (count <= 0);
        const bool domain_changed = (focus.domain_id != domain_id) || (focus.count != count);
        const int target_idx = empty ? -1 : index_of(domain, focus.target_id);
        const bool target_invalid = (!empty && target_idx < 0);
        const bool index_oob = (!empty && (focus.index < 0 || focus.index >= count));

        if (empty) {
            out.reason = FocusSyncReason::EmptyDomain;
            focus.domain_id = domain_id;
            focus.count = 0;
            focus.index = 0;
            focus.target_id = kNullId;
        } else if (domain_changed || target_invalid || index_oob) {
            if (domain_changed) out.reason = FocusSyncReason::DomainChanged;
            else if (target_invalid) out.reason = FocusSyncReason::TargetInvalid;
            else out.reason = FocusSyncReason::IndexOob;

            int new_index = 0;
            if (target_idx >= 0) {
                new_index = target_idx;
            } else if (focus.index >= 0 && focus.index < count) {
                new_index = focus.index;
            }

            focus.domain_id = domain_id;
            focus.count = count;
            focus.index = (std::int16_t)new_index;
            focus.target_id = domain.at(new_index);
        }

        out.changed = (focus.domain_id != prev_domain)
            || (focus.count != prev_count)
            || (focus.index != prev_index)
            || (focus.target_id != prev_target);
        return out;
    }

} // namespace gui::ui
