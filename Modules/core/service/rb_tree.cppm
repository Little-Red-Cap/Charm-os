module;

#include <array>
#include <cstddef>
#include <functional>

export module service.rb_tree;

import util.core;

export namespace service {
    enum class RbColor : util::u8 { red = 0, black = 1 };

    template <typename K, typename V, util::usize Capacity, typename Less = std::less<K>>
    class RbTree {
    public:
        static constexpr util::usize npos = static_cast<util::usize>(-1);

        RbTree() {
            for (util::usize i = 0; i < Capacity; ++i) {
                nodes_[i].used = false;
                nodes_[i].next_free = (i + 1 < Capacity) ? (i + 1) : npos;
            }
            free_head_ = Capacity > 0 ? 0 : npos;
        }

        bool insert(const K& key, const V& value) noexcept {
            util::usize y = npos;
            util::usize x = root_;
            while (x != npos) {
                y = x;
                if (less_(key, nodes_[x].key)) {
                    x = nodes_[x].left;
                } else if (less_(nodes_[x].key, key)) {
                    x = nodes_[x].right;
                } else {
                    nodes_[x].value = value;
                    return true;
                }
            }
            const auto z = alloc_node(key, value);
            if (z == npos) return false;
            nodes_[z].parent = y;
            if (y == npos) {
                root_ = z;
            } else if (less_(key, nodes_[y].key)) {
                nodes_[y].left = z;
            } else {
                nodes_[y].right = z;
            }
            insert_fixup(z);
            ++size_;
            return true;
        }

        [[nodiscard]] V* find(const K& key) noexcept {
            auto idx = find_node(key);
            return idx == npos ? nullptr : &nodes_[idx].value;
        }

        [[nodiscard]] const V* find(const K& key) const noexcept {
            auto idx = find_node(key);
            return idx == npos ? nullptr : &nodes_[idx].value;
        }

        bool erase(const K& key) noexcept {
            const auto z = find_node(key);
            if (z == npos) return false;
            util::usize y = z;
            auto y_color = nodes_[y].color;
            util::usize x = npos;
            util::usize x_parent = npos;
            if (nodes_[z].left == npos) {
                x = nodes_[z].right;
                x_parent = nodes_[z].parent;
                transplant(z, nodes_[z].right);
            } else if (nodes_[z].right == npos) {
                x = nodes_[z].left;
                x_parent = nodes_[z].parent;
                transplant(z, nodes_[z].left);
            } else {
                y = minimum(nodes_[z].right);
                y_color = nodes_[y].color;
                x = nodes_[y].right;
                x_parent = nodes_[y].parent;
                if (nodes_[y].parent == z) {
                    x_parent = y;
                } else {
                    transplant(y, nodes_[y].right);
                    nodes_[y].right = nodes_[z].right;
                    if (nodes_[y].right != npos) {
                        nodes_[nodes_[y].right].parent = y;
                    }
                }
                transplant(z, y);
                nodes_[y].left = nodes_[z].left;
                if (nodes_[y].left != npos) {
                    nodes_[nodes_[y].left].parent = y;
                }
                nodes_[y].color = nodes_[z].color;
            }
            free_node(z);
            if (size_ > 0) --size_;
            if (y_color == RbColor::black) {
                delete_fixup(x, x_parent);
            }
            return true;
        }

