# service_signal_state_demo

This example demonstrates the v0 Foundation primitives for Charm signal/state:

- `util.delegate`
- `service.signal`
- `service.state`
- `service.deferred_signal`
- `kernel.poster` as the runtime-side bridge into scheduler submit semantics
- `kernel.poster::poster_set` for task-local event/io_ready/demand lanes

It intentionally keeps the three execution semantics separate:

- direct `emit()` for same-domain synchronous broadcast
- `state<T>` for truth storage plus change notification
- explicit `post()` for deferred delivery
- explicit `event / io_ready / demand` posters when deferred delivery enters the kernel scheduler

Build:

```bash
cmake -S Examples/service/service_signal_state_demo -B Examples/service/service_signal_state_demo/build -G Ninja
cmake --build Examples/service/service_signal_state_demo/build
```
