# 最小内核 task message session protocol surface 契约（草案）

这份文档描述的不是一个新的“大框架”，而是在 `task_message_session_endpoint` 之上新增的一层很薄的 server-side protocol surface：

- `kernel.task_message_session_protocol`

对应当前新增的模块与证据路径：

- `Modules/system/kernel/task_message_session_protocol.cppm`
- `Examples/kernel/runtime_task_message_session_protocol_host`
- `Examples/kernel/runtime_task_message_session_protocol_schema_host`
- `Examples/kernel/runtime_task_message_session_service_loop_host`
- `Examples/kernel/runtime_task_message_session_roundtrip_host`

## 一句话版本

- `task_message_session_endpoint` 继续负责把 accepted channel 上下文收成稳定 `endpoint` 视图
- `task_message_session_protocol` 只在这层之上再补一个 `operation -> request handler entry` 的薄表面
- close 不再要求每个 endpoint target 手写一份 `close(...)` 样板；它可以绑定一个显式 close hook，也可以退回默认 close ack

也就是说，这层解决的是：

“accepted session 已经落到某个 endpoint 以后，怎样把 `request.operation` 从裸数值收成一张最小协议表，并且不把 protocol 逻辑继续塞回 acceptor / dispatcher / demo handler 里？”

而不是：

“现在就把 session server 一次性长成完整 protocol framework。”

## 当前模块形状

当前模块导出：

- `TaskMessageSessionProtocolRequestHandler`
- `TaskMessageSessionProtocolCloseHandler`
- `TaskMessageSessionProtocolEntry`
- `TaskMessageSessionProtocolLookup`
- `TaskMessageSessionProtocolDispatchResult`
- `TaskMessageSessionProtocolTraceEvent`
- `TaskMessageSessionProtocolTraceBuffer<Capacity>`
- `TaskMessageSessionProtocol<Capacity, TraceBuffer>`
- `make_task_message_session_protocol_request_handler(...)`
- `make_task_message_session_protocol_close_handler(...)`
- `task_message_session_protocol_entry(...)`
- `make_task_message_session_protocol(...)`

它同时转导出：

- `kernel.task_message_session_endpoint`

因此 server-side 只要 `import kernel.task_message_session_protocol`，就能一次拿到：

- accepted channel / endpoint seam
- 协议 entry 表
- close hook 绑定
- protocol trace

## 当前语义

### 1) request 只做 operation lookup，不改写 endpoint 语义

`TaskMessageSessionProtocol` 在 request 路径上只做三件事：

- 用 `request.operation` 查找 `TaskMessageSessionProtocolEntry`
- 命中后把 `TaskMessageSessionEndpointRequestView` 原样交给 entry handler
- 把 unsupported operation、matched-but-unbound handler 等边界稳定收成 trace/result

它不会重新定义：

- `service_id`
- `session_handle`
- `open_payload`
- `channel_slot`

这些语义依然来自更下层的 endpoint / acceptor / dispatch seam。

### 2) entry handler 只要求一个最小 `dispatch(...)`

`make_task_message_session_protocol_request_handler(target)` 要求 target 实现：

- `dispatch(TaskMessageSessionEndpointRequestView)`

这意味着每个 operation handler 拿到的仍然是完整 endpoint 上下文，而不是被切碎的参数包。

### 3) close hook 是可选的

`TaskMessageSessionProtocol` 的 close 路径支持两种形态：

- 绑定显式 close hook：`make_task_message_session_protocol_close_handler(target)`
- 不绑定 close hook：默认返回 `handled(0)`

这让最小协议表可以先专注在 request operation 路由本身，而不强迫每个最小 server demo 先设计一整套 close 结果语义。

### 4) protocol 本体直接实现 endpoint seam

`TaskMessageSessionProtocol` 自己就实现了：

- `request(TaskMessageSessionEndpointRequestView)`
- `close(TaskMessageSessionEndpointCloseView)`

因此它可以被直接：

- `bind_task_message_session_endpoint(binding, protocol)`
- `make_task_message_session_endpoint_handler(protocol)`

挂回现有 accepted channel / endpoint 绑定点，而不需要再写一层中转 wrapper。

## 当前 live 证据

`Examples/kernel/runtime_task_message_session_protocol_host` 至少证明：

1. protocol table
   - `operation -> entry handler` lookup
   - unsupported operation
   - matched-but-unbound entry
   - default close 与显式 close hook
2. endpoint bridge
   - protocol 可以直接经由 `bind_task_message_session_endpoint(...)` 变成既有 channel handler
3. dispatcher integration
   - protocol 可以经由 `task_message_session_service_acceptor_entry(...)` 挂回 `TaskMessageSessionDispatcher`

`Examples/kernel/runtime_task_message_session_roundtrip_host` 现在进一步证明：

- 这层 protocol surface 不只是 synthetic host table
- 它已经进入真正的 message-backed session roundtrip live seam
- client `open / request / close` 可以穿过 `session dispatch -> session acceptor -> session endpoint -> session protocol`
- request reply、close reply 与 unsupported open 错误都能稳定回到 client completion

`Examples/kernel/runtime_task_message_session_protocol_schema_host`
和 `Examples/kernel/runtime_task_message_session_service_loop_host`
现在进一步证明：

- protocol surface 上面已经长出一层很薄的 schema/typed facade
- server handler 可以在 live seam 中看到稳定的
  `operation_name / payload field / result_name`

## 当前没有证明什么

当前这层仍然不处理：

- 多 operation schema versioning
- 多 session / 多 client 公平性
- 阻塞式 RPC / future facade
- service discovery / channel registry
- user/kernel ABI
- 真实 arch ingress / ARMv7-A leaf frame

所以它的定位仍然是：

- endpoint 之上的最小 protocol surface
- 不是完整 session server framework

## 下一跳建议

这条 seam 之后更自然的上行方向是：

1. 继续收一层更明确的 operation family / channel ownership 语义
2. 再决定是否需要真正的 payload schema / RPC facade
3. 继续避免把 `TaskMessageSessionProtocol` 本体膨胀成完整 framework

而不是继续让 `TaskMessageSessionDispatcher`、`TaskMessageSessionServiceAcceptor` 或具体 demo handler 自己长样板代码。
