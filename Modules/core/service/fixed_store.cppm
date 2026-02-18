module;

#include <array>
#include <cstddef>

export module service.fixed_store;

import util.core;

export namespace service {
    template <typename Key, typename T, util::usize Capacity>
    class FixedStore {
    public:
        static_assert(Capacity >= 1);

        void clear() noexcept {
            for (auto& e : entries_) e.used = false;
        }

        [[nodiscard]] T* find(const Key& key) noexcept {
            for (auto& e : entries_) {
                if (e.used && e.key == key) return &e.value;
            }
            return nullptr;
        }

        [[nodiscard]] const T* find(const Key& key) const noexcept {
            for (const auto& e : entries_) {
                if (e.used && e.key == key) return &e.value;
            }
            return nullptr;
        }

        [[nodiscard]] T* get_or_create(const Key& key, const T& init = {}) noexcept {
            if (auto* s = find(key)) return s;
            for (auto& e : entries_) {
                if (!e.used) {
                    e.used = true;
                    e.key = key;
                    e.value = init;
                    return &e.value;
                }
            }
            return nullptr;
        }

    private:
        struct Entry {
            Key key{};
            T value{};
            bool used{false};
        };
        std::array<Entry, Capacity> entries_{};
    };
}
