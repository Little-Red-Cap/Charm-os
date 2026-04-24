# 最小内核 task message syscall pump 契约（草案）

这份文档把 `task_message_syscall_client` 再往上抬半层，但仍然不进入“同步 RPC facade”。

它对应当前新增的：

- `Modules/system/kernel/task_message_syscall_pump.cppm`

## 一句话版本

- `task_message_syscall_client` 负责 one-pending-call 的异步远端 syscall
- `task_message_syscall_pump` 在它之上补一层 queued client orchestration seam：
  - 允许上层先排队多个 request
  - 用 `kick()` 发出第一个 pending request
  - 用 `step(event)` 统一收口 reply / timeout，并在空闲时自动发出下一条
  - 用 `receive_completion(...)` 把完成结果交还给上层

也就是说，这一层解决的是：

“client task 怎样在仍然保持单 in-flight transport 的前提下，把一串 message-backed syscall request 稳定排队、推进并收回 completion？”

而不是：

“现在就发明支持多 outstanding 的最终远端 RPC 子系统”

## 当前模块形状

当前模块导出：

- `TaskMessageSyscallPumpTraceKind`
- `task_message_syscall_pump_trace_kind_name(...)`
- `TaskMessageSyscallPumpRequest<Tick>`
- `make_task_message_syscall_pump_request(...)`
- `TaskMessageSyscallPumpCompletion<Reply>`
- `TaskMessageSyscallPumpStepResult<Client>`
- `TaskMessageSyscallPumpTraceEvent`
- `TaskMessageSyscallPumpTraceBuffer<Capacity>`
- `TaskMessageSyscallPump<Client, RequestCapacity, CompletionCapacity, TraceBuffer>`
- `make_task_message_syscall_pump(...)`

## 当前语义

这一层当前收口五件事：

1. 把多个 `TaskSyscallRequest` 或 `sys_*` helper 请求排入本地 request queue
2. 用 `kick()` 在 client 空闲时发出第一条 pending request
3. 用 `step(event)` 消费下层 client 的 reply / timeout 结果，并把它们写成 completion queue
4. 当一次 completion 被收口后，如果下层 client 已经空闲，则自动发出下一条 pending request
5. 保持下层 transport 始终只有一条 in-flight request，不伪装成多 outstanding transport

`step(event)` 的当前约束是：

- 只有命中下层 client 能消费的 reply event 或 reply-timeout event 才会推进
- reply 和 timeout 都会被收口成 `TaskMessageSyscallPumpCompletion`
- completion queue 满时，不会伪造成功，而是显式暴露 `completion_dropped`
- trace 只记录四类边界：`issue / reply / timeout / completion_drop`

## 与下一层的关系

建议把这条上半层 client 链理解成：

1. `kernel.task_message_syscall_frame_caller`
2. `kernel.task_message_syscall_client`
3. `kernel.task_message_syscall_pump`
4. `kernel.task_message_runtime_service`
5. future async task runtime facade / task RPC runtime

其中：

- `frame_caller` 负责 frame publish / reply completion transport glue
- `syscall_client` 负责 one-pending-call 的 task-facing async syscall seam
- `syscall_pump` 负责多 request 排队、completion 收口和自动续发
- `task_message_runtime_service` 负责把这些能力收成 task-side remote service 命名面
- 更高层 facade 才去讨论 session、同步等待、批量策略或真正的 RPC 风格 API

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_syscall_pump_host`
- `Examples/kernel/runtime_task_message_runtime_service_host`
- `Examples/kernel/runtime_task_message_syscall_client_host`
- `Examples/kernel/runtime_task_message_syscall_frame_caller_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_syscall_pump_host` 当前证明：

- 可以在 bootstrap 时一次性排入 `yield / sleep_until / capability_call / invalid syscall / missing-timeout probe`
- `kick()` 会立即发出第一条 request，并保持下层 client busy
- 每次 reply 或 timeout 都能被收回为稳定 completion，且顺序与排队顺序一致
- 当前一条 completion 收口后，pump 会自动发出下一条 pending request
- 人为抹掉 frame 后，missing reply 会继续走 timeout，而不是伪装成服务端 reply
- trace 当前形成稳定的 `issue/reply/issue/reply/.../issue/timeout` 十条证据，且没有 `completion_drop`

## 当前非目标

当前这层仍然不处理：

- 真正多 outstanding 的 remote syscall transport
- 同步阻塞的 remote syscall facade
- 跨 client 队列的公平性、优先级和取消策略
- service discovery / routing policy
- 跨地址空间 transport
- user/kernel ABI

后续如果继续往上长，更健康的方向是：

- 先在这层之上接 `task_message_runtime_service`
- 再继续往 async task runtime facade / IPC RPC runtime 长

而不是把 `TaskMessageSyscallPump` 本身直接膨胀成最终的远端调用系统。
