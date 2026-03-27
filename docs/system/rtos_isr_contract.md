# RTOS ISR 语义清单（草案）

本清单定义 RTOS API 在 ISR 与任务上下文中的可用性。
默认规则：ISR 只允许轻量“标记/入队”，唤醒必须在任务上下文完成。

## 1) 仅任务上下文可用

- `Scheduler::run_once`
- `Scheduler::sleep_ms`
- `Scheduler::yield`
- `Scheduler::block_current`
- `EventFlags::set`
- `EventFlags::wait_any`
- `EventFlags::wait_all`
- `EventFlags::poll_wake`
- `MessageQueue::send`
- `MessageQueue::recv`
- `MessageQueue::try_send`
- `MessageQueue::try_recv`
- `MessageQueue::poll_wake`

## 2) 仅 ISR 上下文可用

- `EventFlags::set_isr`
- `MessageQueue::try_send_isr`
- `MessageQueue::try_recv_isr`

## 3) ISR/任务上下文均可用

- `EventFlags::get`
- `EventFlags::clear`
- `MessageQueue::empty`
- `MessageQueue::full`

## 4) 违反规则的行为

Debug：
- 触发断言并记录计数。

Release：
- 不中断，但统计计数增加。
