module;

export module platform.win.irq_guard;

export namespace platform::win {
    struct NoopIrqGuard {
        struct Token { };
        static constexpr Token enter() noexcept { return {}; }
        static constexpr void leave(Token) noexcept { }
    };
}
