# 最小内核 task message table 契约（草案）

这份文档用于把 server-side 的 `label -> handler` 路由，收成 `TaskMessageApi` 之上的一条独立边界。

它对应当前新增的：

- `Modules/system/kernel/task_message_table.cppm`

目标不是立即定义完整 IPC catalog，也不是把 mailbox transport 和 service discovery 混成一层，而是先给 server task 一个稳定、薄、可延后绑定的消息路由面。

## 一句话版本

- `kernel.task_message_api` 负责把当前任务的 message entry points 收成 task-facing surface。
- `kernel.task_message_table` 负责把 server 侧收到的 request，按 `label` 稳定分发到 handler。

如果说前一层证明的是“当前任务已经能用统一名字收发消息”，那么这一层证明的是“server 任务已经不必手写 `switch(request.label)`，而可以通过一张稳定的表来接消息”。

## 为什么单独收这一层

当前 mailbox 线上已经有：

- `RuntimeMailbox`
- `TaskMessageApi`

它们分别解决：

- transport / queue / timeout / wakeup
- current-task 的 message naming

但它们还没有单独收出一层“server 如何按 label 路由 request”的契约。

如果不补这层，未来每个 service task 都会倾向于在 task body 里手写：

- `receive(request)`
- `switch(request.label)`
- `reply(request, value)`

这会让：

- label routing 语义散在各个 task 里
- late bind / missing entry / unbound handler 的边界难以统一验证
- 后续往 service task / IPC protocol 长时缺少一个稳定的中间 seam

## 模块位置与核心类型

模块位置：

- `Modules/system/kernel/task_message_table.cppm`

当前核心类型：

- `TaskMessageHandleResult`
- `TaskMessageHandler`
- `TaskMessageHandlerEntry`
- `TaskMessageTableLookup`
- `TaskMessageTableTraceEvent`
- `TaskMessageTable<Capacity, TraceBuffer>`
- `TaskMessageTableTraceBuffer<Capacity>`

配套 helper：

- `handled_task_message(...)`
- `unhandled_task_message()`
- `make_task_message_handler(target)`
- `task_message_handler_entry(...)`
- `make_task_message_table(...)`
- `task_message_request_from_trace_event(...)`

## 当前 API 形状

`TaskMessageTable<...>` 当前暴露：

- `capacity()`
- `bind_trace(...)`
- `bind_entry(index, entry)`
- `entry(index)`
- `lookup(label)`
- `dispatch(request)`
- `dispatch(from, label, value, sequence=0)`

其中 `dispatch(...)` 当前返回的是 `TaskMessageHandleResult`：

- `handled`
- `reply_value`

也就是说，这层当前只回答两件事：

- 这条 request 有没有被某个 handler 接住
- 如果接住了，建议回什么 reply value

## 当前语义边界

### 1) 这层不拥有 mailbox transport

`TaskMessageTable` 当前只做 label routing。

它不直接处理：

- `receive(...)`
- `wait_receive_until(...)`
- `consume_receive_timeout(...)`
- `reply(...)`

这些仍然属于：

- `RuntimeMailbox`
- `TaskMessageApi`

也就是说，当前更健康的组合方式是：

- 先 `TaskMessageApi::receive(request)`
- 再 `TaskMessageTable::dispatch(request)`
- 最后由调用者决定是否 `reply(request, reply_value)`

### 2) missing entry 和 unbound handler 当前都不会伪装成“已处理”

如果：

- `label` 没命中任何 entry
- 命中了 entry，但 handler 还没绑定

那么当前 `dispatch(...)` 都会返回 `handled == false`。

区别会体现在 trace 里：

- `matched`
- `handler_valid`

这样我们既能保留一个很薄的运行时结果面，也能在 verifier 里稳定观察边界。

### 3) 这层允许 late bind

`bind_entry(...)` 当前刻意保留。

这意味着：

- 可以先把 table 壳子搭起来
- 再把具体 handler 在后续阶段绑定进去

这条语义对于我们现在的最小内核推进很重要，因为它允许：

- 先收口 label routing seam
- 后面再逐步接入真实 service task 或更高层 protocol

### 4) 这层不提前定义 catalog / discovery

当前 entry 只关心：

- `label`
- `label_name`
- `handler`

它还不处理：

- endpoint naming
- service discovery
- capability routing
- 多 server 命名空间
- 更高层 message schema

## 与现有上半层的关系

当前更推荐把 mailbox 线理解成：

1. `kernel.runtime_mailbox`
   - transport / queue / timeout / wakeup
2. `kernel.task_message_api`
   - current-task message entry points
3. `kernel.task_message_table`
   - server-side `label -> handler` routing
4. `kernel.task_message_dispatch`
   - `receive -> dispatch -> reply` bridge

这三层现在分别证明：

- 任务之间的 stateful object 语义
- 当前任务如何用统一名字收发消息
- server 如何把 request 稳定分发到 handler

如果下一步要继续长最小 server-side bridge，当前更推荐在这层之上新增独立的：

- `kernel.task_message_dispatch`

这比让 `TaskMessageApi` 直接长成一个“大而全 dispatcher facade”更干净。

## 当前证据路径

当前与这层直接相关的证据路径有两条：

- `Examples/kernel/runtime_task_message_table_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

它们当前分别承担：

- `runtime_task_message_table_host`
  - 独立验证 `lookup(...)`、handler dispatch、late bind、missing entry 与 unbound handler 的边界
- `minimal_kernel_runtime_host_smoke.ps1`
  - 把这层 verifier 纳入现有 runtime host smoke，避免它脱离 mailbox/runtime 证据网

## 当前非目标

当前这层仍然不处理：

- 自动 reply 发送
- 真实 service task loop 装配
- message catalog / schema projection
- trap/syscall ingress
- user/kernel ABI

当前最重要的不是把消息服务框架一次做满，而是先把“server-side label routing”收成一个稳定 seam，让后面长 service task 时不必回到 open-coded `switch(label)`。
