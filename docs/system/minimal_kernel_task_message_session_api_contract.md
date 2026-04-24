# 最小内核 task message session API 契约（草案）

这份文档把 `task_message_syscall_api` 再往上抬半层，收成一个真正开始长出“服务会话语义”的最小 async facade。

它对应当前新增的：

- `Modules/system/kernel/task_message_session_api.cppm`

目标不是现在就发明完整 RPC 框架，也不是提前决定 channel、service discovery 或多路复用策略，而是先证明我们已经能在现有 message-backed syscall 之上，稳定表达一条最小的：

- `open`
- `request`
- `reply`
- `close`

会话闭环。

## 一句话版本

- `task_message_runtime_api` 负责 current-task async runtime naming
- `task_message_syscall_api` 负责 async `sys_*` naming
- `task_message_session_api` 在其之上补一层 single in-flight session facade：
  - 用 `open / request / close` 表达一条最小服务对话
  - 用 `receive_completion(...)` 收口 reply / timeout
  - 把 `close timeout -> faulted -> reset()` 这条边界显式化

也就是说，这层解决的是：

“如果我们已经有了 message-backed async syscall surface，那么怎样才能第一次让任务以 session 语义和远端服务说话？”

而不是：

“现在就把 transport、scheduler、future/promise、service registry 和多会话复用全部一起做完。”

## 当前模块形状

当前模块导出：

- `TaskMessageSessionActionKind`
- `TaskMessageSessionPhase`
- `task_message_session_open_operation`
- `task_message_session_close_operation`
- `TaskMessageSessionOpenView`
- `TaskMessageSessionRequestView`
- `TaskMessageSessionCloseView`
- `TaskMessageSessionCompletion<RawCompletion>`
- `TaskMessageSessionApi<Syscalls>`
- `make_task_message_session_api(...)`

它同时转导出：

- `kernel.task_message_syscall_api`

因此 task-side 只 import `kernel.task_message_session_api`，就能拿到：

- async session-facing naming
- async syscall-facing naming
- current-task async runtime facade
- 更底下的 remote runtime service facade

## 当前 API 形状

`TaskMessageSessionApi<Syscalls>` 当前对 task/worker context 暴露：

- `valid()`
- `busy()`
- `opened()`
- `faulted()`
- `phase()`
- `pending_requests()`
- `pending_completions()`
- `service_id()`
- `session_handle()`
- `syscalls()`
- `bind_syscalls(...)`
- `bind_cursors(...)`
- `open(service_id, wait_due)`
- `open(TaskMessageSessionOpenView, wait_due)`
- `request(operation, payload, wait_due)`
- `request(TaskMessageSessionRequestView, wait_due)`
- `close(reason, wait_due)`
- `close(TaskMessageSessionCloseView, wait_due)`
- `reset()`
- `kick()`
- `step(event)`
- `receive_completion(...)`

这里最关键的变化不再只是命名面换一层，而是第一次显式引入：

- 会话状态
- 打开后的 handle
- 关闭失败后的 faulted 边界

这让“message runtime + syscall 语义”第一次真正长成了“我正在和一个远端服务进行一次有状态对话”。

## 当前语义模型

当前这层故意只支持：

- `single in-flight session`

这意味着：

- 同一时刻最多只能有一条会话
- 打开后必须等待 completion 收到，才能发下一次 request
- close 收到 completion 之后，才能重新 open

当前 phase 语义是：

- `idle`
- `opening`
- `open`
- `requesting`
- `closing`
- `faulted`

其中：

- `open(...)` 把 phase 从 `idle` 推到 `opening`
- `request(...)` 把 phase 从 `open` 推到 `requesting`
- `close(...)` 把 phase 从 `open` 推到 `closing`
- `receive_completion(...)` 负责把 phase 推回稳定状态

## 当前与 syscall 层的映射

当前这层并不重新发明 transport，而是把 session 控制信号编码到已有 `capability_call` 上：

- `open(service_id, payload, wait_due)`
  - 映射成 `sys_capability_call(service_id, task_message_session_open_operation, payload, wait_due)`
  - completion 的 `TrapResult.value` 被解释为新的 `session_handle`
- `request(operation, payload, wait_due)`
  - 映射成 `sys_capability_call(session_handle, operation, payload, wait_due)`
- `close(reason, wait_due)`
  - 映射成 `sys_capability_call(session_handle, task_message_session_close_operation, reason, wait_due)`

这让 session 语义可以先站在现有 syscall 证据链之上长出来，而不用额外引入第二套 transport。

## 当前 completion 语义

`receive_completion(...)` 当前返回 `TaskMessageSessionCompletion<RawCompletion>`，其中收口了：

- `action`
- `phase_before`
- `phase_after`
- `timeout`
- `session_opened`
- `session_closed`
- `session_faulted`
- `service_id`
- `session_handle`
- `operation`
- `payload`
- `reply_value`
- `trap`
- `raw`

当前约定：

- `open` 成功且 `TrapDisposition::handled`
  - session 进入 `open`
  - `TrapResult.value` 写入 `session_handle`
- `open` timeout 或失败
  - session 回到 `idle`
- `request` completion
  - 无论 reply 还是 timeout，session 先回到 `open`
- `close` 成功完成
  - session 回到 `idle`
- `close` timeout
  - session 进入 `faulted`
  - 本地仍保留最后一次 `service_id / session_handle`
  - 直到显式 `reset()`

这里最值钱的边界是：

- `close timeout` 不会被悄悄伪装成“关闭成功”
- 也不会被直接吞成“还能继续当作 open 会话使用”

而是明确进入 `faulted`。

## 当前 escape hatch

虽然这层开始长 session 语义，但它仍然保留了一条向下回退的路径：

- `syscalls()`

这意味着如果上层还需要：

- raw `sys_*`
- raw runtime facade
- raw invalid syscall / explicit token 样本

仍然可以通过：

- `syscalls()`
- `syscalls().runtime()`
- `syscalls().runtime().services()`

一路回到底层。

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_session_api_host`
- `Examples/kernel/runtime_task_message_syscall_api_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_session_api_host` 当前证明：

- 默认未绑定 session facade 会稳定拒绝 `open / request / close`
- `open -> request -> close` 能形成一条完整 roundtrip
- `open` reply 会把 handle 稳定写进本地 session state
- `request` completion 会保持会话继续处于 `open`
- `close timeout` 会把本地会话推入 `faulted`
- `reset()` 能把 faulted session 明确收回 `idle`
- `bind_syscalls(...)` 会重置旧会话状态，不把旧 handle 泄漏到新 binding
- 如果仍然需要 raw invalid syscall 样本，可以继续通过 `syscalls()` 下钻

## 当前非目标

当前这层仍然不处理：

- 多 session 并发
- session routing / discovery
- channel / endpoint registry
- 阻塞式 RPC 或 future/promise API
- 真正多 outstanding transport
- user/kernel ABI
- 真实 arch ingress

后续如果继续往上长，更健康的方向是：

- 让这层在 server 侧接上 `task_message_session_dispatch`
- 再在两边之上继续长 async RPC / channel facade
- 或者把这层映射到更真实的用户态 service/session 语义

而不是把 `TaskMessageSessionApi` 立刻膨胀成完整框架。
