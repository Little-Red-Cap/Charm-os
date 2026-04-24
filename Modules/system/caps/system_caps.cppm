module;

#include <type_traits>

export module charm.system.caps;

import charm.system.clock;
import kernel.capabilities;

export namespace charm::system {
    template <typename IrqGuardT, typename WakeupT, typename SwiTriggerT = kernel::NoopSwiTrigger>
    struct SystemCaps {
        using TimeSource = charm::system::ClockCaps::TimeSource;
        using IrqGuard = IrqGuardT;
        using Wakeup = WakeupT;
        using SwiTrigger = SwiTriggerT;
    };

    using DefaultCaps = SystemCaps<kernel::NoopIrqGuard, kernel::NoopWakeup, kernel::NoopSwiTrigger>;

    static_assert(kernel::Capabilities<DefaultCaps>);
}
