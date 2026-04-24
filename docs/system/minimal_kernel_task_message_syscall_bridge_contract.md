# 最小内核 task message syscall bridge 契约（草案）

这份文档用于把上半层第一条真正的 `message runtime -> syscall semantic` 焊缝收成稳定 seam。

它对应当前新增的：

- `Modules/system/kernel/task_message_syscall_bridge.cppm`

目标不是现在就发明完整的 message-borne syscall ABI，而是先把一条很薄、但真实可跑的 v0 边界固定下来：

- message `label` 映射到 `TaskSyscallId`
- message `value` 映射到 `TaskSyscallRequest.arg0`
- `TrapResult.value` 映射到 message reply value
- 不支持的 message-side syscall 继续显式表现为“无 reply，由上层等待/超时观察到”

## 一句话版本

- `TaskMessageServicePump` 负责把 server task 跑起来
- `TaskMessageDispatch` 负责 `receive -> handler -> reply`
- `TaskMessageSyscallBridge` 负责把 mailbox request 解成最小 `TaskSyscallRequest`
- `TaskSyscallTable` / `TaskSyscallDispatch` 负责真正的 syscall 语义分发

也就是说，这一层证明的是：

“message 服务任务已经可以通过一条稳定 bridge，把一部分最小 syscall 语义吃进去并把结果回包出来。”

## 当前模块形状

当前模块导出：

- `task_message_syscall_label(...)`
- `task_message_syscall_label_name(...)`
- `task_message_syscall_id(...)`
- `task_message_syscall_ingress_supported(...)`
- `task_message_syscall_request_from_message(...)`
- `task_message_syscall_semantic_projection(...)`
- `TaskMessageSyscallBridgeResult`
- `TaskMessageSyscallBridgeTraceEvent`
- `TaskMessageSyscallBridgeTraceBuffer<Capacity>`
- `TaskMessageSyscallBridge<Dispatch, TraceBuffer>`
- `make_task_message_syscall_bridge(...)`

## v0 ingress 规则

当前 v0 故意只支持单字 payload 的 message-side syscall ingress。

也就是：

- `yield`
- `sleep_until`
- `debug_write`

当前明确不支持：

- `capability_call`
- 任何需要 `arg1..arg3` 才能完整表达的 syscall
- 任何试图把 message surface 提前升级成通用 syscall frame transport 的做法

当前约定是：

1. message label 使用 `TaskSyscallId` 的数值编码
2. message value 只承载 `arg0`
3. `arg1..arg3` 在 v0 中保持为 `0`

## handled / reply 语义

这一层继续遵守现有 message 服务面：

- 只有当下游 syscall dispatch 返回 `TrapDisposition::handled` 且 `TrapError::none` 时，bridge 才返回 `handled_task_message(reply_value)`
- 其余结果一律收成 `unhandled_task_message()`

这意味着：

- 成功 syscall 会转成正常 reply
- unsupported / rejected / unbound 结果不会被伪装成“看似成功的 reply”
- 这些失败路径当前主要通过 bridge trace 和 client-side reply timeout 被观察到

这正是当前 v0 想保留的边界：不提前发明新的错误回包 ABI。

## 与现有层的关系

建议把当前上半层链路理解成：

1. `kernel.runtime_mailbox`
2. `kernel.task_message_api`
3. `kernel.task_message_table`
4. `kernel.task_message_dispatch`
5. `kernel.task_message_service_loop`
6. `kernel.task_message_service_drain`
7. `kernel.task_message_service_pump`
8. `kernel.task_message_syscall_bridge`
9. `kernel.task_syscall_table`
10. `kernel.task_syscall_dispatch`

其中：

- `TaskMessageTable` 继续负责 message label 是否被 server surface 接受
- `TaskMessageSyscallBridge` 负责把已接受的 request 投影成最小 syscall request
- `TaskSyscallTable/Dispatch` 继续负责 syscall 语义本身

## 当前证据路径

当前与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_syscall_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

这条 verifier 当前证明：

- 真实 `RuntimeBridge + RuntimeMailbox + TaskMessageServicePump` 上的 message 服务任务是活的
- `yield / sleep_until / debug_write` 能经过 message ingress v0 投影到 syscall table
- `TrapResult.value` 能回到 message reply
- `capability_call` 在当前 message ingress v0 中保持 unsupported，不伪装成成功 reply
- client 侧可以通过 reply timeout 观察到这条 unsupported 边界

## 当前非目标

当前这层仍然不处理：

- 完整多参数 message-borne syscall ABI
- per-request errno / error reply schema
- user/kernel ABI
- numbered syscall frame
- arch trap ingress capture / writeback

如果后面要把 message syscall 真正长成完整 transport，更健康的方向是：

- 在这层之上再长一个显式 message-frame seam
- 而不是把 `TaskMessageSyscallBridge` 直接变成隐式的万能 ABI 层
