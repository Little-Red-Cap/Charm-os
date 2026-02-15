module;

#include <array>
#include <cstddef>

export module service.lru_cache;

import util.core;
import service.fixed_hash_map;

export namespace service {
    template <typename K, typename V, util::usize Capacity>
    class LruCache {
    public:
        static constexpr util::usize npos = static_cast<util::usize>(-1);
        static constexpr util::usize map_capacity = Capacity * 2 + 1;

        LruCache() {
            for (util::usize i = 0; i < Capacity; ++i) {
                nodes_[i].used = false;
            }
        }

        bool get(const K& key, V& out) noexcept {
            auto* idx_ptr = index_.find(key);
            if (!idx_ptr) return false;
            const auto idx = *idx_ptr;
            out = nodes_[idx].value;
            touch(idx);
            return true;
        }

        bool put(const K& key, const V& value) noexcept {
            if (auto* idx_ptr = index_.find(key)) {
                const auto idx = *idx_ptr;
                nodes_[idx].value = value;
                touch(idx);
                return true;
            }
            util::usize slot = free_slot();
            if (slot == npos) {
                slot = evict();
            }
            nodes_[slot].key = key;
            nodes_[slot].value = value;
            nodes_[slot].used = true;
            link_front(slot);
            if (!index_.insert(key, slot)) {
                unlink(slot);
                nodes_[slot].used = false;
                return false;
            }
            ++size_;
            return true;
        }

        [[nodiscard]] util::usize size() const noexcept { return size_; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    private:
        struct Node {
            K key{};
            V value{};
            util::usize prev{npos};
            util::usize next{npos};
            bool used{false};
        };

        util::usize find(const K& key) const noexcept {
            for (util::usize i = 0; i < Capacity; ++i) {
                if (nodes_[i].used && nodes_[i].key == key) return i;
            }
            return npos;
        }

        util::usize free_slot() const noexcept {
            for (util::usize i = 0; i < Capacity; ++i) {
                if (!nodes_[i].used) return i;
            }
            return npos;
        }

        util::usize evict() noexcept {
            const auto idx = tail_;
            if (idx == npos) return 0;
            unlink(idx);
            (void)index_.erase(nodes_[idx].key);
            nodes_[idx].used = false;
            if (size_ > 0) --size_;
            return idx;
        }

        void touch(util::usize idx) noexcept {
            if (idx == head_) return;
            unlink(idx);
            link_front(idx);
        }

        void link_front(util::usize idx) noexcept {
            nodes_[idx].prev = npos;
            nodes_[idx].next = head_;
            if (head_ != npos) nodes_[head_].prev = idx;
            head_ = idx;
            if (tail_ == npos) tail_ = idx;
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
        FixedHashMap<K, util::usize, map_capacity> index_{};
        util::usize head_{npos};
        util::usize tail_{npos};
        util::usize size_{0};
    };
}
