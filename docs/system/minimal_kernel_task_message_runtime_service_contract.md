# 最小内核 task message runtime service 契约（草案）

这份文档把 `task_message_syscall_pump` 再往上抬半层，收成一个更接近“任务侧远端服务门面”的命名面。

它对应当前新增的：

- `Modules/system/kernel/task_message_runtime_service.cppm`

目标不是直接发明同步 RPC，也不是绕过现有 `task_message -> syscall` 链重做一套协议，而是先给 task 侧一个稳定、薄、可验证的 remote-service facade。

## 一句话版本

- `task_message_syscall_pump` 负责 queued client orchestration
- `task_message_runtime_service` 在它之上补一层 task-side remote service facade：
  - 继续保留排队、`kick()`、`step(event)`、completion 收口
  - 但把任务侧常见入口重新命名成 `yield_current / sleep_current_until / debug_write / capability_call`
  - 同时保留 raw `enqueue(...)` 作为负向样本和 future extension 的逃生口

也就是说，这一层解决的是：

“task 怎样把一串 message-backed 远端 runtime service 请求，按任务能直接读懂的名字排队、推进并回收完成结果？”

而不是：

“现在就定版最终的异步 RPC session / multi-outstanding transport”

## 当前模块形状

当前模块导出：

- `TaskMessageRuntimeServiceFacade<Pump>`
- `make_task_message_runtime_service_facade(...)`

它同时转导出：

- `kernel.runtime_service`
- `kernel.task_message_syscall_pump`

因此 task-side 只 import `kernel.task_message_runtime_service`，就能拿到：

- 远端服务 facade
- 最小 trap/service view 词汇
- pump request / completion 类型

## 当前 API 形状

`TaskMessageRuntimeServiceFacade<Pump>` 当前对任务侧暴露：

- `valid()`
- `busy()`
- `pending_requests()`
- `pending_completions()`
- `pump()`
- `bind_pump(...)`
- `bind_cursors(...)`
- `enqueue(...)`
- `yield_current(wait_due)`
- `yield_current(TrapYieldCurrentView, wait_due)`
- `sleep_current_until(...)`
- `debug_write(...)`
- `capability_call(...)`
- `kick()`
- `step(event)`
- `receive_completion(...)`

这层最关键的变化不是 transport 能力变多，而是把“pump 眼里的 syscall request”重新写成“任务眼里的 remote runtime service request”。

## 与下一层的关系

建议把这条 message-backed 上半层链理解成：

1. `kernel.task_message_syscall_frame_caller`
2. `kernel.task_message_syscall_client`
3. `kernel.task_message_syscall_pump`
4. `kernel.task_message_runtime_service`
5. `kernel.task_message_runtime_api`
6. `kernel.task_message_syscall_api`
7. future async RPC / session runtime

其中：

- `frame_caller` 负责 frame publish / reply completion transport glue
- `syscall_client` 负责 one-pending-call 的 async syscall seam
- `syscall_pump` 负责 request queue、auto issue、completion queue
- `task_message_runtime_service` 负责把这些能力收成 task-side remote service 命名面
- `task_message_runtime_api` 负责把任务主语进一步收成 current-task async runtime API
- `task_message_syscall_api` 负责把同一组 async 能力继续收成 `sys_*` 命名面

## 当前语义边界

### 1) 这层不重写 pump 的完成协议

当前 `step(event)` 和 `receive_completion(...)` 继续沿用 pump 的 step result 与 completion 形状。

这意味着：

- reply / timeout 语义仍然来自下层 `task_message_syscall_pump`
- 这层不额外发明第三套 completion protocol
- 这层的价值主要在 task-side 命名与门面稳定性，而不是协议重写

### 2) 这层保留 raw `enqueue(...)` 作为 escape hatch

虽然当前更鼓励任务侧直接写：

- `yield_current(wait_due)`
- `sleep_current_until(due, wait_due)`
- `debug_write(value, wait_due)`
- `capability_call(id, op, payload, wait_due)`

但它仍然保留：

- `enqueue(TaskSyscallRequest, wait_due)`
- `enqueue(owner, request, wait_due)`
- `enqueue(owner, token, sequence, request, wait_due)`

这让上层仍然可以：

- 保留 invalid syscall 这类负向样本
- 显式指定 owner / token / sequence
- 继续给 future IPC / RPC facade 留出扩展缝

### 3) 这层不假装自己已经是最终 RPC

当前 facade 仍然明确暴露：

- `kick()`
- `step(event)`
- `receive_completion(...)`
- `pending_requests()`
- `pending_completions()`

也就是说，上层依然知道这是一条显式推进的 async queue seam，而不是一个已经阻塞、等待、隐藏 transport 的最终 RPC API。

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_runtime_service_host`
- `Examples/kernel/runtime_task_message_runtime_api_host`
- `Examples/kernel/runtime_task_message_syscall_api_host`
- `Examples/kernel/runtime_task_message_syscall_pump_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_runtime_service_host` 当前证明：

- 默认未绑定 facade 会稳定拒绝排队、`kick()` 和 completion 拉取
- `yield_current / sleep_current_until / debug_write / capability_call` 会被稳定翻译成正确的 `TaskSyscallRequest`
- `kick()`、`step(event)`、`receive_completion(...)` 会继续穿透到底层 pump 语义
- auto issue、reply completion、timeout completion 和 rebind 都能被任务侧 facade 直接观察到
- raw `enqueue(...)` 仍可承接 invalid syscall 和显式 token/sequence 样本

而已有的 `runtime_task_message_syscall_pump_host` 继续证明：

- 这层 facade 依赖的真实 message-backed pump 证据链本身已经闭环

## 当前非目标

当前这层仍然不处理：

- 真正多 outstanding 的 remote transport
- 同步阻塞的 RPC facade
- session / channel / service discovery
- 跨地址空间 transport
- user/kernel ABI
- 真正面向最终用户态 API 的 async runtime 封装

后续如果继续往上长，更健康的方向是：

- 先在这层之上接 `task_message_runtime_api`
- 再在其上接 `task_message_syscall_api`
- 再继续向 async RPC / session runtime 长出更明确的会话与路由语义

而不是把 `TaskMessageRuntimeServiceFacade` 本身直接膨胀成最终系统。
