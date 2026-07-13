# M1 Sync/IPC Behavior Spec (Draft)

> `status`: `archived`。行为需以当前 sync/wait/IPC module 源码为准。

## SyncUnified 语义

- `wait(token)`：等待 notify、cancel 或 timeout；
- `notify_one/all(result)`：以 `EventId::sync` 和 `WaitResult` 唤醒 waiter；
- `cancel(token)`：移除 waiter、取消 timeout token 并发送 `canceled`；
- `wait_timeout(token, due)`：notify 与 timeout 中先发生者决定结果。

原确定性规则：

1. notify 先发生时移除 waiter、取消 timeout，结果为 `ok`；
2. timeout 先发生时移除 waiter，结果为 `timeout`；
3. cancel 移除 waiter、取消 timeout，结果为 `canceled`；
4. `notify_all` 对每个 waiter 执行相同处理。

`wait*` 在 wait list 满或 timeout schedule 失败时返回 `false`；`notify_*` 在无 waiter 时返回
`false`；`cancel` 找不到 token 时返回 `false`。timeout token 与 waiter 一起保存在 WaitSet entry。
同一 Sync 实例内 token 必须唯一，否则 `erase(token)` 只移除首个匹配项。

## 早期 IPC 约定

- `SemaphoreIpc::post()` 唤醒一个 waiter；
- `QueueIpc::send()` 向 task 投递 message event；
- `TriggerIpc::trigger()` 投递 `WaitResult::ok`；
- legacy `SyncBase::pend()` 不做 token 去重。

原手工检查包括 notify、timeout、cancel、notify_all 及 notify-before-timeout。它们不是当前自动回归
清单，具体行为和覆盖必须重新从源码与测试核对。
