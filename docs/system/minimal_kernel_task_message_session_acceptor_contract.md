# 最小内核 task message session acceptor / channel facade 契约（草案）

这份文档描述的不是对 `task_message_session_dispatch` 的重写，而是在它之上新增的一层更贴近 server-side session runtime 的 facade：

- `kernel.task_message_session_acceptor`

对应当前新增的模块与证据路径：

- `Modules/system/kernel/task_message_session_acceptor.cppm`
- `Examples/kernel/runtime_task_message_session_acceptor_host`

## 一句话版本

- `task_message_session_dispatch` 继续负责全局 `service_id -> session_handle -> open/request/close` ingress seam
- `task_message_session_acceptor` 在它之上补一层“每个 service 一个 acceptor、每个 accepted session 一个 channel handler”的 server-side facade
- dispatcher 分配出来的 `session_handle` 不变
- facade 自己管理每个 service 内部的 `channel slot -> channel handler` 生命周期

也就是说，这一层解决的是：

“当 server 侧已经能接住 `open / request / close` 以后，怎样把 accepted session 长成真正可持有状态的 per-channel handler？”

而不是：

“现在就把 `TaskMessageSessionDispatcher` 本体膨胀成完整的 session framework、service loop 和 channel registry。”

## 当前模块形状

当前模块导出：

- `TaskMessageSessionChannel`
- `TaskMessageSessionChannelHandler`
- `make_task_message_session_channel_handler(...)`
- `TaskMessageSessionChannelAcceptor`
- `make_task_message_session_channel_acceptor(...)`
- `TaskMessageSessionChannelSlot`
- `TaskMessageSessionChannelLookup`
- `TaskMessageSessionServiceAcceptorResult`
- `TaskMessageSessionServiceAcceptorTraceEvent`
- `TaskMessageSessionServiceAcceptorTraceBuffer<Capacity>`
- `TaskMessageSessionServiceAcceptor<ChannelCapacity, TraceBuffer>`
- `make_task_message_session_service_acceptor(...)`
- `task_message_session_service_acceptor_entry(...)`

它同时转导出：

- `kernel.task_message_session_dispatch`

因此 server-side 只要 `import kernel.task_message_session_acceptor`，就能同时拿到：

- 现有的 session dispatch ingress seam
- 新的 service acceptor / accepted channel facade

## 分层关系

建议把当前 server-side session 上半层理解成三层：

1. `task_message_session_dispatch`
   - 负责 `service_id -> handler entry`
   - 负责全局 `session_handle` 分配
   - 负责 global session slot 生命周期
2. `task_message_session_acceptor`
   - 负责每个 service 内部的 accepted channel slot 表
   - 负责 `session_handle -> accepted channel handler`
   - 负责 close 成功后的 channel slot 回收
3. future server loop / protocol surface
   - 负责谁来持有 acceptor
   - 负责 service task body / wait-drain orchestration
   - 负责更真实的 message session protocol

关键点是：

- `TaskMessageSessionDispatcher` 不需要知道 accepted channel 的内部状态
- `TaskMessageSessionServiceAcceptor` 也不重做 `service_id` 查表和全局 `session_handle` 分配

这两层是叠加关系，不是替代关系。

## 当前语义模型

### 1) 每个 service 一个 acceptor facade

`TaskMessageSessionServiceAcceptor<...>` 的定位是“一个 service 对应一个 facade 实例”。

它内部持有：

- 一个 `TaskMessageSessionChannelAcceptor`
- 一个 service-local channel slot 数组
- 可选 trace buffer

因此它天然适合这样接回现有 dispatcher：

- `task_message_session_service_acceptor_entry(service_id, acceptor)`

也就是说，dispatcher 看见的仍然只是一个普通 `TaskMessageSessionHandler`；
只是这个 handler 的 `open / request / close` 语义，已经变成了“accepted channel facade”的语义。

### 2) open 语义

