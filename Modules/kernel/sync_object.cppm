module;

#include <cstddef>

export module kernel.sync_object;

import kernel.sync_base;

export namespace kernel {
    template <typename Scheduler>
    using SyncObject = SyncBase<Scheduler>;
}
