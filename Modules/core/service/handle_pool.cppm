module;

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

export module service.handle_pool;

export namespace service {
    template <typename T, std::size_t N>
    class HandlePool {
    public:
        struct Handle {
            std::uint16_t index{0};
            std::uint16_t generation{0};
        };

        HandlePool() = default;

        template <typename... Args>
        std::optional<Handle> create(Args&&... args) noexcept {
            for (std::size_t i = 0; i < N; ++i) {
                if (!used_[i]) {
                    used_[i] = true;
                    (void)new (&storage_[i]) T(std::forward<Args>(args)...);
                    return Handle{static_cast<std::uint16_t>(i), generation_[i]};
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] bool valid(Handle h) const noexcept {
            if (h.index >= N) return false;
            return used_[h.index] && generation_[h.index] == h.generation;
        }

        T* get(Handle h) noexcept {
            if (!valid(h)) return nullptr;
            return reinterpret_cast<T*>(&storage_[h.index]);
        }

        void destroy(Handle h) noexcept {
            T* obj = get(h);
            if (!obj) return;
            obj->~T();
            used_[h.index] = false;
            ++generation_[h.index];
        }

        constexpr std::size_t capacity() const noexcept { return N; }

    private:
        using Slot = std::aligned_storage_t<sizeof(T), alignof(T)>;
        Slot storage_[N]{};
        bool used_[N]{};
        std::uint16_t generation_[N]{};
    };
} // namespace service
