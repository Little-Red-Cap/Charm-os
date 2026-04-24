# 最小内核 task message service loop 契约（草案）

这份文档用于把 server-side 的 `receive wait / timeout consume / dispatch-reply` 装配，收成 `TaskMessageDispatch` 之上的一条独立边界。

它对应当前新增的：

- `Modules/system/kernel/task_message_service_loop.cppm`

目标不是提前定义完整 message service framework，而是先把“server task 如何在真实 runtime loop 里等待消息、处理 timeout、再把 request 交给 dispatcher”的最小 service loop 收成一个可验证 seam。

## 一句话版本

- `kernel.task_message_dispatch` 负责单次 `receive -> dispatch -> reply`。
- `kernel.task_message_service_loop` 负责把这次单步 dispatch 放进 server-side 的等待循环里，形成 `wait -> timeout -> rearm -> dispatch` 的最小 service loop。

如果说前一层证明的是“server 已经能处理一条 request”，那么这一层证明的是“server 现在也已经能在 runtime loop 里稳定等待、超时、重挂等待，再吃进下一条 request”。

更上一层如果要表达“同一次 server 唤醒里最多继续吃多少条 request”，当前已经单独上移到：

- `kernel.task_message_service_drain`

如果要再把 server task body 里的 bootstrap/rearm/hold 样板收口，当前继续上移到：

- `kernel.task_message_service_pump`

## 为什么单独收这一层

如果只有：

- `TaskMessageApi`
- `TaskMessageTable`
- `TaskMessageDispatcher`

那么 server task 仍然要在 task body 里反复手写：

- `wait_receive_until(due)`
- `consume_receive_timeout(event)`
- `dispatcher.serve_once()`
- timeout 之后重新 `wait_receive_until(next_due)`

这会让：

- wait policy 和 dispatch 边界仍然散在 task body 里
- timeout consume / rearm 的语义缺少统一落点
- 后续 service task / protocol progress task 难以建立稳定证据

因此当前更健康的做法是把这层单独收成：

- `TaskMessageServiceLoop`

## 模块位置与核心类型

模块位置：

- `Modules/system/kernel/task_message_service_loop.cppm`

当前核心类型：

- `TaskMessageServiceLoopResult`
- `TaskMessageServiceLoopTraceKind`
- `TaskMessageServiceLoopTraceEvent`
- `TaskMessageServiceLoopTraceBuffer<Capacity>`
- `TaskMessageServiceLoop<Dispatcher, TraceBuffer>`

配套 helper：

- `task_message_service_loop_trace_kind_name(...)`
- `make_task_message_service_loop(...)`

## 当前 API 形状

`TaskMessageServiceLoop<...>` 当前暴露：

- `valid()`
- `dispatcher()`
- `bind_dispatcher(...)`
- `bind_trace(...)`
- `receive_event()`
- `receive_timeout_event()`
- `wait_receive_until(due)`
- `step(event)`
- `step_and_wait_until(event, due)`

其中：

- `wait_receive_until(due)` 用于 server 进入下一次 receive wait
- `step(event)` 用于消费一次 timeout 或一次 receive-ready event
- `step_and_wait_until(event, due)` 用于“若这次 event 是 timeout，则立刻 rearm 下一次 wait”

返回值 `TaskMessageServiceLoopResult` 当前收口：

- `progressed`
- `wait_armed`
- `timeout_consumed`
- `dispatch`

其中 `dispatch` 直接带着底层 `TaskMessageDispatchResult`。

## 当前语义边界

### 1) 这层不重写 dispatch 的单步语义

`TaskMessageServiceLoop` 当前不重新定义：

- handler routing
- handled/unhandled 规则
- reply success/failure 规则

这些仍由：

- `TaskMessageDispatcher`

定义。

这层只负责把：

- receive wait
- timeout consume
- timeout 后 rearm
- receive-ready event 上的单步 dispatch

装配成一个稳定的 service loop seam。

### 2) `step(event)` 仍然只推进一步

当前 `step(event)` 的目标很克制：

- 最多消费一次 receive-timeout
- 或最多完成一次 `dispatch/reply`

它当前不处理：

- 多 request drain
- batch serve
- 长时公平性
- server 生命周期所有权

这使得它非常适合成为 service task body 的最小 building block。

budgeted multi-request drain 则应交给更上一层的：

- `TaskMessageServiceDrain`

### 3) rearm 语义是显式的

`step_and_wait_until(event, due)` 当前只在 timeout 被成功消费后才会尝试 rearm。

这意味着：

- timeout consume 和下一次 wait arm 是可观察的两个连续动作
- rearm 失败不会被吞掉
- task body 不必自己重复写 timeout 后再挂 wait 的样板

### 4) 这层仍然不拥有 service discovery / protocol schema

`TaskMessageServiceLoop` 当前仍然不处理：

- service discovery
- endpoint naming
- message catalog/schema
- capability routing
- user/kernel ABI

它当前只是 server-side service loop seam，不是完整 message service framework。

## 与现有上半层的关系

当前更推荐把消息线理解成：

1. `kernel.runtime_mailbox`
   - transport / queue / timeout / wakeup
2. `kernel.task_message_api`
   - current-task message entry points
3. `kernel.task_message_table`
   - server-side `label -> handler` routing
4. `kernel.task_message_dispatch`
   - 单步 `receive -> dispatch -> reply`
5. `kernel.task_message_service_loop`
   - `wait -> timeout -> rearm -> dispatch`
6. `kernel.task_message_service_drain`
   - bounded `dispatch -> drain -> stop(boundary)`
7. `kernel.task_message_service_pump`
   - `bootstrap wait -> timeout rearm -> drain hold/rearm`

这七层现在分别证明：

- runtime object 语义
- current-task naming
- server-side label routing
- 单步 dispatch/reply bridge
- server-side 最小 service loop 装配
- 单次唤醒内的 bounded drain 边界
- service task body 级别的 bootstrap/rearm/hold orchestration

## 当前证据路径

当前与这层直接相关的证据路径有两条：

- `Examples/kernel/runtime_task_message_loop_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

它们当前分别承担：

- `runtime_task_message_loop_host`
  - 验证真实 `RuntimeBridge + RuntimeMailbox + TaskMessageServiceLoop` 已经能在 scheduler loop 里形成 `wait -> timeout -> rearm -> dispatch -> reply` 的 live 闭环
- `minimal_kernel_runtime_host_smoke.ps1`
  - 把这层纳入 runtime host smoke，保证它与现有 mailbox/message/runtime 证据网保持对齐

更上一层的 bounded drain 装配，当前已经单独上移到：

- `kernel.task_message_service_drain`

再往上的 service task body orchestration，当前继续上移到：

- `kernel.task_message_service_pump`

## 当前非目标

当前这层仍然不处理：

- budgeted multi-request drain
- bootstrap/rearm/hold orchestration
- starvation/fairness policy
- service discovery
- higher-level message catalog/schema
- trap/syscall ingress
- user/kernel ABI

当前最重要的不是立刻造出完整 service framework，而是先把“最小 server-side service loop”收成一个稳定 seam，让后面的 protocol progress task 有明确的落点。
