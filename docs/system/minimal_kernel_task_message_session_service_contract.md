# 最小内核 task message session service facade 契约（草案）

这份文档描述的不是对 `task_message_service_pump`、`task_message_session_dispatch` 或 `task_message_session_acceptor` 的重写，而是在它们之上新增的一层更贴近 server-side ownership 的薄 facade：
- `kernel.task_message_session_service`

对应当前新增的模块与证据路径：
- `Modules/system/kernel/task_message_session_service.cppm`
- `Examples/kernel/runtime_task_message_session_service_host`
- `Examples/kernel/runtime_task_message_session_service_loop_host`
- `Examples/kernel/runtime_task_message_session_roundtrip_host`

## 一句话版本

- `task_message_service_pump` 继续负责 `bootstrap / wait / timeout / drain / hold` 的编排。
- `task_message_session_dispatch` 继续负责 `service_id -> session_handle -> open / request / close` 的 ingress seam。
- `task_message_session_acceptor` 继续负责 `session_handle -> accepted channel` 的 service-local channel 生命周期。
- `task_message_session_service` 只把这三层焊成一个 server-side facade，让 service task body 不必直接抓裸 `service_pump` 和分散的 session/channel inspection API。

## 当前模块形状

当前模块导出：
- `TaskMessageSessionServiceTraceEvent`
- `TaskMessageSessionServiceTraceBuffer<Capacity>`
- `TaskMessageSessionServiceResult<RawPumpResult>`
- `TaskMessageSessionService<Pump, SessionDispatcher, SessionAcceptor, TraceBuffer>`
- `make_task_message_session_service(...)`

它同时转导出：
- `kernel.task_message_service_pump`
- `kernel.task_message_session_acceptor`

## 当前语义

### 1) facade 只做 ownership 收口，不重写下层语义

`TaskMessageSessionService` 内部只持有：
- 一个 `Pump`
- 一个 `SessionDispatcher*`
- 一个 `SessionAcceptor*`
- 一个可选 trace buffer

它不重新定义 `open / request / close`，也不绕开既有 `dispatcher / acceptor` 逻辑。

### 2) service task body 只需要面向一条 server-side seam

这一层对上暴露最小但完整的 service-side surface：
- `service_name()`
- `bootstrap_event()`
- `receive_event()`
- `receive_timeout_event()`
- `wait_receive_until(...)`
- `step(event, budget, due)`

同时保留最小 inspection：
- `active_sessions() / active_channels()`
- `session(...) / lookup_session(...)`
- `channel(...) / lookup_channel(...)`

### 3) `step(...)` 把 pump progress 和 session surface 合成一个结果

返回 `TaskMessageSessionServiceResult<RawPumpResult>`：
- 保留 `progressed / bootstrap_consumed / wait_armed / hold_ready / reason`
- 附带 `active_sessions / active_channels`
- 附带最近一次 dispatch 的 `accepted / handled / replied / reply_value`
- 保留原始 `raw` pump result，便于继续下钻到 drain / dispatch 细节

## 当前 live 证据

`Examples/kernel/runtime_task_message_session_service_host` 至少证明：

1. unbound facade 会稳定失败，不会伪装成可用 service。
2. `bootstrap -> open -> request -> close` 四段都能经由同一条 `step(...)` seam 被观察。
3. `service_name()`、session/channel lookup 与 active counters 可以直接从 facade 取回。
4. service trace 能稳定记录 pump reason、event、budget、reply 和 active session/channel 数。

`Examples/kernel/runtime_task_message_session_roundtrip_host` 现在进一步证明：
- server 侧不再直接抓裸 `TaskMessageServicePump`
- live roundtrip 已经改为 `service pump -> session service -> session dispatch -> session acceptor -> accepted channel` 的 weld

`Examples/kernel/runtime_task_message_session_service_loop_host` 现在进一步证明：
- server task body 已经能由 `task_message_session_service` 真实持有 wait/timeout/drain ownership
- 即便 client 侧只走 raw `sys_capability_call(...)`，server-side session service loop 仍能稳定闭环

## 当前没有证明什么

当前这层仍然不处理：
- 真正的 service registry / discovery
- 多 service fairness
- 阻塞式 RPC facade
- 更高层 session protocol surface
- 真实 arch ingress
- user ABI

## 下一跳建议

这层之后更自然的上行方向是：

1. 把 message-backed server task body 固定收口到 `task_message_session_service`
2. 在这层之上补更明确的 server-side protocol / channel ownership
3. 用 `runtime_task_message_session_service_loop_host` 这一类 live verifier 继续把 ownership 证据焊实
4. 再讨论真正的 session server framework，而不是继续让 `dispatcher` 或 `acceptor` 各自膨胀
