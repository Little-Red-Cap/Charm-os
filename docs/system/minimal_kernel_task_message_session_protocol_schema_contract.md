# 最小内核 task message session protocol schema 契约（草案）

这份文档描述的不是一个新的“大框架”，而是在
`task_message_session_protocol` 之上新增的一层很薄的 typed/schema facade：

- `kernel.task_message_session_protocol_schema`

对应当前新增的模块与证据路径：

- `Modules/system/kernel/task_message_session_protocol_schema.cppm`
- `Examples/kernel/runtime_task_message_session_protocol_schema_host`
- `Examples/kernel/runtime_task_message_session_service_loop_host`
- `Examples/kernel/runtime_task_message_session_roundtrip_host`

## 一句话版本

- `task_message_session_protocol` 继续负责 `operation -> handler` lookup、close hook 和 protocol trace
- `task_message_session_protocol_schema` 只补一层稳定的 operation descriptor：
  - `operation_name`
  - `view_kind`
  - `payload field name`
  - `result_name`
- server handler 可以继续直接拿 raw `TaskMessageSessionEndpointRequestView`
- 也可以经由 `TaskMessageSessionProtocolSchemaBinding` 拿到
  `TaskMessageSessionProtocolSemanticProjection`

## 当前模块形状

- `TaskMessageSessionProtocolSchemaViewKind`
- `TaskMessageSessionProtocolSchemaEntry`
- `TaskMessageSessionProtocolSchemaLookup`
- `TaskMessageSessionProtocolSemanticField`
- `TaskMessageSessionProtocolSemanticProjection`
- `TaskMessageSessionProtocolSchemaCatalog<Capacity>`
- `TaskMessageSessionProtocolSchemaHandler`
- `TaskMessageSessionProtocolSchemaBinding`
- `task_message_session_protocol_schema_entry(...)`
- `task_message_session_protocol_schema_opaque_entry(...)`
- `task_message_session_protocol_semantic_projection(...)`
- `make_task_message_session_protocol_schema_catalog(...)`
- `make_task_message_session_protocol_schema_handler(...)`
- `make_task_message_session_protocol_schema_binding(...)`
- `task_message_session_protocol_entry(binding)`

## 当前语义

### 1) schema 只声明 operation 语义，不重写 lower context

`TaskMessageSessionProtocolSchemaEntry` 只声明：

- `operation`
- `operation_name`
- `view_kind`
- `field_names`
- `result_name`

它不重新定义：

- `service_id`
- `session_handle`
- `open_payload`
- `channel_slot`

这些上下文仍然来自 endpoint / acceptor / dispatch seam。

### 2) typed projection 仍然保留 raw request 的数值形态

`TaskMessageSessionProtocolSemanticProjection` 只是把
`TaskMessageSessionEndpointRequestView` 向上投影成：

- `endpoint`
- `operation`
- `payload`
- `descriptor`
- `fields`
- `result_name`

所以 handler 既能继续看 raw `operation/payload` 数值，
也能同时拿到稳定的 schema 名字。

### 3) binding 只是把 schema handler 桥回现有 protocol entry

`TaskMessageSessionProtocolSchemaBinding` 的定位很窄：

- 持有一个 `schema`
- 持有一个 typed handler
- 把 typed handler 适配回现有 `TaskMessageSessionProtocolEntry`

这层不会重写 `TaskMessageSessionProtocol`，
而是显式复用现有的 protocol table / trace / close hook 语义。

### 4) unmapped operation 仍然允许退回 raw / opaque 语义

`TaskMessageSessionProtocolSchemaCatalog::describe(operation)` 在未命中时会返回
`opaque/unmapped` descriptor。

这让 schema 层可以明确表达：

- 哪些 operation 已经有稳定名字和字段语义
- 哪些 operation 仍然只是 raw/opaque 数值

## 当前 live 证据

`Examples/kernel/runtime_task_message_session_protocol_schema_host`
至少证明：

1. schema catalog
   - `operation -> schema entry` lookup
   - `unmapped -> opaque descriptor`
2. semantic projection
   - raw request 可以稳定投影成
     `operation_name / payload field / result_name`
3. binding bridge
   - typed handler 可以经由
     `TaskMessageSessionProtocolSchemaBinding`
     直接桥回现有 `task_message_session_protocol`
4. unbound binding
   - schema binding 未绑定 handler 时仍然稳定返回
     `unbound_adapter`

`Examples/kernel/runtime_task_message_session_service_loop_host`
和 `Examples/kernel/runtime_task_message_session_roundtrip_host`
现在进一步证明：

- live seam 已经走到
  `session_endpoint -> session_protocol_schema -> session_protocol`
- server handler 能在真实 service loop / roundtrip 中看到稳定的
  `operation_name / payload field / result_name`

## 当前没有证明什么

当前这层仍然不处理：

- payload serialization / codec
- version negotiation
- 多参数 request decoding
- 多 session / 多 client 公平性
- 阻塞式 RPC / future facade
- service discovery / channel registry
- user/kernel ABI
- 真实 arch ingress / ARMv7-A leaf frame

## 下一跳建议

1. 继续收一层 server-side session channel facade / operation family grouping
2. 等 operation family 稳定以后，再决定是否需要真正的 payload codec / RPC schema
3. 避免过早把 schema 层膨胀成完整 serialization framework
