export module power.policy;

import util.core;
import power.types;

export namespace power {
    struct Constraints {
        State min_state{State::active};
        bool allow_deep{true};
    };

    struct PolicySnapshot {
        State target{State::active};
        util::u32 active_wake_sources{0};
        util::u32 active_clock_domains{0};
    };

    struct Policy {
        virtual ~Policy() = default;
        virtual Constraints constraints() const noexcept = 0;
        virtual State choose_target(const PolicySnapshot& snapshot) const noexcept = 0;
    };
}
