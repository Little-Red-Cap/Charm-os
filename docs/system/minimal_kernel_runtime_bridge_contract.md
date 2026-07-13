# Minimal-Kernel Runtime Bridge

> status: supporting
>
> 本文描述 minimal-kernel 上半层 runtime 与 lower-half/task-side 的窄接口。它不是
> Charm Core 执行模型，也不定义完整 syscall、RPC、进程或用户态 ABI。

## 链路

```text
arch IRQ/timer -> RuntimeLoopPort -> RuntimeBridge -> Scheduler
task context   -> RuntimeThreadPort -----------^

task -> TaskRuntimeApi -> RuntimeTrapServiceFacade
     -> trap transport -> trap ingress -> arch frame

task/server -> RuntimeMailbox -> Scheduler event/timer
```

| 模块 | 负责 | 不负责 |
|---|---|---|
| `kernel.runtime_glue` | scheduler 的 tick/defer/bootstrap/yield/sleep/run-once helper 与 trace | 平台状态、任务 ABI |
| `kernel.runtime_bridge` | 绑定 scheduler、idle task/event、trace，导出两个 port | 异常帧和 service number |
| `kernel.runtime_mailbox` | 固定容量 request/reply 与 timeout waiter | 同步 RPC、动态队列、跨进程复制 |
| `kernel.runtime_trap*` | trap 语义与架构帧 ingress | scheduler 管理 API |
| `kernel.runtime_service` | task-side trap transport facade | 改写 `TrapResult` |
| `kernel.task_runtime_api` | current-task self-service | 管理任意 `TaskId` |

## Glue 与 Ports

`RuntimeBridge<Scheduler, TraceBuffer>` 不拥有 scheduler。它提供：

- `advance_tick`
- `defer_from_isr`
- `bootstrap_idle / bootstrap_worker`
- `yield_current / sleep_current_until`
- `run_once_or_idle`

`RuntimeLoopPort<Tick>` 面向 lower-half/run loop，只暴露 tick、ISR defer、bootstrap 和
run-once。`RuntimeThreadPort<Tick>` 面向 task context，只暴露 yield 与 sleep。两者
type-erase scheduler 模板，但不增加线程安全、生命周期或跨核语义。

Port 的 `valid()` 要求 self 与全部 function pointer 存在。无效 port 的动作返回
`false`，`advance_tick()` 返回 `0`；调用方不得把这些返回值解释为执行成功。

Lower-half 仍负责 exception/IRQ/FIQ/trap 入口、硬件 tick、架构 frame、idle 指令与
上下文切换。ARMv7-A frame 映射见
[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)。

## RuntimeMailbox

`RuntimeMailbox<Scheduler, RequestCapacity, ReplyCapacity, ReplyWaitCapacity>` 是
scheduler-bound 固定容量异步机制：

1. `send()` 只入队并 post server event，不表示 request 已处理；
2. `receive()` / `receive_reply()` 显式消费，不阻塞；
3. `wait_receive_until()` / `wait_reply_until()` 注册 scheduler timer，调用方负责消费
   timeout event；
4. `reply()` 用 sequence 建立 request/reply 关联并取消 reply waiter；
5. queue/waiter 满返回 `false`，没有重试、backpressure、exactly-once 或事务。

Request/reply 只携带固定 task、label/value、sequence 字段。该机制可验证最小异步
闭环，不能被描述为通用 IPC/RPC 或 capability transport。

## Trap 与 Task API

`RuntimeTrapServiceFacade<Transport>` 原样转发：

- `yield_current`
- `sleep_current_until`
- `debug_write`
- `capability_call`

`bind_transport()` 只替换 transport；facade 不缓存或翻译 `TrapResult`。
`TaskRuntimeApi<Services>` 将同组操作收成 current-task 视角，不拥有 services/transport，
不引入 errno。显式 `TaskId` 管理由 `TaskApi/ThreadApi` 负责。

Syscall 与 trap 细节见：

- [`minimal_kernel_task_syscall_table_contract.md`](minimal_kernel_task_syscall_table_contract.md)
- [`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)
- [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)

## 证据边界

`Examples/kernel/runtime_*_host` 分别覆盖 tick、ISR defer、bridge/port、run loop、
mailbox、service、task API、binding chain 和组合路径。它们证明 host C++ 语义与绑定，
不证明真实异常入口。

完整入口：

- [`minimal_kernel_host_smoke_bundle_contract.md`](minimal_kernel_host_smoke_bundle_contract.md)
- [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)

QEMU lower-half 是否成立必须由当次 runtime evidence bundle 证明，不能从 target、脚本
或 example 存在推断。

## 不承诺

- 完整用户态 syscall、errno、libc 或用户指针校验；
- 地址空间隔离、进程与跨核 runtime domain；
- 同步 RPC、对象发现、取消传播、优先级继承或死锁处理；
- 多核调度一致性和远程 mailbox。

历史设计取舍见
[`../archive/minimal-kernel-runtime-v0/README.md`](../archive/minimal-kernel-runtime-v0/README.md)。