        [[nodiscard]] util::usize size() const noexcept { return size_; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

        template <typename Fn>
        void inorder(Fn&& fn) const noexcept {
            inorder_impl(root_, fn);
        }

    private:
        struct Node {
            K key{};
            V value{};
            util::usize parent{npos};
            util::usize left{npos};
            util::usize right{npos};
            util::usize next_free{npos};
            RbColor color{RbColor::red};
            bool used{false};
        };

        util::usize alloc_node(const K& key, const V& value) noexcept {
            if (free_head_ == npos) return npos;
            const auto idx = free_head_;
            free_head_ = nodes_[idx].next_free;
            nodes_[idx].key = key;
            nodes_[idx].value = value;
            nodes_[idx].left = npos;
            nodes_[idx].right = npos;
            nodes_[idx].parent = npos;
            nodes_[idx].color = RbColor::red;
            nodes_[idx].used = true;
            return idx;
        }

        void free_node(util::usize idx) noexcept {
            nodes_[idx].used = false;
            nodes_[idx].next_free = free_head_;
            free_head_ = idx;
        }

        util::usize find_node(const K& key) const noexcept {
            util::usize x = root_;
            while (x != npos) {
                if (less_(key, nodes_[x].key)) {
                    x = nodes_[x].left;
                } else if (less_(nodes_[x].key, key)) {
                    x = nodes_[x].right;
                } else {
                    return x;
                }
            }
            return npos;
        }

        util::usize minimum(util::usize node) const noexcept {
            util::usize x = node;
            while (x != npos && nodes_[x].left != npos) {
                x = nodes_[x].left;
            }
            return x;
        }

        void transplant(util::usize u, util::usize v) noexcept {
            const auto p = nodes_[u].parent;
            if (p == npos) {
                root_ = v;
            } else if (u == nodes_[p].left) {
                nodes_[p].left = v;
            } else {
                nodes_[p].right = v;
            }
            if (v != npos) {
                nodes_[v].parent = p;
            }
        }

        RbColor color_of(util::usize idx) const noexcept {
            if (idx == npos) return RbColor::black;
            return nodes_[idx].color;
        }

        void set_color(util::usize idx, RbColor color) noexcept {
            if (idx == npos) return;
            nodes_[idx].color = color;
        }

        void rotate_left(util::usize x) noexcept {
            auto y = nodes_[x].right;
            nodes_[x].right = nodes_[y].left;
            if (nodes_[y].left != npos) {
                nodes_[nodes_[y].left].parent = x;
            }
            nodes_[y].parent = nodes_[x].parent;
            if (nodes_[x].parent == npos) {
                root_ = y;
            } else if (x == nodes_[nodes_[x].parent].left) {
                nodes_[nodes_[x].parent].left = y;
            } else {
                nodes_[nodes_[x].parent].right = y;
            }
            nodes_[y].left = x;
            nodes_[x].parent = y;
        }

        void rotate_right(util::usize y) noexcept {
            auto x = nodes_[y].left;
            nodes_[y].left = nodes_[x].right;
            if (nodes_[x].right != npos) {
                nodes_[nodes_[x].right].parent = y;
            }
            nodes_[x].parent = nodes_[y].parent;
            if (nodes_[y].parent == npos) {
                root_ = x;
            } else if (y == nodes_[nodes_[y].parent].right) {
                nodes_[nodes_[y].parent].right = x;
            } else {
                nodes_[nodes_[y].parent].left = x;
            }
            nodes_[x].right = y;
            nodes_[y].parent = x;
        }

        void insert_fixup(util::usize z) noexcept {
            while (z != root_ && nodes_[nodes_[z].parent].color == RbColor::red) {
                const auto parent = nodes_[z].parent;
                const auto grand = nodes_[parent].parent;
                if (parent == nodes_[grand].left) {
                    const auto y = nodes_[grand].right;
                    if (y != npos && nodes_[y].color == RbColor::red) {
                        nodes_[parent].color = RbColor::black;
                        nodes_[y].color = RbColor::black;
                        nodes_[grand].color = RbColor::red;
                        z = grand;
                    } else {
                        if (z == nodes_[parent].right) {
                            z = parent;
                            rotate_left(z);
                        }
                        nodes_[nodes_[z].parent].color = RbColor::black;
                        nodes_[grand].color = RbColor::red;
                        rotate_right(grand);
                    }
                } else {
                    const auto y = nodes_[grand].left;
                    if (y != npos && nodes_[y].color == RbColor::red) {
                        nodes_[parent].color = RbColor::black;
                        nodes_[y].color = RbColor::black;
                        nodes_[grand].color = RbColor::red;
                        z = grand;
                    } else {
                        if (z == nodes_[parent].left) {
                            z = parent;
                            rotate_right(z);
                        }
                        nodes_[nodes_[z].parent].color = RbColor::black;
                        nodes_[grand].color = RbColor::red;
                        rotate_left(grand);
                    }
                }
            }
            nodes_[root_].color = RbColor::black;
        }

        util::usize left_of(util::usize idx) const noexcept {
            return idx == npos ? npos : nodes_[idx].left;
        }

        util::usize right_of(util::usize idx) const noexcept {
            return idx == npos ? npos : nodes_[idx].right;
        }

        void delete_fixup(util::usize x, util::usize parent) noexcept {
            util::usize cur = x;
            util::usize cur_parent = parent;
            while (cur != root_ && color_of(cur) == RbColor::black) {
                if (cur_parent == npos) break;
                if (cur == nodes_[cur_parent].left) {
                    util::usize w = nodes_[cur_parent].right;
                    if (color_of(w) == RbColor::red) {
                        set_color(w, RbColor::black);
                        set_color(cur_parent, RbColor::red);
                        rotate_left(cur_parent);
                        w = nodes_[cur_parent].right;
                    }
                    if (color_of(left_of(w)) == RbColor::black
                        && color_of(right_of(w)) == RbColor::black) {
                        set_color(w, RbColor::red);
                        cur = cur_parent;
                        cur_parent = nodes_[cur].parent;
                    } else {
                        if (color_of(right_of(w)) == RbColor::black) {
                            set_color(left_of(w), RbColor::black);
                            set_color(w, RbColor::red);
                            rotate_right(w);
                            w = nodes_[cur_parent].right;
                        }
                        set_color(w, color_of(cur_parent));
                        set_color(cur_parent, RbColor::black);
                        set_color(right_of(w), RbColor::black);
                        rotate_left(cur_parent);
                        cur = root_;
                        break;
                    }
                } else {
                    util::usize w = nodes_[cur_parent].left;
                    if (color_of(w) == RbColor::red) {
                        set_color(w, RbColor::black);
                        set_color(cur_parent, RbColor::red);
                        rotate_right(cur_parent);
                        w = nodes_[cur_parent].left;
                    }
                    if (color_of(right_of(w)) == RbColor::black
                        && color_of(left_of(w)) == RbColor::black) {
                        set_color(w, RbColor::red);
                        cur = cur_parent;
                        cur_parent = nodes_[cur].parent;
                    } else {
                        if (color_of(left_of(w)) == RbColor::black) {
                            set_color(right_of(w), RbColor::black);
                            set_color(w, RbColor::red);
                            rotate_left(w);
                            w = nodes_[cur_parent].left;
                        }
                        set_color(w, color_of(cur_parent));
                        set_color(cur_parent, RbColor::black);
                        set_color(left_of(w), RbColor::black);
                        rotate_right(cur_parent);
                        cur = root_;
                        break;
                    }
                }
            }
            set_color(cur, RbColor::black);
        }

        template <typename Fn>
        void inorder_impl(util::usize node, Fn&& fn) const noexcept {
            if (node == npos) return;
            inorder_impl(nodes_[node].left, fn);
            fn(nodes_[node].key, nodes_[node].value);
            inorder_impl(nodes_[node].right, fn);
        }

        Less less_{};
        std::array<Node, Capacity> nodes_{};
        util::usize root_{npos};
        util::usize free_head_{npos};
        util::usize size_{0};
    };
}
