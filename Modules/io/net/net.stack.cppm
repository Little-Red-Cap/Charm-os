export module net.stack;

import net.socket;

export namespace net {
    class Stack {
    public:
        constexpr Stack() noexcept = default;

        constexpr explicit Stack(SocketProviderRef provider) noexcept
            : provider_(provider) {}

        template <SocketProvider T>
        constexpr explicit Stack(T& provider) noexcept
            : provider_(make_socket_provider_ref(provider)) {}

        [[nodiscard]] constexpr bool valid() const noexcept {
            return provider_.valid();
        }

        [[nodiscard]] constexpr SocketProviderRef provider() const noexcept {
            return provider_;
        }

        constexpr void bind(SocketProviderRef provider) noexcept {
            provider_ = provider;
        }

        template <SocketProvider T>
        constexpr void bind(T& provider) noexcept {
            provider_ = make_socket_provider_ref(provider);
        }

    private:
        SocketProviderRef provider_{};
    };
}
