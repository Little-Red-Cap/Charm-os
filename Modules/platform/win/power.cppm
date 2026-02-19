export module platform.win.power;

import power.types;

export namespace platform::win {
    struct NoopPower {
        static bool enter(power::State) noexcept { return true; }
        static void exit(power::State) noexcept { }
    };
}
