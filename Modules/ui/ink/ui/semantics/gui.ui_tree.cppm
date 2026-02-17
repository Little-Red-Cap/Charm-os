//
// Minimal UI tree + fixed-capacity state storage.
//

module;
#include <cstdint>
export module gui.ui_tree;

import gui.core;

export namespace gui::ui {

    using NodeId = std::uint32_t;
    using NodeIndex = std::int16_t;

    constexpr NodeId kNullId = 0;
    constexpr NodeIndex kNullIndex = -1;

    constexpr NodeId fnv1a(const char* s) noexcept {
        std::uint32_t h = 2166136261u;
        while (*s) {
            h ^= static_cast<std::uint8_t>(*s++);
            h *= 16777619u;
        }
        return h;
    }

    constexpr NodeId combine(NodeId a, NodeId b) noexcept {
        return (a ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2)));
    }

    // List helper: stable ID derived from base + 1-based index.
    constexpr NodeId list_id(NodeId base, std::uint16_t index_1based) noexcept {
        return combine(base, (NodeId)index_1based);
    }

    constexpr std::int16_t list_index_from_id(NodeId base, NodeId id, std::int16_t count) noexcept {
        if (count <= 0) return -1;
        for (std::int16_t i = 0; i < count; ++i) {
            if (list_id(base, (std::uint16_t)(i + 1)) == id) return i;
        }
        return -1;
    }

    struct Node {
        NodeId id{};
        Rect rect{};
        NodeIndex parent{kNullIndex};
        NodeIndex first_child{kNullIndex};
        NodeIndex last_child{kNullIndex};
        NodeIndex next_sibling{kNullIndex};
        std::uint16_t flags{0};
    };

    constexpr std::uint16_t kFocusable = 1u << 0;

    inline void set_focusable(Node& n, bool on = true) noexcept {
        if (on) n.flags |= kFocusable;
        else n.flags = (std::uint16_t)(n.flags & ~kFocusable);
    }

    [[nodiscard]] inline bool is_focusable(const Node& n) noexcept {
        return (n.flags & kFocusable) != 0;
    }

    template<int MaxNodes, int MaxDepth>
    class Tree {
    public:
        void begin_frame() noexcept {
            count_ = 0;
            depth_ = 0;
        }

        NodeIndex begin(NodeId id) noexcept {
            if (count_ >= MaxNodes || depth_ >= MaxDepth) return kNullIndex;
            const NodeIndex idx = (NodeIndex)count_++;
            Node& n = nodes_[idx];
            n = Node{};
            n.id = id;
            n.parent = (depth_ > 0) ? stack_[depth_ - 1] : kNullIndex;
            if (n.parent != kNullIndex) {
                append_child(n.parent, idx);
            }
            stack_[depth_++] = idx;
            return idx;
        }

        void end() noexcept {
            if (depth_ > 0) --depth_;
        }

        [[nodiscard]] NodeIndex root() const noexcept {
            return (count_ > 0) ? (NodeIndex)0 : kNullIndex;
        }

        [[nodiscard]] int count() const noexcept { return count_; }

        [[nodiscard]] Node& node(NodeIndex i) noexcept { return nodes_[i]; }
        [[nodiscard]] const Node& node(NodeIndex i) const noexcept { return nodes_[i]; }

    private:
        void append_child(NodeIndex parent, NodeIndex child) noexcept {
            Node& p = nodes_[parent];
            if (p.first_child == kNullIndex) {
                p.first_child = child;
                p.last_child = child;
            } else {
                nodes_[p.last_child].next_sibling = child;
                p.last_child = child;
            }
        }

        Node nodes_[MaxNodes]{};
        NodeIndex stack_[MaxDepth]{};
        int count_{0};
        int depth_{0};
    };

    template<class T, int N>
    class StateStore {
    public:
        void clear() noexcept {
            for (auto& e : entries_) e.used = false;
        }

        [[nodiscard]] T* find(NodeId id) noexcept {
            for (auto& e : entries_) {
                if (e.used && e.id == id) return &e.state;
            }
            return nullptr;
        }

        [[nodiscard]] T* get_or_create(NodeId id, const T& init = {}) noexcept {
            if (auto* s = find(id)) return s;
            for (auto& e : entries_) {
                if (!e.used) {
                    e.used = true;
                    e.id = id;
                    e.state = init;
                    return &e.state;
                }
            }
            return nullptr;
        }

    private:
        struct Entry {
            NodeId id{};
            T state{};
            bool used{false};
        };
        Entry entries_[N]{};
    };

} // namespace gui::ui
