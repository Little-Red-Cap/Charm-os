# service_signal_state_demo

这个示例用最小代码演示 Charm signal/state v0 的四个基础点：

- `util.delegate`
- `service.signal`
- `service.state`
- `service.deferred_signal`

同时它也演示了 runtime 侧如何用：

- `kernel.poster`
- `kernel.poster::poster_set`

把“必须延迟投递”的事件接到 scheduler 的 `event / io_ready / demand` 三条提交面上。

这个示例刻意把三种语义分开，不做混用：

- `emit()`：同执行域、同步、立即广播
- `state<T>`：真相存储 + 变化通知
- `post()`：显式延迟投递

这个示例也刻意不演示下面这些越界用法：

- 不在 ISR 里直接 `emit()`
- 不用 `signal` 承担跨 task / 跨 reactor 通知
- 不在 slot 里做阻塞或重逻辑

构建：

```bash
cmake -S Examples/service/service_signal_state_demo -B Examples/service/service_signal_state_demo/build -G Ninja
cmake --build Examples/service/service_signal_state_demo/build
```
