module;

#include <type_traits>

export module util.delegate;

export namespace util {
    template <class... Args>
    struct delegate {
        using stub_t = void (*)(void*, Args...) noexcept;

        void* ctx{};
        stub_t stub{};

        constexpr delegate() noexcept = default;
        constexpr delegate(void* ctx_in, stub_t stub_in) noexcept
            : ctx(ctx_in), stub(stub_in) {}

        constexpr void operator()(Args... args) const noexcept {
            if (!stub) {
                return;
            }
            stub(ctx, static_cast<Args&&>(args)...);
        }

        [[nodiscard]] constexpr bool same_as(const delegate& other) const noexcept {
            return ctx == other.ctx && stub == other.stub;
        }

        constexpr explicit operator bool() const noexcept {
            return stub != nullptr;
        }

        template <auto Method, class T>
        [[nodiscard]] static constexpr delegate bind(T& obj) noexcept
            requires(std::is_member_function_pointer_v<decltype(Method)> &&
                     std::is_nothrow_invocable_r_v<void, decltype(Method), T&, Args...>)
        {
            return delegate{
                &obj,
                [](void* p, Args... args) noexcept {
                    (static_cast<T*>(p)->*Method)(static_cast<Args&&>(args)...);
                },
            };
        }

        template <auto Fn>
        [[nodiscard]] static constexpr delegate bind() noexcept
            requires(std::is_pointer_v<decltype(Fn)> &&
                     std::is_function_v<std::remove_pointer_t<decltype(Fn)>> &&
                     std::is_nothrow_invocable_r_v<void, decltype(Fn), Args...>)
        {
            return delegate{
                nullptr,
                [](void*, Args... args) noexcept {
                    Fn(static_cast<Args&&>(args)...);
                },
            };
        }
    };
}
