# io.reactor 契约

## 文档状态

- `status`: `canonical`
- `scope`: subscription、event queue、wake 与 drain
- `source`: [`io.reactor.cppm`](../../Modules/io/reactor/io.reactor.cppm)

## Subscription

- `subscribe()` 要求非空 callback 和非零 event mask；非法输入返回 `invalid_arg`。
- subscription 固定容量为 32；耗尽时返回 `busy`。
- `unsubscribe()` 接受无效或已删除 token，并保持 no-op。
- subscribe/unsubscribe 和 callback 派发由 task context 串行调用；类型本身不提供锁。

## Notify 与 Drain

- `notify()` 只入队或合并同一 Channel 的待处理事件，不直接调用 callback。
- pending queue 固定容量为 64。溢出会增加 `dropped_events()`、置位 `overflowed()`，并安排一次
  `Event::error` 派发。
- `drain(budget)` 在调用方 context 派发事件；`budget` 按 pending event 计数，`0` 表示无限制。
- callback 不得阻塞；需要继续处理时保留状态，等待后续事件。

## Queue Policy 与 Wake

默认 policy 是 `irq_safe`。该 policy 下，`notify()` 入队、合并或记录 overflow 后会尝试调用 waker；
一次未完成 wake 只触发一次回调。`set_waker()` 在已有 pending event 时也会触发 wake。

`sched_safe` 与 `no_lock` 不在 `notify()` 中自动 wake。当前类型只记录 policy，不实现临界区、原子队列
或 scheduler handoff；调用方必须为所选 policy 提供对应同步与驱动方式。
