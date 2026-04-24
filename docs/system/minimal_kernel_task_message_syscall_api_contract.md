# 最小内核 task message syscall API 契约（草案）

这份文档把 `task_message_runtime_api` 再往上抬半层，收成一个更接近“当前任务正在发起 async syscall”的命名面。

它对应当前新增的：

- `Modules/system/kernel/task_message_syscall_api.cppm`

目标不是现在就把 message-backed runtime 伪装成同步 RPC，也不是提前决定 future/promise 或 session/channel 形态，而是先把 task-facing 的 `sys_*` 语义收成一个稳定、薄、可验证的 async facade。

## 一句话版本

- `task_message_runtime_service` 负责 remote runtime service facade
- `task_message_runtime_api` 负责 current-task async runtime naming
- `task_message_syscall_api` 在其之上再补一层 async syscall-facing naming：
  - 把 `yield / sleep_until / debug_write / capability_call` 收成 `sys_yield / sys_sleep_until / sys_debug_write / sys_capability_call`
  - 保留 `kick / step / receive_completion` 这组显式异步推进语义
  - 继续把 pending queue、busy state 和 runtime rebind 暴露出来

也就是说，这层解决的是：

“如果当前任务已经站在 message-backed runtime 之上，那么它看到的最小 syscall surface 应该长什么样？”

而不是：

“现在就把 transport、completion protocol 和等待策略全部藏起来，直接发明最终 async RPC。”

## 当前模块形状

当前模块导出：

- `TaskMessageSyscallApi<Runtime>`
- `make_task_message_syscall_api(...)`

它同时转导出：

- `kernel.task_message_runtime_api`

因此 task-side 只 import `kernel.task_message_syscall_api`，就能拿到：

- async syscall-facing naming
- current-task async runtime facade
- remote runtime service facade
- 既有 trap/syscall view 词汇

## 当前 API 形状

`TaskMessageSyscallApi<Runtime>` 当前对 task/worker context 暴露：

- `valid()`
- `busy()`
- `pending_requests()`
- `pending_completions()`
- `runtime()`
- `bind_runtime(...)`
- `bind_cursors(...)`
- `sys_yield(wait_due)`
- `sys_yield(TrapYieldCurrentView, wait_due)`
- `sys_sleep_until(due, wait_due)`
- `sys_sleep_until(TrapSleepUntilView<Tick>, wait_due)`
- `sys_debug_write(value, wait_due)`
- `sys_debug_write(TrapDebugWriteView, wait_due)`
- `sys_capability_call(capability_id, operation, wait_due)`
- `sys_capability_call(capability_id, operation, payload, wait_due)`
- `sys_capability_call(TrapCapabilityCallView, wait_due)`
- `kick()`
- `step(event)`
- `receive_completion(...)`

这里最关键的变化不是 transport 能力又变多了一层，而是把 task-facing 的主语继续从：

- “我在推进一个 async runtime”

收成：

- “我在发起一个 async syscall”

因此这一层的价值主要是命名面与边界稳定性，而不是协议重写。

## 与同步链的对照

当前建议把最小运行时链理解成两条平行梯子：

同步 / 本地：

1. `kernel.runtime_service`
2. `kernel.task_runtime_api`
3. `kernel.task_syscall_api`

异步 / message-backed：

1. `kernel.task_message_runtime_service`
2. `kernel.task_message_runtime_api`
3. `kernel.task_message_syscall_api`

这意味着我们并不是在另起炉灶，而是在把已有同步链的命名层次，平移成一条显式异步推进的 message-backed 版本。

## 与下一层的分工

当前建议这样切：

- `task_message_runtime_api`
  - 解决“当前任务怎样以 runtime 语义排队并推进远端服务”
  - 保留 `services()` 作为更底层 escape hatch
- `task_message_syscall_api`
  - 解决“当前任务怎样以 syscall-facing 语义使用同一组 async 能力”
  - 继续保留 `kick / step / receive_completion`
  - 通过 `runtime()` 保留向下回到 runtime/service facade 的路径

这层因此不是新的 transport，也不是新的 protocol，而是 message-backed syscall surface 的最小收口。

## 当前语义边界

### 1) 这层不重写 completion 协议

当前 `step(event)` 和 `receive_completion(...)` 仍然直接沿用下层 runtime facade 的结果类型。

这意味着：

- completion 的真实结构仍然来自 `task_message_syscall_pump`
- 这层不额外发明第四套 async completion object
- 这层的价值主要是 `sys_*` naming，而不是 completion schema 改造

### 2) 这层不把 raw enqueue 升成主入口

和 `task_message_runtime_service` 不同，这层默认不直接暴露：

- `enqueue(TaskSyscallRequest, ...)`
- `enqueue(owner, request, ...)`
- `enqueue(owner, token, sequence, ...)`

如果上层仍然需要：

- invalid syscall 负向样本
- 显式 token / sequence 控制
- future IPC / RPC facade 的逃生口

当前建议通过：

- `runtime().services()`

回到底下那层处理，而不是把 `TaskMessageSyscallApi` 自己做成混合层。

### 3) 这层仍然显式保留 async progress

虽然这层把命名改成了 `sys_*`，但它仍然明确保留：

- `kick()`
- `step(event)`
- `receive_completion(...)`
- `busy()`
- `pending_requests()`
- `pending_completions()`

也就是说，这层仍然不是阻塞式 facade，而是一层“syscall naming + explicit async progress”的薄壳。

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_syscall_api_host`
- `Examples/kernel/runtime_task_message_runtime_api_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_syscall_api_host` 当前证明：

- 默认未绑定 async syscall facade 会稳定拒绝 `sys_yield / sys_sleep_until / sys_debug_write / sys_capability_call`
- `sys_*` 命名入口会稳定映射到正确的 message-backed request
- `kick / step / receive_completion` 仍然保持可见，reply / timeout / auto issue 语义不会被 wrapper 吃掉
- `bind_runtime(...)` 的 unbind / rebind 仍然可见
- 如果需要 raw invalid syscall / explicit token 样本，仍然可以经由 `runtime().services()` 回到底层 remote-service facade

而已有的 `runtime_task_message_runtime_api_host` 继续证明：

- 这层依赖的 current-task async runtime facade 本身已经闭环

## 当前非目标

当前这层仍然不处理：

- 同步阻塞或 future/promise 风格 async API
- 真正多 outstanding 的 remote transport
- user/kernel ABI
- 真实 arch ingress

后续如果继续往上长，更健康的方向是：

- 在这层之上接 `task_message_session_api`
- 再继续讨论更高层的 async RPC / channel 语义
- 或者把它接到真实 trap/syscall boundary 与 user-facing ABI

而不是把 `TaskMessageSyscallApi` 本身直接膨胀成最终系统。