`open` 到来时：

1. dispatcher 先决定这是哪个 service，并先分配好 `session_handle`
2. `TaskMessageSessionServiceAcceptor` 在自己的 channel slot 表里找空槽
3. 它构造 `TaskMessageSessionChannel`
4. 调用 `TaskMessageSessionChannelAcceptor.accept(...)`
5. 如果 accept 成功并返回有效 `TaskMessageSessionChannelHandler`
   - 激活该 channel slot
   - 之后这个 `session_handle` 的 request / close 都路由到这个 channel handler

这里最重要的是：

- `session_handle` 仍然来自 dispatcher 的全局语义
- `channel_slot` 则是 service-local 的 accepted channel 语义

### 3) request / close 语义

`request` 和 `close` 到来时：

1. facade 先按 `session_handle` 查 active channel slot
2. 命中后，把原始 dispatch view 连同当前 `TaskMessageSessionChannel` 一起交给 per-channel handler
3. `close` 成功时，释放对应 channel slot

因此 accepted channel handler 可以稳定看到：

- `service_id`
- `service_name`
- `session_handle`
- `open_payload`
- `channel_slot`

也就是说，server-side channel handler 拿到的已经不再只是“一次 request”，而是一条带 session 上下文的对话通道视图。

## 当前错误边界

当前 facade 显式收口这些边界：

### 1) acceptor 未绑定

- `open` 直接返回 `TrapError::unbound_adapter`

### 2) accept 成功但未绑定有效 channel handler

- 仍然返回 `TrapError::unbound_adapter`

这条边界很重要，因为它证明：

- “accept 被调用了” 与 “真正形成 accepted channel” 不是同一件事
- facade 不会把“空 handler”伪装成成功 open

### 3) channel slot 用尽

- `open` 返回 `TrapError::invalid_argument`

这条边界收的是 service-local channel 容量耗尽，而不是 dispatcher 的 global session slot 耗尽。

### 4) request / close 找不到 session_handle

- 返回 `TrapError::invalid_argument`

也就是说，dispatcher 和 acceptor 之间的语义分工是：

- dispatcher 保证 `service_id -> session_handle`
- acceptor 保证 `session_handle -> accepted channel`

## 当前 live 证据路径

与这层直接相关的 host verifier 是：

- `Examples/kernel/runtime_task_message_session_acceptor_host`

它当前至少证明：

1. direct facade lifecycle
   - `open -> request -> close`
   - close 成功后 slot 可回收并 reopen
2. channel slot exhaustion
   - service-local channel 槽位满时稳定拒绝
3. broken / unbound acceptor
   - 未绑定 acceptor
   - accept 返回 handled 但没绑定 channel handler
4. dispatcher integration
   - `task_message_session_service_acceptor_entry(...)` 能直接挂回 `TaskMessageSessionDispatcher`
   - 证明这层是 dispatcher 之上的 facade，而不是旁路实现

## 当前没有证明什么

当前这层仍然不处理：

- 真正的 server loop ownership
- service discovery / dynamic service registry
- 多 service 共享 channel registry
- 多 client 公平性
- 阻塞式 RPC facade
- 用户态协议格式
- 真实 arch ingress / ARMv7-A leaf frame

所以它的定位仍然是：

- server-side session runtime 的第一层薄 facade
- 不是最终的 session server framework

## 下一跳建议

这条 seam 之后更自然的上行方向是：

1. 先把 `task_message_session_endpoint` 这一层补出来，把 accepted channel 的上下文与 handler 绑定样板再抽薄一层
2. 再把 `task_message_session_service` 这一层补出来，让 server loop ownership 不必直接抓裸 `service_pump`
3. 然后把 live verifier 升级成“client session facade + server session service/endpoint facade”的 weld
4. 最后再讨论更真实的 channel routing / protocol surface

而不是继续把 `TaskMessageSessionDispatcher` 本体做成全能类型。
