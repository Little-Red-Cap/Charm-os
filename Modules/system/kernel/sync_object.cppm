module;

#include <cstddef>

export module kernel.sync_object;

import kernel.sync_base;
import util.core;

export namespace kernel {
    template <typename Scheduler, util::usize MaxWaiters = 4>
    using SyncObject = SyncBase<Scheduler, MaxWaiters>;
}
