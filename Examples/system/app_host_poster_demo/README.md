# app_host_poster_demo

This example demonstrates task-local poster facades on top of a real `AppHost` scheduler.

It focuses on three pieces:

- `charm.system.app_host`
- `kernel.poster::poster_set`
- `service.deferred_signal` bound to a task-local `demand` poster

Build:

```bash
cmake -S Examples/system/app_host_poster_demo -B Examples/system/app_host_poster_demo/build -G Ninja
cmake --build Examples/system/app_host_poster_demo/build
```
