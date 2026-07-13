# service_signal_state_demo

> status: `supporting`

This Host fixture checks the local primitives behind the
[`signal/state contract`](../../../docs/architecture/signal_state_contract_v0.md):

- `signal.emit()`: synchronous same-domain edge notification;
- `state<T>`: stored truth plus change notification;
- `deferred_signal.post()`: explicit delivery through a `kernel.poster` lane.

The fixture covers empty delegate rejection, fixed-capacity overflow, clear and
stale-token behavior, no duplicate state notification for an equal value,
disconnect without loss of stored truth, and preservation of the poster's
`event/io_ready/demand` lane identity.

It does not prove ISR safety, cross-task direct emission or blocking slot
behavior. Those uses remain outside the primitive contract.

Use one build directory for repeated validation:

```bash
cmake -S Examples/service/service_signal_state_demo -B <cmake-build-dir> -G Ninja
cmake --build <cmake-build-dir> -- -j1
ctest --test-dir <cmake-build-dir> --output-on-failure
```
