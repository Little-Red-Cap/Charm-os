# 最小内核 task message syscall frame caller 契约（草案）

这份文档用于把 client 侧的 `syscall request -> message frame publish -> reply completion` 胶水收成一条单独 seam。

它对应当前新增的：
- `Modules/system/kernel/task_message_syscall_frame_caller.cppm`

目标不是现在就发明“同步阻塞的 message-backed syscall ABI”，而是先把上半层 client 侧最小、最稳定、最可验证的 glue 固定下来。

## 一句话版本

- caller 侧先把 `TaskSyscallRequest` 构造成 frame
- 再把 frame 发布到共享 frame store
- 然后用固定 label + token 发出 message request，并挂起 reply waiter
- reply 到来后，按 `(owner, token)` 取回 frame，并把 frame 中的 writeback 结果重新投影成 `TrapResult`

也就是说，这一层解决的是：

“client 端怎样以稳定、可回归的方式驱动 `task_message_syscall_frame` transport”

而不是：

“message runtime 已经提供了一个同步、阻塞、最终形态的 remote syscall API”

## 当前模块形状

当前模块导出：
- `TaskMessageSyscallFrameCallAdapter<Frame>`
- `task_message_syscall_frame_call_adapter_ready(...)`
- `TaskMessageSyscallFrameCallerState`
- `task_message_syscall_frame_caller_pending(...)`
- `TaskMessageSyscallFrameCaller<...>`
- `make_task_message_syscall_frame_caller(...)`

## 当前 caller 语义

当前 caller 的最小语义是：

1. 一次只允许一个 pending call
2. `begin(...)` 成功后，store 中必然存在与当前 `(owner, token)` 对应的 frame
3. `receive_reply(...)` 成功时，会取回该 frame，并通过 adapter 把结果收回 `TrapResult`
4. `consume_reply_timeout(...)` 成功时，会清掉 pending 状态并擦除对应 frame
5. caller 不负责 server 侧 dispatch，也不负责发明新的 reply schema

换句话说，当前 caller 只收口三件事：

- request frame 怎么发布
- message request 怎么挂 token / sequence
- reply / timeout 回来后 client 怎么完成这次调用

## 为什么要单独切这一层

当前我们已经有：

- server 侧 `task_message_syscall_frame` transport
- 下游 `task_syscall_frame -> task_syscall_table -> dispatch`

但 client 侧如果没有单独 seam，就会在每个 verifier 里反复手写：

- 构 frame
- publish frame
- arm reply wait
- send tokenized request
- 收 reply
- take frame
- 把 writeback 还原成 `TrapResult`

这会让“语义已经稳定”与“example 临时 glue”混在一起。

caller 层的价值，就是把这段重复 glue 从 example 里拿出来。

## 当前 live 证据路径

当前与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_syscall_frame_caller_host`
- `Examples/kernel/runtime_task_message_syscall_frame_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_syscall_frame_caller_host` 当前证明：

- client 侧可以按顺序发起 `yield / sleep_until / capability_call / invalid syscall`
- 四次调用都经由 caller -> message frame transport -> syscall frame bridge -> table 完成
- `capability_call` 的多参数仍能完整穿过 transport
- invalid syscall 会作为“reply 已到达，但语义失败”为 `unsupported_service` 回到 client
- missing token 仍然保持 reply timeout 路径，不会被伪装成成功返回

## 当前非目标

当前 caller 层仍然不处理：

- 真正同步阻塞的 remote syscall facade
- 多 outstanding call 并发公平性
- 多 server / service discovery
- 跨地址空间 transport
- user/kernel ABI

后续如果继续往上长，更健康的方向是：

- 在这层之上再长一个更高层的 remote syscall / RPC facade
- 而不是把 `TaskMessageSyscallFrameCaller` 本身直接扩成最终 API

这样我们能继续保持：

- transport glue
- syscall 语义
- 更高层 remote facade

三层职责各自清楚。
