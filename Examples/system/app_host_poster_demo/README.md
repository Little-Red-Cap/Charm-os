# app_host_poster_demo

> status: `supporting`

This Host fixture materializes task-local `kernel.poster::poster_set` facades
from a real `charm.system.app_host` scheduler. A `service.deferred_signal` binds
to the task-local `demand` poster, so the producer calls `post()` and the worker
runs only when the scheduler consumes that lane.

The fixture proves this AppHost integration only. Generic signal/state rules are
owned by the
[`signal/state contract`](../../../docs/architecture/signal_state_contract_v0.md).

```bash
cmake -S Examples/system/app_host_poster_demo -B <cmake-build-dir> -G Ninja
cmake --build <cmake-build-dir> -- -j1
ctest --test-dir <cmake-build-dir> --output-on-failure
```
