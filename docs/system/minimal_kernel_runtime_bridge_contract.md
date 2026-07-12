# Minimal-kernel Runtime 契约

## 文档角色

本文是 minimal-kernel 上半层 runtime 的 supporting 契约，按当前源码描述：

- scheduler 如何接收 tick、ISR defer、bootstrap、yield、sleep 和 run-loop 动作；
- lower-half ingress 与 task-side service 如何通过窄接口连接；
- mailbox 当前实际提供什么，不提供什么。

它不是 Charm Core 的统一执行模型，也不代表完整进程、RPC、syscall 或用户态 ABI 已经实现。完整 host 与 QEMU 证据入口分别见：

- [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md)
- [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)

历史设计取舍见 [`../archive/minimal-kernel-runtime-v0/README.md`](../archive/minimal-kernel-runtime-v0/README.md)。

## 当前链路

```text
arch IRQ/timer ----> RuntimeLoopPort ----> RuntimeBridge ----> Scheduler
task context ------> RuntimeThreadPort ---^

task context -> TaskRuntimeApi -> RuntimeTrapServiceFacade
             -> trap transport -> trap ingress -> arch frame

tasks/server -> RuntimeMailbox -> Scheduler events/timers
```

源码对应关系：

| 模块 | 当前职责 | 明确不负责 |
|---|---|---|
| `kernel.runtime_glue` | 无状态 scheduler glue 与 trace | 平台状态、任务 ABI |
| `kernel.runtime_bridge` | 绑定 scheduler、idle task/event、trace；导出 loop/thread ports | 异常帧翻译、服务编号 |
| `kernel.runtime_mailbox` | 固定容量 request/reply 队列和显式 timeout waiter | 同步 RPC、动态队列、跨进程传输 |
| `kernel.runtime_trap` / `kernel.runtime_trap_ingress` | trap 语义与架构帧翻译 | scheduler 管理 API |
| `kernel.runtime_service` | 将 trap transport 包装成 task-side facade | 改写 `TrapResult` |
| `kernel.task_runtime_api` | current-task self-service 命名面 | 管理任意 `TaskId` |

## Runtime Glue 与 Bridge

`kernel.runtime_glue` 直接复用现有 scheduler/thread/timer 语义，提供：

- `runtime_advance_tick(...)`
- `runtime_defer_from_isr(...)`
- `runtime_bootstrap_idle(...)`
- `runtime_bootstrap_worker(...)`
- `runtime_yield_current(...)`
- `runtime_sleep_current_until(...)`
- `runtime_run_once_or_idle(...)`

`RuntimeBridge<Scheduler, TraceBuffer>` 在这些动作上绑定一个具体 scheduler、idle task/event 和可选 trace。它不拥有 scheduler，也不把平台中断状态塞进 runtime 对象。

两个 type-erased port 刻意分开：

- `RuntimeLoopPort<Tick>` 给 lower-half / run loop 使用，包含 tick、ISR defer、bootstrap 与 run-once；
- `RuntimeThreadPort<Tick>` 给任务上下文使用，只包含 `yield_current` 与 `sleep_current_until`。

这一区分防止架构入口获得不必要的 task-side API，也防止任务代码直接依赖 scheduler 模板类型。

## Lower-half 与 Upper-half

lower-half 负责：

- exception/IRQ/FIQ/trap 入口；
- 硬件时钟与 tick 来源；
- 架构寄存器帧捕获、参数提取与返回值写回；
- idle 指令和最终上下文切换落点。

upper-half runtime 负责：

- 把 tick 推进到 scheduler；
- 将 ISR 工作转成 scheduler demand；
- bootstrap idle/worker；
- current-task yield/sleep；
- trap service 的平台无关结果语义。

二者的接缝是 port 与 frame mapping，不是“把多核或硬件伪装成普通线程”。ARMv7-A 的具体映射由 [`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md) 约束。

## Runtime Mailbox

`RuntimeMailbox<Scheduler, RequestCapacity, ReplyCapacity, ReplyWaitCapacity>` 当前是 scheduler-bound 的固定容量异步机制。默认三种容量均为 `4`，请求与回复只携带固定字段：task、label/value、sequence。

语义边界：

1. `send(...)` 只入队并向 server post receive event；成功不表示 server 已处理。
2. `receive(...)` 与 `receive_reply(...)` 是显式消费，不隐含阻塞。
3. `wait_receive_until(...)` 和 `wait_reply_until(...)` 通过 scheduler timer 注册等待；调用者必须消费对应 timeout event。
4. `reply(...)` 通过 `sequence` 建立最小因果关系，并取消目标 task 的 reply waiter；它不提供 exactly-once、重试或事务语义。
5. 队列或 waiter 满时操作返回 `false`；当前没有 backpressure 协议、动态扩容或跨地址空间复制。

因此 mailbox 可以证明异步 request/reply 与 timeout 的最小闭环，但不能被描述为成熟 RPC、IPC 或 capability transport。

## Trap Service 与 Task API

`RuntimeTrapServiceFacade<Transport>` 统一四个 task-side 动作：

- `yield_current(...)`
- `sleep_current_until(...)`
- `debug_write(...)`
- `capability_call(...)`

facade 只转发 transport，并保留其 `TrapResult`。`bind_transport(...)` 是重定向，不会重建、缓存或翻译结果语义。

`TaskRuntimeApi<Services>` 再把 current-task 视角收成：

- `yield()`
- `sleep_until(...)`
- `debug_write(...)`
- `capability_call(...)`

它与 `TaskApi` / `ThreadApi` 的边界是主语不同：前者是当前任务自服务，后者面向显式 `TaskId` 的 scheduler 管理。`TaskRuntimeApi` 不拥有 transport，不引入 errno，也不是完整 syscall ABI。syscall 编号、dispatch 与 trap 结果见：

- [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)
- [`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)
- [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)

## 当前证据

源码级 host seam 由以下示例分别覆盖：

- `runtime_tick_host`：tick 推进；
- `runtime_isr_defer_host`：ISR defer；
- `runtime_bridge_binding_host`：idle/trace 绑定；
- `runtime_loop_port_host`：lower-half loop port；
- `runtime_thread_port_host`：thread-side port；
- `runtime_run_loop_host`：run-once/idle；
- `runtime_mailbox_host`：异步 request/reply、等待与 timeout；
- `runtime_service_host`：trap service facade；
- `runtime_task_api_host`：current-task API；
- `runtime_binding_chain_host`：transport/service 的 unbind/rebind 传播；
- `runtime_minimal_host` 与 `runtime_trap_armv7a_host`：组合路径。

这些 host 示例证明 C++ 语义和绑定关系，不证明真实异常入口。QEMU lower-half 是否成立必须看 runtime evidence bundle 的实际运行结果，不能因脚本或 target 存在就宣称通过。

## 非目标与未决项

当前实现不承诺：

- 完整用户态 syscall、errno 或 libc facade；
- 同步 RPC、跨核 mailbox、对象发现或 capability namespace；
- 优先级继承、死锁处理、取消传播或服务生命周期；
- 用户指针校验、地址空间隔离、进程语义；
- 多核调度一致性或远程 runtime domain。

这些是可继续讨论的方向，但在对应代码、schema 和可重复证据出现前只能作为 exploration，不能写成现状。
