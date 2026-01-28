module;

export module platform.win.wakeup;

export namespace platform::win {
    struct NoopWakeup {
        static constexpr void signal() noexcept { }
    };
}
