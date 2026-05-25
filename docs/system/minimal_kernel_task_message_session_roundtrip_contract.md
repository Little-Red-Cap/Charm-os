# 最小内核 task message session roundtrip 证据契约（草案）

这份文档描述的不是一个新的公共模块，而是一条新的组合证据 seam：

- `kernel.task_message_session_api`
- `kernel.task_message_runtime_service`
- `kernel.task_message_session_acceptor`
- `kernel.task_message_session_endpoint`
- `kernel.task_message_session_protocol_schema`
- `kernel.task_message_session_protocol`
- `kernel.task_message_session_service`
- `kernel.task_message_session_dispatch`
- `kernel.task_message_session_roundtrip`

第一次被焊成了一条真实可跑的 client/server roundtrip。

对应 live verifier：

- `Examples/kernel/runtime_task_message_session_roundtrip_host`

当前 roundtrip semantic witness 已经从 host-local helper 提升为：

- `Modules/system/kernel/task_message_session_roundtrip.cppm`
- `TaskMessageSessionRoundtripWitness`
- `task_message_session_roundtrip_witness(...)`

## 一句话版本

- client 用 `task_message_session_api` 发起 `open / request / close`
- 中间继续走现有 `task_message_runtime_service -> syscall pump -> syscall client -> frame caller`
- server 侧通过 `task_message_syscall_frame -> task_syscall_table -> task_message_session_dispatch -> task_message_session_service -> task_message_session_acceptor -> task_message_session_endpoint -> task_message_session_protocol_schema -> task_message_session_protocol` 接住
- handler 的 reply 再沿原路回到 client completion

这意味着我们现在已经不只是分别拥有 client 侧 session facade 和 server 侧 session ingress，
而是已经把这两端通过现有 message runtime 焊成了同一条 live 闭环。

## 当前证明了什么

当前 `runtime_task_message_session_roundtrip_host` 证明了：

1. `open`
   - client 发起 `service_id + payload`
   - server 侧 `task_message_session_dispatch` 正常分配 `session_handle`
   - server-side `task_message_session_service` 把 `dispatch + acceptor + pump inspection` 收成一条 ownership seam
   - 这条 seam 再经由 `task_message_session_acceptor -> task_message_session_endpoint -> task_message_session_protocol_schema -> task_message_session_protocol` 把这次 open 收成 accepted channel / typed endpoint surface
   - `open reply -> session_handle` 能穿过整条 message runtime 返回到 client
2. `request`
   - client 继续用返回的 `session_handle` 发起 request
   - server 侧能稳定命中 active session slot
   - accepted channel handler 现在能稳定拿到 endpoint facade 上的 `service_id / session_handle / open_payload / channel_slot`
   - handler 的 reply value 能稳定回到 client completion
3. `close`
   - close reason 能稳定到达 server handler
   - close 成功后 session slot 与 accepted channel slot 都被回收
   - client phase 回到 `idle`
4. `unsupported open`
   - 未注册 `service_id` 的 unsupported 错误
   - 能从 server-side dispatch 一直回传到 client completion
   - 不会伪装成 session opened
5. 运行时收口
   - verifier 结束时 `active_sessions == 0`
   - mailbox / frame store / syscall pump 都回到空闲
6. semantic witness seam
   - dispatch / acceptor / protocol / service / pump 五段 witness 必须同时 standing
   - handoff target 必须可用
   - request path 的 `service_id / session_handle / operation / payload / reply_value / channel_slot` 必须一致

## 当前组合路径

当前 roundtrip seam 的最小组合路径是：

1. client:
   - `kernel.task_message_session_api`
   - `kernel.task_message_syscall_api`
   - `kernel.task_message_runtime_api`
   - `kernel.task_message_runtime_service`
2. transport:
   - `kernel.task_message_syscall_pump`
   - `kernel.task_message_syscall_client`
   - `kernel.task_message_syscall_frame_caller`
   - `kernel.task_message_syscall_frame`
3. server:
   - `kernel.task_syscall_table`
   - `kernel.task_message_session_dispatch`
   - `kernel.task_message_session_service`
   - `kernel.task_message_session_acceptor`
   - `kernel.task_message_session_endpoint`
   - `kernel.task_message_session_protocol_schema`
   - `kernel.task_message_session_protocol`
4. witness:
   - `kernel.task_message_session_roundtrip`

这条链没有另起 transport，而是明确复用了既有的 message/syscall 证据路径。

## 当前没有证明什么

这一条 roundtrip seam 当前仍然没有证明：

- 多 session 并发
- 多 client 公平性
- 阻塞式 RPC facade
- channel registry / endpoint routing
- user/kernel ABI
- 真实 arch ingress / ARMv7-A leaf frame

所以它的定位仍然是：

- 上半层 session client/server 语义已经能带着 accepted channel 闭环
- 但还没有长成完整的 session server framework

## 当前 evidence path

- verifier:
  - `Examples/kernel/runtime_task_message_session_roundtrip_host`
- full smoke:
  - `scripts/minimal_kernel_runtime_host_smoke.ps1`
- semantic witness ladder:
  - `scripts/semantic_witness_ladder_smoke.ps1`
  - [`minimal_kernel_semantic_witness_ladder_smoke_contract.md`](minimal_kernel_semantic_witness_ladder_smoke_contract.md)

当前 roundtrip verifier 至少覆盖这几类可观察证据：

- client completion 序列
- server-side session dispatch trace
- client-side syscall pump trace
- 最终 runtime/mailbox/frame/pump 清空状态

## 下一跳建议

这条 seam 之后更自然的上行方向是：

1. message-backed session service loop ownership
2. 把 session service loop 也切到 message session protocol surface / endpoint ownership
3. 更真实的 channel routing / endpoint ownership

而不是继续把 `TaskMessageSessionDispatcher` 本体膨胀成完整框架。
