# 最小内核 task message syscall client 契约（草案）

这份文档把 client 侧再往上抬半层，但仍然停在“异步单 pending 调用”这条线上。

它对应当前新增的：

- `Modules/system/kernel/task_message_syscall_client.cppm`

## 一句话版本

- `task_message_syscall_frame_caller` 负责 `request -> frame publish -> message send/wait -> reply completion`
- `task_message_syscall_client` 在它之上补一层 task-facing client seam：
  - 用 `sys_*` 命名发起远端 syscall
  - 用 `step(event)` 统一收口 reply / timeout
  - 保持 one-pending-call，不伪装成同步 RPC

也就是说，这一层解决的是：

“client task 怎样以更接近未来 remote syscall facade 的命名，稳定消费 message-backed syscall reply/timeout 事件？”

而不是：

“现在就发明最终形态的同步阻塞 remote syscall API”

## 当前模块形状

当前模块导出：

- `TaskMessageSyscallClientTraceKind`
- `task_message_syscall_client_trace_kind_name(...)`
- `TaskMessageSyscallClientStepResult<Caller>`
- `TaskMessageSyscallClientTraceEvent`
- `TaskMessageSyscallClientTraceBuffer<Capacity>`
- `TaskMessageSyscallClient<Caller, TraceBuffer>`
- `make_task_message_syscall_client(...)`

## 当前语义

这一层当前只收口三件事：

1. 用 `sys_yield / sys_sleep_until / sys_debug_write / sys_capability_call` 发起异步远端 syscall
2. 用 `step(event)` 把 mailbox `reply-ready / reply-timeout` 事件统一翻译成 client completion
3. 在 timeout 路径上明确表达“没有 reply”，而不是伪装成某个 trap/service 失败结果

`step(event)` 的当前约束是：

- 只有命中 reply event 或 reply-timeout event 才会尝试推进
- reply 命中时，结果来自下层 caller 的 frame writeback 恢复
- timeout 命中时，只暴露 `timeout_consumed/completed`，不额外发明 trap error 语义

## 与下一层的关系

建议把这条上半层 client 链理解成：

1. `kernel.task_message_syscall_frame_caller`
2. `kernel.task_message_syscall_client`
3. future remote syscall facade / IPC RPC

其中：

- `frame_caller` 负责 transport glue
- `syscall_client` 负责 client-side naming 与 event completion seam
- 更高层 facade 才去讨论多 outstanding、同步等待、RPC 风格 API

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_syscall_client_host`
- `Examples/kernel/runtime_task_message_syscall_frame_caller_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_syscall_client_host` 当前证明：

- client 可以用 `sys_yield / sys_sleep_until / sys_capability_call` 发起远端 syscall
- 也可以保留 raw `begin(TaskSyscallRequest, due)` 入口承接 invalid syscall 这类负向样本
- `step(event)` 可以把四次 reply 收回成稳定 `TrapResult`
- 人为抹掉 frame 后，missing reply 会继续走 timeout，而不是伪装成服务端 reply

## 当前非目标

当前这层仍然不处理：

- 真正同步阻塞的 remote syscall facade
- 多 outstanding 并发与公平性策略
- 跨地址空间 transport
- service discovery / routing policy
- user/kernel ABI

后续如果继续往上长，更健康的方向是：

- 在这层之上再长 remote syscall facade / IPC RPC

而不是把 `TaskMessageSyscallClient` 本身直接膨胀成最终 API。
