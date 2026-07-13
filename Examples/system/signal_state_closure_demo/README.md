# signal_state_closure_demo

> status: `supporting`

This fixture composes four already-separate mechanisms:

```text
signal edge -> state update -> deferred poster -> AppHost worker
                     \\-> init.connection observation
```

It verifies that local `emit()`/`state.set()` complete synchronously, a changed
state posts work to the task-local `demand` lane, the worker is not called before
`dispatch_batch()`, and direct/deferred wiring remains visible in the
materialized connection graph.

It does not generate runtime binding from a system compiler and does not merge
signal, state, connection and poster into one abstraction. Their ownership is
defined by the
[`signal/state contract`](../../../docs/architecture/signal_state_contract_v0.md).

```bash
cmake -S Examples/system/signal_state_closure_demo -B <cmake-build-dir> -G Ninja
cmake --build <cmake-build-dir> -- -j1
ctest --test-dir <cmake-build-dir> --output-on-failure
```
