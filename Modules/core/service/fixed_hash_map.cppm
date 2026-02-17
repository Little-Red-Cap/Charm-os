module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

export module service.fixed_hash_map;

import util.core;

export namespace service {
    enum class SlotState : util::u8 { empty = 0, filled, tombstone };

    template <typename K>
    struct DefaultHash {
        constexpr util::u64 operator()(const K& key) const noexcept {
            if constexpr (std::is_integral_v<K>) {
                return static_cast<util::u64>(key) * 11400714819323198485ull;
            } else {
                return 0;
            }
        }
    };

    template <typename K, typename V, util::usize Capacity,
              typename Hash = DefaultHash<K>,
              typename Eq = std::equal_to<K>>
    class FixedHashMap {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool insert(const K& key, const V& value) noexcept {
            util::usize idx = probe_insert(key);
            if (idx == npos) return false;
            entries_[idx].key = key;
            entries_[idx].value = value;
            if (states_[idx] != SlotState::filled) {
                ++size_;
            }
            states_[idx] = SlotState::filled;
            return true;
        }

        [[nodiscard]] V* find(const K& key) noexcept {
            const auto idx = probe_find(key);
            if (idx == npos) return nullptr;
            return &entries_[idx].value;
        }

        [[nodiscard]] const V* find(const K& key) const noexcept {
            const auto idx = probe_find(key);
            if (idx == npos) return nullptr;
            return &entries_[idx].value;
        }

        [[nodiscard]] bool erase(const K& key) noexcept {
            const auto idx = probe_find(key);
            if (idx == npos) return false;
            states_[idx] = SlotState::tombstone;
            if (size_ > 0) --size_;
            return true;
        }

        [[nodiscard]] util::usize size() const noexcept { return size_; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    private:
        struct Entry {
            K key{};
            V value{};
        };

        static constexpr util::usize npos = static_cast<util::usize>(-1);

        util::usize hash_index(const K& key) const noexcept {
            return static_cast<util::usize>(hash_(key) % Capacity);
        }

        util::usize probe_find(const K& key) const noexcept {
            const auto start = hash_index(key);
            for (util::usize i = 0; i < Capacity; ++i) {
                const auto idx = (start + i) % Capacity;
                if (states_[idx] == SlotState::empty) return npos;
                if (states_[idx] == SlotState::filled && eq_(entries_[idx].key, key)) {
                    return idx;
                }
            }
            return npos;
        }

        util::usize probe_insert(const K& key) const noexcept {
            const auto start = hash_index(key);
            util::usize tomb = npos;
            for (util::usize i = 0; i < Capacity; ++i) {
                const auto idx = (start + i) % Capacity;
                if (states_[idx] == SlotState::filled && eq_(entries_[idx].key, key)) {
                    return idx;
                }
                if (states_[idx] == SlotState::tombstone && tomb == npos) {
                    tomb = idx;
                }
                if (states_[idx] == SlotState::empty) {
                    return tomb == npos ? idx : tomb;
                }
            }
            return tomb;
        }

        Hash hash_{};
        Eq eq_{};
        std::array<Entry, Capacity> entries_{};
        std::array<SlotState, Capacity> states_{};
        util::usize size_{0};
    };
}
