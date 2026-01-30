module;

#include <array>
#include <cstddef>

export module kernel.init_list;

export namespace kernel {
    using InitHook = void (*)();

    template <std::size_t Max>
    class InitList {
    public:
        [[nodiscard]] bool add(InitHook hook) noexcept {
            if (hook == nullptr || count_ >= Max) {
                return false;
            }
            hooks_[count_++] = hook;
            return true;
        }

        void run() const noexcept {
            for (std::size_t i = 0; i < count_; ++i) {
                hooks_[i]();
            }
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return count_;
        }

    private:
        std::array<InitHook, Max> hooks_{};
        std::size_t count_{0};
    };
}
