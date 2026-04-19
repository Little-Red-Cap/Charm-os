# 最小内核 task message dispatch 契约（草案）

这份文档用于把 `TaskMessageApi` 与 `TaskMessageTable` 之间的 server-side dispatch/reply bridge，收成一条独立边界。

它对应当前新增的：

- `Modules/system/kernel/task_message_dispatch.cppm`

目标不是提前定义完整 message server framework，而是先把“收到 request 之后如何路由并自动 reply”的最小闭环收成一个可验证 seam。

## 一句话版本

- `kernel.task_message_api` 负责 current-task 的 `receive(...)` / `reply(...)` 入口。
- `kernel.task_message_table` 负责 `label -> handler` 路由。
- `kernel.task_message_dispatch` 负责把这两层接起来，形成 `receive -> dispatch -> reply` 的最小 server-side bridge。

如果说前一层证明的是“server 已经可以按 label 稳定分发 request”，那么这一层证明的是“server 现在也可以通过一条独立 bridge，把 request 吃进去并把 reply 送回去”。

## 为什么单独收这一层

如果只有：

- `TaskMessageApi`
- `TaskMessageTable`

那么 server task 仍然要在 task body 里手写：

- `receive(request)`
- `table.dispatch(request)`
- `reply(request, value)`

这比 open-coded `switch(label)` 已经好很多，但仍然会让：

- request consume / handler route / reply send 的边界散在调用点
- handled 但 reply 失败的语义缺少统一落点
- 后续 service task loop 难以建立稳定的 host 证据

因此当前更健康的做法是把这条桥单独收成：

- `TaskMessageDispatcher`

## 模块位置与核心类型

模块位置：

- `Modules/system/kernel/task_message_dispatch.cppm`

当前核心类型：

- `TaskMessageDispatchResult`
- `TaskMessageDispatchTraceEvent`
- `TaskMessageDispatchTraceBuffer<Capacity>`
- `TaskMessageDispatcher<Messages, Table, TraceBuffer>`

配套 helper：

- `make_task_message_dispatcher(...)`
- `task_message_request_from_trace_event(...)`

## 当前 API 形状

`TaskMessageDispatcher<...>` 当前暴露：

- `valid()`
- `messages()`
- `table()`
- `bind_messages(...)`
- `bind_table(...)`
- `bind_trace(...)`
- `dispatch_request(request)`
- `serve_once()`

其中：

- `dispatch_request(request)` 用于“request 已在手里，做路由并尝试 reply”
- `serve_once()` 用于“先从 `TaskMessageApi` 收一条 request，再走一遍 dispatch/reply”

返回值 `TaskMessageDispatchResult` 当前收口：

- `accepted`
- `matched`
- `handler_valid`
- `handled`
- `replied`
- `request`
- `reply_value`

## 当前语义边界

### 1) 这层不重写 table 的 label routing

`TaskMessageDispatcher` 当前不重新定义：

- lookup 规则
- missing entry 规则
- unbound handler 规则

这些仍由：

- `TaskMessageTable`

定义。

这层只是把 table 的 `handled/unhandled` 结果，与 message-side `reply(...)` 接起来。

### 2) `handled == true` 不等于 `replied == true`

这是这层最重要的边界之一。

如果：

- handler 已经命中并返回 `handled`
- 但底层 message surface 无法成功 `reply(...)`

那么当前结果会表现为：

- `handled == true`
- `replied == false`

这让“业务处理成功”和“reply transport 成功”能够被独立观察，而不是混成一个模糊的 bool。

### 3) `serve_once()` 只收一条 request

当前 `serve_once()` 的目标很克制：

- 最多收一条 request
- 最多完成一次 dispatch/reply

它当前不处理：

- 长循环
- wait policy
- timeout policy
- batch drain

这使得它非常适合 host verifier，也适合作为未来 service task loop 的最小 building block。

### 4) 这层不拥有 service lifecycle

`TaskMessageDispatcher` 当前不处理：

- server bootstrap
- idle / tick / scheduler glue
- worker loop ownership

这些仍应留在更外层 runtime/demo/task body 里。

因此当前更推荐把这层理解为：

- message server 的一个 bridge seam

而不是：

- 一个完整 message server framework

## 与现有上半层的关系

当前更推荐把消息线理解成：

1. `kernel.runtime_mailbox`
   - transport / queue / timeout / wakeup
2. `kernel.task_message_api`
   - current-task message entry points
3. `kernel.task_message_table`
   - server-side `label -> handler` routing
4. `kernel.task_message_dispatch`
   - `receive -> dispatch -> reply` bridge

这四层现在分别证明：

- transport/object semantics
- current-task naming
- server-side label routing
- server-side dispatch/reply bridge

## 当前证据路径

当前与这层直接相关的证据路径有两条：

- `Examples/kernel/runtime_task_message_dispatch_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

它们当前分别承担：

- `runtime_task_message_dispatch_host`
  - 独立验证 direct dispatch、`serve_once()`、handled 但 reply 失败、以及 missing label 的边界
- `minimal_kernel_runtime_host_smoke.ps1`
  - 把这层纳入 runtime host smoke，保证它与现有消息/runtime 证据网保持对齐

## 当前非目标

当前这层仍然不处理：

- 多 request drain loop
- receive timeout / wait policy 装配
- service discovery
- higher-level message catalog/schema
- trap/syscall ingress
- user/kernel ABI

当前最重要的不是立刻造出完整 message server，而是先把“最小 server-side dispatch/reply bridge”收成一个稳定 seam，让后面长 service task loop 时有明确的落点。
