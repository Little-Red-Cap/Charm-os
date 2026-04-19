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

它现在同时承担一层很轻的 contract smoke，明确冻结这些边界：

- 空 `delegate` 连接会返回 `invalid_arg`
- 超出 `signal` 固定容量会返回 `buffer_overflow`
- `signal.clear()` 会丢掉本地 wiring，不保留隐藏分发
- stale `connection token` 会被拒绝
- `state.set(same_value)` 不会重复通知
- `state.disconnect(token)` 之后真相仍可更新，但观察者不再被通知
- `deferred_signal` 只保留显式 `poster` 身份，不 direct call
- scheduler `poster_set` 会继续保留 `event / io_ready / demand` 三条提交语义

构建：

```bash
cmake -S Examples/service/service_signal_state_demo -B Examples/service/service_signal_state_demo/build -G Ninja
cmake --build Examples/service/service_signal_state_demo/build
```
