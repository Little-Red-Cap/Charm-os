# RTOS ISR 语义清单（草案）

本清单定义 RTOS API 在 ISR 与任务上下文中的可用性。默认规则：ISR 只允许“标记/入队”，唤醒必须在任务上下文完成。违反规则时：Debug 断言 + 记录 trace；Release 不中断但统计递增。

## 1) 仅任务上下文可用

- `Scheduler::create`
- `Scheduler::reserve`
- `Scheduler::activate`
- `Scheduler::enter_runtime`
- `Scheduler::run_once`
- `Scheduler::yield`
- `Scheduler::sleep_ms`
- `Scheduler::block_current`
- `Scheduler::cleanup_all`
- `Scheduler::start`
- `Scheduler::setup_tick_rate`
- `Scheduler::schedule_at`（软定时器）
- `Scheduler::schedule_after`（软定时器）
- `Scheduler::cancel_timer`
- `Scheduler::lock`
- `Scheduler::unlock`
- `EventFlags::set`
- `EventFlags::wait_any`
- `EventFlags::wait_all`
- `EventFlags::poll_wake`
- `MessageQueue::send`
- `MessageQueue::recv`
- `MessageQueue::try_send`
- `MessageQueue::try_recv`
- `MessageQueue::send_batch`
- `MessageQueue::recv_batch`
- `MessageQueue::poll_wake`
- `Semaphore::post`
- `Semaphore::wait`
- `Semaphore::poll_wake`
- `Semaphore::cancel_waiters`
- `Mutex::try_lock`
- `Mutex::lock`
- `Mutex::unlock`
- `Mutex::cancel_waiters`

## 2) ISR 上下文可用

- `EventFlags::set_isr`
- `MessageQueue::try_send_isr`
- `MessageQueue::try_recv_isr`
- `Semaphore::post_isr`
- `Scheduler::schedule_at`（`TimerSlot::Kind::hard`）
- `Scheduler::schedule_after`（`TimerSlot::Kind::hard`）

## 3) ISR / 任务上下文均可用

- `Scheduler::tick`（不可阻塞）
- `EventFlags::get`
- `EventFlags::clear`
- `MessageQueue::empty`
- `MessageQueue::full`

## 4) 统一唤醒路径

ISR 触发的唤醒必须通过调度器的统一轮询入口完成（`isr_polls`）。
