# 最小内核 task message runtime API 契约（草案）

这份文档把 `task_message_runtime_service` 再往上抬半层，收成一个更接近“当前任务自己在使用远端 runtime service”的 async API。

它对应当前新增的：

- `Modules/system/kernel/task_message_runtime_api.cppm`

目标不是现在就发明最终 async RPC，也不是把 `task_message_runtime_service` 膨胀成一层过大的门面，而是先把 current-task 视角的 message-backed runtime 命名面单独收口。

## 一句话版本

- `task_message_runtime_service` 负责 remote service facade
- `task_message_runtime_api` 在它之上补一层 current-task async runtime API：
  - 把 `yield_current / sleep_current_until` 收成 `yield / sleep_until`
  - 保留 `kick / step / receive_completion` 这组显式异步推进语义
  - 让 task 代码看到的是“我自己排队并推进 runtime service”，而不是“我在操作某个 pump”

也就是说，这一层解决的是：

“当前任务怎样以比 remote service facade 更接近自服务语义的名字，排队并推进一串 message-backed runtime request？”

而不是：

“现在就隐藏 transport、等待策略和 completion protocol，直接给出最终 async RPC”

## 当前模块形状

当前模块导出：

- `TaskMessageRuntimeApi<Services>`
- `make_task_message_runtime_api(...)`

它同时转导出：

- `kernel.task_message_runtime_service`

因此 task-side 只 import `kernel.task_message_runtime_api`，就能拿到：

- async current-task runtime API
- 远端 runtime service facade
- 最小 trap/service view 词汇

## 当前 API 形状

`TaskMessageRuntimeApi<Services>` 当前对 task/worker context 暴露：

- `valid()`
- `busy()`
- `pending_requests()`
- `pending_completions()`
- `services()`
- `bind_services(...)`
- `bind_cursors(...)`
- `yield(wait_due)`
- `yield(TrapYieldCurrentView, wait_due)`
- `sleep_until(due, wait_due)`
- `sleep_until(TrapSleepUntilView<Tick>, wait_due)`
- `debug_write(value, wait_due)`
- `debug_write(TrapDebugWriteView, wait_due)`
- `capability_call(capability_id, operation, payload, wait_due)`
- `capability_call(TrapCapabilityCallView, wait_due)`
- `kick()`
- `step(event)`
- `receive_completion(...)`

这层最关键的变化不是多了新 transport，而是把主语从：

- “某个 remote service facade”

进一步收成：

- “当前任务自己”

所以这里的名字不再强调 `current`，而是直接给出：

- `yield(...)`
- `sleep_until(...)`
- `debug_write(...)`
- `capability_call(...)`

## 与上一层的分工

当前建议这样分：

- `task_message_runtime_service`
  - 解决“远端 runtime service 门面长什么样”
  - 仍然显式保留 `enqueue(...)` escape hatch
- `task_message_runtime_api`
  - 解决“当前任务真正想怎么调用这些远端 runtime service”
  - 把任务主语收清楚
  - 保留显式 async 推进语义，但不再鼓励直接围绕 `enqueue(...)` 写代码

这和同步链上的关系是对称的：

1. `kernel.runtime_service`
2. `kernel.task_runtime_api`

对应到当前 message-backed 异步链就是：

1. `kernel.task_message_runtime_service`
2. `kernel.task_message_runtime_api`

## 当前语义边界

### 1) 这层不重写 completion 协议

当前 `step(event)` 和 `receive_completion(...)` 仍然直接沿用下层 service facade 的结果类型。

这意味着：

- completion 的具体形状仍然来自 `task_message_syscall_pump`
- 这层不另造第四套 async completion object
- 这层的主要价值是 task-facing naming，而不是协议重写

### 2) 这层不直接暴露 raw syscall request helper

和 `task_message_runtime_service` 不同，这层默认不把：

- `enqueue(TaskSyscallRequest, ...)`
- `enqueue(owner, token, sequence, ...)`

作为主入口继续向上推广。

如果上层仍然需要：

- invalid syscall 负向样本
- 显式 token / sequence 控制
- future extension 的逃生口

当前建议通过：

- `services()`

回到底下一层处理，而不是让 `TaskMessageRuntimeApi` 自己变成混合层。

### 3) 这层不隐藏显式 async 推进

虽然它把调用名收成了 current-task 语义，但它仍然明确保留：

- `kick()`
- `step(event)`
- `receive_completion(...)`
- `busy()`
- `pending_requests()`
- `pending_completions()`

也就是说，这层仍然不是阻塞式 async facade，而是一层“current-task naming + explicit async progress”的薄壳。

## 与下一层的关系

建议把当前 message-backed 上半层链理解成：

1. `kernel.task_message_syscall_frame_caller`
2. `kernel.task_message_syscall_client`
3. `kernel.task_message_syscall_pump`
4. `kernel.task_message_runtime_service`
5. `kernel.task_message_runtime_api`
6. `kernel.task_message_syscall_api`
7. `kernel.task_message_session_api`
8. future async RPC / channel runtime

其中：

- `runtime_service` 负责 remote runtime service 命名面
- `runtime_api` 负责 current-task async runtime 命名面
- 再往上才适合讨论 async syscall facade、session、channel 或 RPC 风格 API

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_runtime_api_host`
- `Examples/kernel/runtime_task_message_syscall_api_host`
- `Examples/kernel/runtime_task_message_session_api_host`
- `Examples/kernel/runtime_task_message_runtime_service_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_runtime_api_host` 当前证明：

- 默认未绑定 async runtime 会稳定拒绝 `yield / sleep_until / debug_write / capability_call`
- 任务侧命名面会稳定映射到正确的 message-backed request
- `kick / step / receive_completion` 仍然保持可见，reply / timeout / auto issue 语义不会被 wrapper 吃掉
- `bind_services(...)` 的 unbind / rebind 仍然可见
- 如果需要 raw invalid syscall / explicit token 样本，仍然可以通过 `services()` 回到底下一层

而新的 `runtime_task_message_syscall_api_host` 继续证明：

- 这层 current-task async runtime facade 现在已经可以被更高一层 `sys_*` wrapper 稳定承接

而新的 `runtime_task_message_session_api_host` 进一步证明：

- 这层 current-task async runtime facade 现在已经能承接更高一层的最小 session 对话语义

## 当前非目标

当前这层仍然不处理：

- 真正隐藏 transport 的 async facade
- 同步阻塞或 future/promise 风格 API
- async syscall-facing naming
- session / channel / service discovery
- 真正多 outstanding 的 remote transport
- user/kernel ABI

后续如果继续往上长，更健康的方向是：

- 在这层之上再长 async task syscall facade
- 再继续向 IPC RPC runtime 收口

而不是把 `TaskMessageRuntimeApi` 本身直接变成最终系统。
