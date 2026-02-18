module;

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

export module service.object_pool;

export namespace service {
    template <typename T, std::size_t N>
    class ObjectPool {
    public:
        constexpr ObjectPool() noexcept = default;

        template <typename... Args>
        T* create(Args&&... args) noexcept {
            for (std::size_t i = 0; i < N; ++i) {
                if (!used_[i]) {
                    used_[i] = true;
                    return new (&storage_[i]) T(std::forward<Args>(args)...);
                }
            }
            return nullptr;
        }

        void destroy(T* obj) noexcept {
            if (!obj) return;
            for (std::size_t i = 0; i < N; ++i) {
                if (reinterpret_cast<void*>(&storage_[i]) == reinterpret_cast<void*>(obj)) {
                    obj->~T();
                    used_[i] = false;
                    return;
                }
            }
        }

        constexpr std::size_t capacity() const noexcept { return N; }

    private:
        using Slot = std::aligned_storage_t<sizeof(T), alignof(T)>;
        Slot storage_[N]{};
        bool used_[N]{};
    };
} // namespace service
