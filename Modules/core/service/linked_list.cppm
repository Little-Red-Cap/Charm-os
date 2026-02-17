module;

#include <array>
#include <cstddef>

export module service.linked_list;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class LinkedList {
    public:
        static constexpr util::usize npos = static_cast<util::usize>(-1);

        LinkedList() {
            for (util::usize i = 0; i < Capacity; ++i) {
                nodes_[i].next_free = (i + 1 < Capacity) ? (i + 1) : npos;
            }
            free_head_ = Capacity > 0 ? 0 : npos;
        }

        util::usize push_front(const T& value) noexcept {
            const auto idx = alloc_node(value);
            if (idx == npos) return npos;
            link_front(idx);
            return idx;
        }

        util::usize push_back(const T& value) noexcept {
            const auto idx = alloc_node(value);
            if (idx == npos) return npos;
            link_back(idx);
            return idx;
        }

        bool pop_front(T* out = nullptr) noexcept {
            if (head_ == npos) return false;
            const auto idx = head_;
            if (out) *out = nodes_[idx].value;
            unlink(idx);
            free_node(idx);
            return true;
        }

        bool pop_back(T* out = nullptr) noexcept {
            if (tail_ == npos) return false;
            const auto idx = tail_;
            if (out) *out = nodes_[idx].value;
            unlink(idx);
            free_node(idx);
            return true;
        }

        bool erase(util::usize idx, T* out = nullptr) noexcept {
            if (idx >= Capacity || !nodes_[idx].used) return false;
            if (out) *out = nodes_[idx].value;
            unlink(idx);
            free_node(idx);
            return true;
        }

        [[nodiscard]] util::usize head() const noexcept { return head_; }
        [[nodiscard]] util::usize tail() const noexcept { return tail_; }
        [[nodiscard]] util::usize next(util::usize idx) const noexcept {
            return idx < Capacity ? nodes_[idx].next : npos;
        }

        [[nodiscard]] T& value(util::usize idx) noexcept { return nodes_[idx].value; }
        [[nodiscard]] const T& value(util::usize idx) const noexcept { return nodes_[idx].value; }

        [[nodiscard]] util::usize size() const noexcept { return size_; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    private:
        struct Node {
            T value{};
            util::usize prev{npos};
            util::usize next{npos};
            util::usize next_free{npos};
            bool used{false};
        };

        util::usize alloc_node(const T& value) noexcept {
            if (free_head_ == npos) return npos;
            const auto idx = free_head_;
            free_head_ = nodes_[idx].next_free;
            nodes_[idx].value = value;
            nodes_[idx].used = true;
            nodes_[idx].prev = npos;
            nodes_[idx].next = npos;
            ++size_;
            return idx;
        }

        void free_node(util::usize idx) noexcept {
            nodes_[idx].used = false;
            nodes_[idx].next_free = free_head_;
            free_head_ = idx;
            if (size_ > 0) --size_;
        }

        void link_front(util::usize idx) noexcept {
            nodes_[idx].prev = npos;
            nodes_[idx].next = head_;
            if (head_ != npos) nodes_[head_].prev = idx;
            head_ = idx;
            if (tail_ == npos) tail_ = idx;
        }

        void link_back(util::usize idx) noexcept {
            nodes_[idx].next = npos;
            nodes_[idx].prev = tail_;
            if (tail_ != npos) nodes_[tail_].next = idx;
            tail_ = idx;
            if (head_ == npos) head_ = idx;
        }

        void unlink(util::usize idx) noexcept {
            const auto prev = nodes_[idx].prev;
            const auto next = nodes_[idx].next;
            if (prev != npos) nodes_[prev].next = next;
            if (next != npos) nodes_[next].prev = prev;
            if (head_ == idx) head_ = next;
            if (tail_ == idx) tail_ = prev;
            nodes_[idx].prev = npos;
            nodes_[idx].next = npos;
        }

        std::array<Node, Capacity> nodes_{};
        util::usize head_{npos};
        util::usize tail_{npos};
        util::usize free_head_{npos};
        util::usize size_{0};
    };
}
