# 最小内核 task message session dispatch 契约（草案）

这份文档把刚长出来的 `task_message_session_api` 对称到 server 侧，收成一条最小的：

- `service_id -> session slot -> handler`

分发缝。

它对应当前新增的：

- `Modules/system/kernel/task_message_session_dispatch.cppm`

目标不是现在就发明完整的 RPC server framework，而是先证明我们已经能在现有 `capability_call` 语义之上，稳定把：

- `open`
- `request`
- `close`

这三类 session ingress 路由到真正的 service handler。

## 一句话版本

- `task_message_session_api` 负责 client 侧发起 session 对话
- `task_message_session_dispatch` 负责 server 侧接住同一套 session 语义
- 它做的事很克制：
  - 用 `service_id` 找 handler
  - 用 `session_handle` 找活动会话槽
  - 在 `open / request / close` 三种 action 之间稳定分流

也就是说，这层解决的是：

“如果 client 已经会发最小 session 语义，那么 server 侧最小应该怎样 accept / serve / close？”

而不是：

“现在就把 service discovery、channel registry、多路复用和完整 server loop 全都做完。”

## 当前模块形状

当前模块导出：

- `TaskMessageSessionOpenDispatchView`
- `TaskMessageSessionRequestDispatchView`
- `TaskMessageSessionCloseDispatchView`
- `TaskMessageSessionHandler`
- `make_task_message_session_handler(...)`
- `TaskMessageSessionHandlerEntry`
- `task_message_session_handler_entry(...)`
- `TaskMessageSessionServiceLookup`
- `TaskMessageSessionSlot`
- `TaskMessageSessionSlotLookup`
- `TaskMessageSessionDispatchResult`
- `TaskMessageSessionDispatchTraceEvent`
- `TaskMessageSessionDispatchTraceBuffer<Capacity>`
- `TaskMessageSessionDispatcher<ServiceCapacity, SessionCapacity, TraceBuffer>`
- `make_task_message_session_dispatcher(...)`

它同时转导出：

- `kernel.task_message_session_api`
- `kernel.task_syscall_dispatch`

因此 server-side 只 import `kernel.task_message_session_dispatch`，就能拿到：

- client/session 侧保留的控制常量
- capability-call request 词汇
- server-side session handler 与分发表

## 当前责任边界

这层当前只负责三件事：

1. `open`
   - 把 `capability_id` 当成 `service_id`
   - 查找 service handler
   - 分配新的 `session_handle`
   - 只有 handler 成功时才激活 session slot
2. `request`
   - 把 `capability_id` 当成既有 `session_handle`
   - 查找活动 slot
   - 把 `(service_id, session_handle, operation, payload)` 交给 handler
3. `close`
   - 查找活动 slot
   - 把 close reason 交给 handler
   - 只有 handler 成功时才释放 slot

这意味着它既不重写 transport，也不重写 client 状态机，而是把 server 侧最小 ingress 与本地 slot 管理先收稳。

## 当前 handler 形状

handler 当前必须提供三组入口：

- `open(TaskMessageSessionOpenDispatchView)`
- `request(TaskMessageSessionRequestDispatchView)`
- `close(TaskMessageSessionCloseDispatchView)`

其中：

- `open` 会看到 `service_id + session_handle + payload`
- `request` 会看到 `service_id + session_handle + operation + payload`
- `close` 会看到 `service_id + session_handle + reason`

handler 当前继续直接返回 `TrapResult`。

这意味着：

- 成功仍然用 `TrapDisposition::handled + TrapError::none`
- 失败仍然沿用既有 `invalid_argument / unsupported_service / unbound_adapter`
- 不额外引入第二套 session-server result 协议

唯一特殊点是：

- `open` 成功后，dispatcher 会把 reply value 固定写成新分配的 `session_handle`

因为 client 侧 session API 当前就是靠这条约定拿到 handle。

## 当前 slot 语义

dispatcher 内部当前维护最小的 session slot 数组，每个 slot 只保存：

- `active`
- `service_id`
- `session_handle`
- `service_slot`

当前约定：

- `open` 成功时，占用空闲 slot
- `request` 不改变 slot 占用
- `close` 成功时，释放 slot
- `close` 失败时，slot 保持原样

这条边界很关键，因为它保证 server 侧不会把“关闭失败”偷偷伪装成“已经释放”。

## 当前错误语义

当前最小规则是：

- service_id 未命中
  - `TrapDisposition::unsupported`
  - `TrapError::unsupported_service`
- handler 未绑定
  - `TrapDisposition::rejected`
  - `TrapError::unbound_adapter`
- session_handle 未命中
  - `TrapDisposition::rejected`
  - `TrapError::invalid_argument`
- session slot 用尽
  - `TrapDisposition::rejected`
  - `TrapError::invalid_argument`
- 非 `capability_call` request 打到这层
  - `TrapDisposition::unsupported`
  - `TrapError::unsupported_service`

## 与现有链路的关系

当前建议把 message-backed session 两端理解成：

client 侧：

1. `kernel.task_message_runtime_service`
2. `kernel.task_message_runtime_api`
3. `kernel.task_message_syscall_api`
4. `kernel.task_message_session_api`

server 侧：

1. `kernel.task_syscall_table`
2. `kernel.task_message_session_dispatch`
3. future session service loop / acceptor / channel facade

其中：

- client 侧负责发起对话
- server 侧负责接住 capability-call 形式的 session ingress
- 中间仍然复用现有 syscall/message 证据链

## 当前 live 证据路径

与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_session_dispatch_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_session_dispatch_host` 当前证明：

- `open -> request -> close` 的 server-side dispatch 路径已经独立可跑
- `service_id` lookup、session slot 分配与回收是稳定的
- slot 用尽时会显式拒绝新的 `open`
- unbound handler、unknown service、unknown session handle 都有稳定负向结果
- 这层已经可以直接作为 `TaskSyscallHandler` 挂进 `task_syscall_table`

## 当前非目标

当前这层仍然不处理：

- 多 client / 多 queue 公平性
- service discovery / registry
- channel / endpoint routing
- 阻塞式 RPC server facade
- 真正的 message service loop ownership
- user/kernel ABI
- 真实 arch ingress

后续如果继续往上长，更健康的方向是：

- 在这层之上继续长 server-side session acceptor / channel facade
- 再把 client 与 server 两边的 session 语义焊成真正的双端运行时

而不是把 `TaskMessageSessionDispatcher` 本身直接膨胀成完整框架。
