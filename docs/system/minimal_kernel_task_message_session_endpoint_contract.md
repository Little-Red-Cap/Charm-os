# 最小内核 task message session endpoint facade 契约（草案）

这份文档描述的不是对 `task_message_session_acceptor` 的重写，而是在它之上新增的一层更贴近 server-side protocol / channel ownership 的薄 facade：

- `kernel.task_message_session_endpoint`

对应当前新增的模块与证据路径：

- `Modules/system/kernel/task_message_session_endpoint.cppm`
- `Examples/kernel/runtime_task_message_session_endpoint_host`
- `Examples/kernel/runtime_task_message_session_service_loop_host`

## 一句话版本

- `task_message_session_acceptor` 继续负责 `session_handle -> accepted channel slot -> channel handler` 的底层收口
- `task_message_session_endpoint` 在它之上补一层更薄的 server-side endpoint 语义
- acceptor 只需要处理一个稳定的 `endpoint` 视图和一个 `binding`
- request / close handler 也不必再直接吃原始 `channel + dispatch view` 组合

也就是说，这一层解决的是：

“accepted channel 已经建立以后，怎样把 server 侧真正想持有的 per-session context 和 reply 语义，再抽薄成更稳定的一层？”

而不是：

“现在就把 `TaskMessageSessionServiceAcceptor` 直接膨胀成完整 protocol framework。”

## 当前模块形状

当前模块导出：

- `TaskMessageSessionEndpoint`
- `TaskMessageSessionEndpointRequestView`
- `TaskMessageSessionEndpointCloseView`
- `TaskMessageSessionEndpointBinding`
- `make_task_message_session_endpoint(...)`
- `task_message_session_endpoint_handled(...)`
- `task_message_session_endpoint_rejected(...)`
- `task_message_session_endpoint_unsupported(...)`
- `task_message_session_endpoint_invalid_argument(...)`
- `task_message_session_endpoint_unbound_adapter(...)`
- `make_task_message_session_endpoint_handler(...)`
- `bind_task_message_session_endpoint(...)`
- `make_task_message_session_endpoint_acceptor(...)`

它同时转导出：

- `kernel.task_message_session_acceptor`

因此 server-side 只要 `import kernel.task_message_session_endpoint`，就能同时拿到：

- 现有 accepted channel / service acceptor facade
- 新的 endpoint 语义桥接层

## 当前语义

### 1) endpoint 只收口稳定上下文，不改 session handle 语义

`TaskMessageSessionEndpoint` 只携带：

- `service_id`
- `service_name`
- `session_handle`
- `open_payload`
- `channel_slot`

也就是说：

- global `session_handle` 语义仍然来自 dispatcher / acceptor
- 这一层只把 accepted channel 的稳定上下文整理成更好用的 view

### 2) handler bridge 把 `channel + dispatch view` 收成 endpoint 语义

`make_task_message_session_endpoint_handler(target)` 要求 target 只实现：

- `request(TaskMessageSessionEndpointRequestView)`
- `close(TaskMessageSessionEndpointCloseView)`

这意味着 server-side handler 看到的是：

- 一份稳定的 endpoint 上下文
- 再加上 `operation / payload` 或 `reason`

而不必继续在每个 handler 里手工拼：

- `channel.service_id`
- `channel.session_handle`
- `channel.open_payload`
- `channel.channel_slot`

### 3) acceptor bridge 把 raw handler 绑定样板收成一个 `binding`

`make_task_message_session_endpoint_acceptor(target)` 要求 target 实现：

- `accept(TaskMessageSessionEndpoint endpoint, TaskMessageSessionEndpointBinding binding)`

这样 acceptor 侧只需要决定：

- 这次 open 对应哪个 service-local endpoint state
- 要不要把这个 endpoint state 绑定成后续的 request / close handler

而不必再直接构造：

- `TaskMessageSessionChannelHandler`
- `make_task_message_session_channel_handler(...)`

具体绑定通过：

- `bind_task_message_session_endpoint(binding, target)`

完成。

### 4) reply 语义先收成最小 helper，不提前长成完整协议层

当前模块只提供最小 reply helper：

- `task_message_session_endpoint_handled(...)`
- `task_message_session_endpoint_rejected(...)`
- `task_message_session_endpoint_unsupported(...)`

以及两个常用边界：

- `task_message_session_endpoint_invalid_argument(...)`
- `task_message_session_endpoint_unbound_adapter(...)`

它们的目标只是把 demo / verifier 里重复出现的 `TrapResult` 样板先收口，
而不是现在就引入更高层 protocol result type。

## 当前 live 证据

`Examples/kernel/runtime_task_message_session_endpoint_host` 至少证明：

1. endpoint handler bridge
   - endpoint-style `request / close` target
   - 能稳定桥接回既有 `TaskMessageSessionChannelHandler`
2. endpoint acceptor bridge
   - endpoint-style `accept(endpoint, binding)`
   - 能稳定桥接回既有 `TaskMessageSessionServiceAcceptor`
   - slot exhaustion 与 “accept 成功但未绑定 handler” 仍然按既有边界暴露
3. dispatcher integration
   - `task_message_session_service_acceptor_entry(...)`
   - 仍能稳定把 endpoint facade 挂回 `TaskMessageSessionDispatcher`

`Examples/kernel/runtime_task_message_session_service_loop_host` 现在进一步证明：

- endpoint facade 不是只在 synthetic host 里成立
- 它已经能被真实 `session_service` live loop 复用
- server task body 持有的 ownership seam 不会因为这层抽薄而破坏

## 当前没有证明什么

当前这层仍然不处理：

- 完整 session protocol schema
- 多 endpoint / 多 client 公平性
- 阻塞式 RPC facade
- channel registry / endpoint discovery
- user/kernel ABI
- 真实 arch ingress / ARMv7-A leaf frame

所以它的定位仍然是：

- server-side accepted channel 之上的一层 endpoint 语义收口
- 不是最终的 protocol framework

## 下一跳建议

这条 seam 之后更自然的上行方向是：

1. 继续把 live `session_service_loop` 和 future roundtrip 路径切到 endpoint facade
2. 在 endpoint 之上补更明确的 protocol surface / endpoint ownership
3. 最后再讨论更完整的 session server framework

而不是继续让 `acceptor` 或具体 demo handler 自己增长样板代码。
