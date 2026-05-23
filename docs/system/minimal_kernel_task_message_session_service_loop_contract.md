# 最小内核 task message session service loop 证据契约（草案）

这份文档描述的不是一个新的公共模块，而是一条新的组合证据 seam：

- `kernel.task_message_session_service`
- `kernel.task_message_syscall_api`
- `kernel.task_message_runtime_service`
- `kernel.task_message_session_acceptor`
- `kernel.task_message_session_endpoint`
- `kernel.task_message_session_protocol_schema`
- `kernel.task_message_session_protocol`
- `kernel.task_message_session_dispatch`
- `kernel.task_message_session_service_loop`

它们第一次被焊成了一条“server-side session ownership 由真实 service task loop 持有”的 live 路径。

对应 live verifier：

- `Examples/kernel/runtime_task_message_session_service_loop_host`

当前 service-loop semantic witness 已经从 host-local 检查提升为：

- `Modules/system/kernel/task_message_session_service_loop.cppm`
- `TaskMessageSessionServiceLoopWitness`
- `task_message_session_service_loop_witness(...)`

## 一句话版本

- client 侧故意不走 `task_message_session_api`
- 而是直接用 `task_message_syscall_api.sys_capability_call(...)` 发起 `open / request / close`
- 中间继续复用既有 `task_message_runtime_service -> syscall pump -> syscall client -> frame caller`
- server 侧则通过 `task_message_syscall_frame -> task_syscall_table -> task_message_session_dispatch -> task_message_session_service -> task_message_session_acceptor -> task_message_session_endpoint -> task_message_session_protocol_schema -> task_message_session_protocol` 接住

这意味着我们现在已经有一条更聚焦的 live 证据，单独证明：

- `TaskMessageSessionService` 不只是一个静态 facade
- 它已经能真正持有 server task body 的 wait/timeout/drain/trace ownership
- 同时又不需要借助更高层 `TaskMessageSessionApi` 才能成立

## 当前证明了什么

当前 `runtime_task_message_session_service_loop_host` 证明了：

1. server-side ownership
   - `bootstrap -> timeout -> queue-empty rearm`
   - 都能经由同一条 `task_message_session_service.step(...)` seam 被观察
   - service trace 稳定记录 `reason / event / due / budget / served / active sessions / active channels`
2. raw syscall caller
   - client 只用 `sys_capability_call(...)`
   - 仍能稳定发起 `open / request / close / unsupported open`
   - 说明 server-side session loop ownership 与更高层 session client facade 解耦
3. accepted channel lifecycle
   - open 后能稳定建立 accepted channel
   - accepted channel 的上下文能进一步收成稳定 endpoint facade
   - request 能命中 active session/channel
   - close 后 session slot 与 channel slot 都会回收
4. full reply path
   - `open -> session_handle`
   - `request -> reply value`
   - `close -> close reply`
   - `unsupported open -> unsupported_service`
   - 都能稳定穿过 message runtime 回到 client completion
5. runtime cleanup
   - verifier 结束时 mailbox / frame store / syscall pump / runtime service 都回到空闲
   - `session_service.active_sessions() == 0`
   - `session_service.active_channels() == 0`
6. semantic witness seam
   - bootstrap / timeout / open dispatch / open service / request roundtrip / close dispatch / close service / ghost dispatch / ghost service 九段 witness 必须同时 standing
   - handoff target 必须可用
   - ownership path 必须证明 `service_id / session_handle / reply_value / active session/channel` 在 open、request、close、unsupported open 路径上一致

## 当前组合路径

当前这条 service-loop seam 的最小组合路径是：

1. client：
   - `kernel.task_message_syscall_api`
   - `kernel.task_message_runtime_api`
   - `kernel.task_message_runtime_service`
2. transport：
   - `kernel.task_message_syscall_pump`
   - `kernel.task_message_syscall_client`
   - `kernel.task_message_syscall_frame_caller`
   - `kernel.task_message_syscall_frame`
3. server：
   - `kernel.task_syscall_table`
   - `kernel.task_message_session_dispatch`
   - `kernel.task_message_session_service`
   - `kernel.task_message_session_acceptor`
   - `kernel.task_message_session_endpoint`
   - `kernel.task_message_session_protocol_schema`
   - `kernel.task_message_session_protocol`
4. witness：
   - `kernel.task_message_session_service_loop`
   - `kernel.task_message_session_roundtrip`

和 `runtime_task_message_session_roundtrip_host` 的区别是：

- roundtrip 更强调“client session facade + server accepted channel” 两端焊通
- service loop 更强调“server task body 的 ownership 已经真实落到 session_service”，并且 accepted channel 已经能通过 endpoint facade 表达

## 当前没有证明什么

这一条 seam 当前仍然没有证明：

- `TaskMessageSessionApi` 本身的 client 语义
- 多 session 并发
- 多 client 公平性
- 阻塞式 RPC facade
- 更真实的 channel registry / endpoint routing
- user/kernel ABI
- 真实 arch ingress / ARMv7-A leaf frame

所以它的定位仍然是：

- 第一条专门证明 server-side session ownership loop 的 live 证据
- 不是完整 session framework，也不是最终 protocol surface

## 当前 evidence path

- verifier：
  - `Examples/kernel/runtime_task_message_session_service_loop_host`
- full smoke：
  - `scripts/minimal_kernel_runtime_host_smoke.ps1`
- semantic witness ladder：
  - `scripts/semantic_witness_ladder_smoke.ps1`
  - [`minimal_kernel_semantic_witness_ladder_smoke_contract.md`](minimal_kernel_semantic_witness_ladder_smoke_contract.md)

当前 verifier 至少覆盖这几类可观察证据：

- server-side session service trace
- session dispatch trace
- session acceptor trace
- client-side syscall pump trace
- 最终 runtime/mailbox/frame/pump 清空状态

## 下一跳建议

这条 seam 之后更自然的上行方向是：

1. 继续把 message-backed session protocol surface 沿 service loop / roundtrip 两条 live seam 收成同一张 server-side 脸
2. 继续收清 server-side channel ownership / endpoint routing
3. 再讨论更完整的 session server framework

而不是回头把 `TaskMessageSessionDispatcher` 或 `TaskMessageSessionService` 膨胀成全能类型。
