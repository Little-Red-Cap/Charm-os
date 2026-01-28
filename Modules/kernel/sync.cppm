export module kernel.sync;

import <cstddef>;
import kernel.capabilities;
import util.core;

export namespace kernel {
    template <typename Caps>
    class Mutex {
    public:
        [[nodiscard]] bool try_lock() noexcept {
            auto guard = Caps::IrqGuard::enter();
            const bool acquired = !locked_;
            if (acquired) {
                locked_ = true;
            }
            Caps::IrqGuard::leave(guard);
            return acquired;
        }

        void unlock() noexcept {
            auto guard = Caps::IrqGuard::enter();
            locked_ = false;
            Caps::IrqGuard::leave(guard);
        }

    private:
        bool locked_{false};
    };

    template <typename Caps, util::usize MaxCount>
    class Semaphore {
    public:
        static_assert(MaxCount >= 1);

        [[nodiscard]] bool try_acquire() noexcept {
            auto guard = Caps::IrqGuard::enter();
            const bool ok = count_ > 0;
            if (ok) {
                --count_;
            }
            Caps::IrqGuard::leave(guard);
            return ok;
        }

        [[nodiscard]] bool release() noexcept {
            auto guard = Caps::IrqGuard::enter();
            const bool ok = count_ < MaxCount;
            if (ok) {
                ++count_;
            }
            Caps::IrqGuard::leave(guard);
            return ok;
        }

    private:
        util::usize count_{0};
    };
}
