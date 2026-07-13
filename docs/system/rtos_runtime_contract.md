# RTOS Runtime Contract

> status: `supporting`
>
> 本文描述 [`system_rtos.cppm`](../../Modules/system/rtos/system_rtos.cppm) 已实现的 context、lifecycle、
> wait 和 observability 边界，不定义 Charm Core 或跨平台 RTOS ABI。

## Core 与 Port

RTOS core 持有 scheduler state、task/timer lifecycle、同步原语、timeout、wait cancellation 和 trace。
Port 提供 critical section、context switch、tick source、first-task start 等机器入口。Board driver 不进入
core；真实 IRQ、timer 和 context switch 行为必须由对应 port/target 验证。

## Lifecycle

Scheduler 区分 startup 与 runtime。Runtime task/timer creation 默认关闭，可通过显式配置或 setter
开启；因此“runtime 禁止创建”是默认 policy，不是不可覆盖的编译期规则。

## Task 与 ISR Context

`require_task_context()` 和 `require_isr_context()` 在 context 错误时累计 violation、写 trace，并触发
debug assertion。Release 行为取决于 `debug_assert` 配置，调用方不能把 assertion 当作运行时恢复。

当前显式 ISR API 包括 `Semaphore::post_isr()`、`EventFlags::set_isr()`、
`MessageQueue::try_send_isr()` 和 `try_recv_isr()`。普通 task API、scheduler mutation 和 trace dump
使用 task-context guard。`Scheduler::tick()` 处理 hard timer；ISR 产生的 deferred wake 由 task context
中的 `poll_isr_wake()` 消费。

## Wait 与 Observability

- 阻塞等待可通过 cleanup/cancel 路径得到 `WaitResult::cancelled`；
- trace ring、trace mask、scheduler statistics 和 task/ISR violation counter 是当前诊断面；
- trace 被关闭或容量为零时不会产生完整事件历史；
- 统计和 trace 只能证明对应路径被观察，不能证明调度公平性或实时上限。

## 未证明

- priority inheritance 或稳定的 priority-inversion detection；
- 所有 API 的 ISR-safe/reentrant/lock-free 性质；
- Cortex-M、RISC-V、Cortex-A 间相同的 port 行为；
- SMP、用户态隔离、抢占延迟和产品级 cleanup policy；
- 公共、长期冻结的 RTOS ABI。

具体 API 清单、guard 调用和默认配置以源码及相应 QEMU/Host smoke 为准，不在文档中维护平行台账。
