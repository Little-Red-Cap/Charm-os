# app_host_poster_demo

这个示例演示如何在真实 `AppHost` scheduler 上 materialize 出 task-local poster facade。

它关注三件事：

- `charm.system.app_host`
- `kernel.poster::poster_set`
- 绑定到 task-local `demand` poster 的 `service.deferred_signal`

这个示例的重点不是“signal 广播”，而是：

- 当前上下文不要直接调用 worker
- 先显式 `post()`
- 再由 scheduler 在正确 task context 消费

也就是说，它对应的是 Charm 版规则里的这句：

> 能当前上下文安全做完的，用 `emit()`；不能当前上下文安全做完的，用 `post()`。

构建：

```bash
cmake -S Examples/system/app_host_poster_demo -B Examples/system/app_host_poster_demo/build -G Ninja
cmake --build Examples/system/app_host_poster_demo/build
```
