# M1 Sync/IPC Behavior Spec (Draft)

## 1. SyncUnified Semantics

### States
- **wait(token)**: task waits on token until notified/canceled/timeout
- **notify_one/result**: wakes one waiter with result
- **notify_all/result**: wakes all waiters with result
- **cancel(token)**: cancels a specific waiter
- **wait_timeout(token, due)**: waits until notified or timeout event fires

### Event Delivery
- All wakeups are delivered as `EventId::sync` with payload = `WaitResult`.
- `WaitResult` values: ok / timeout / canceled

### Deterministic Rules
1) If `notify_one` arrives before timeout event fires:
   - waiter is removed
   - timeout token is canceled
   - waiter receives `WaitResult::ok`
2) If timeout fires first:
   - waiter is removed
   - waiter receives `WaitResult::timeout`
3) If `cancel(token)` occurs:
   - waiter is removed
   - timeout token is canceled
   - waiter receives `WaitResult::canceled`
4) `notify_all` behaves like `notify_one` for each waiter

### Error/Return Semantics
- `wait(...)` returns `false` if wait list is full
- `wait_timeout(...)` returns `false` if wait list is full or timeout schedule fails
- `notify_*` return `false` if no waiters
- `cancel(...)` returns `false` if token not found

### Token & Waiter Assumptions
- `SyncUnified` 依赖 `WaitToken` 的唯一性（至少在同一 Sync 实例内唯一）。
- `erase(token)` 只移除第一个匹配项；若允许重复 token 会导致残留。

---

## 2. IPC Semantics (Minimal)

### SemaphoreIpc
- `wait(task)` registers wait; later `post()` wakes one task with `WaitResult::ok`

### QueueIpc
- `send(task, msg)` posts message event to task

### TriggerIpc
- `trigger(task)` posts sync event with `WaitResult::ok`

---

## 3. SyncBase Semantics (Legacy/Minimal)
- `SyncBase::pend()` 只做等待登记，不带 token 去重。
- 同一 task 重复 pend 不会被阻止（上层需保证一致性）。

---

## 3. Test Checklist (manual/demo)
- Wait + notify_one -> ok
- Wait + timeout -> timeout
- Wait + cancel -> canceled
- notify_all -> all ok
- WaitTimeout + notify before due -> ok

