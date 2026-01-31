module;

// Optional/experimental module: task pool wrapper.
#include <optional>

export module kernel.task_pool;

import service.slot_pool;
import util.core;

export namespace kernel {
    template <typename T, util::usize Capacity>
    class TaskPool {
    public:
        using Handle = typename service::SlotPool<T, Capacity>::Handle;

        [[nodiscard]] std::optional<Handle> acquire() noexcept {
            return pool_.acquire();
        }

        void release(Handle handle) noexcept {
            pool_.release(handle);
        }

        [[nodiscard]] T& get(Handle handle) noexcept {
            return pool_.get(handle);
        }

    private:
        service::SlotPool<T, Capacity> pool_{};
    };
}
