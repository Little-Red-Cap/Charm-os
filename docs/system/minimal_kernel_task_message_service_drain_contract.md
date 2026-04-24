# 最小内核 task message service drain 契约（草案）

这份文档用于把 server-side 的“单次唤醒内最多 drain 多少条 request”语义，从 `TaskMessageServiceLoop` 之上单独收成一条边界。

它对应当前新增的：

- `Modules/system/kernel/task_message_service_drain.cppm`

目标不是提前定义完整 message server scheduler，也不是提前固化跨唤醒公平性策略，而是先把“同一次 receive-ready 唤醒里，server 最多继续处理多少条已排队 request，以及为什么停下”收成一个可验证 seam。

## 一句话版本

- `kernel.task_message_service_loop` 负责 `wait -> timeout -> rearm -> dispatch`。
- `kernel.task_message_service_drain` 负责在一次 receive-ready 唤醒上，继续做 bounded `dispatch -> drain -> stop(boundary)`。

如果说前一层证明的是“server 已经能在 runtime loop 里等到一条 request 并处理掉”，那么这一层证明的是“server 现在也已经能在同一次唤醒里，继续按预算吃掉更多已排队 request，并把停下的原因显式收口”。

如果要再把 server task body 里的 bootstrap wait、queue-empty rearm 和 budget hold 收口，当前继续上移到：

- `kernel.task_message_service_pump`

## 为什么单独收这一层

如果只有：

- `TaskMessageApi`
- `TaskMessageTable`
- `TaskMessageDispatcher`
- `TaskMessageServiceLoop`

那么 server task 仍然要在 task body 里继续手写：

- “这次 wake 最多处理多少条”
- “处理到 budget 上限时先停下”
- “如果队列已经空了，也要把 stop boundary 留下来”

这会让：

- drain budget 与单步 dispatch/service loop 边界重新缠在一起
- “停下是因为 budget 命中，还是因为 queue 已空”缺少统一落点
- 后续 service task / protocol task 难以稳定复用同一条 budgeted progress seam

因此当前更健康的做法是把这层单独收成：

- `TaskMessageServiceDrain`

## 模块位置与核心类型

模块位置：

- `Modules/system/kernel/task_message_service_drain.cppm`

当前核心类型：

- `TaskMessageServiceDrainTraceKind`
- `TaskMessageServiceDrainStopReason`
- `TaskMessageServiceDrainResult`
- `TaskMessageServiceDrainTraceEvent`
- `TaskMessageServiceDrainTraceBuffer<Capacity>`
- `TaskMessageServiceDrain<ServiceLoop, TraceBuffer>`

配套 helper：

- `task_message_service_drain_trace_kind_name(...)`
- `task_message_service_drain_stop_reason_name(...)`
- `make_task_message_service_drain(...)`

## 当前 API 形状

`TaskMessageServiceDrain<...>` 当前暴露：

- `valid()`
- `service_loop()`
- `bind_service_loop(...)`
- `bind_trace(...)`
- `receive_event()`
- `receive_timeout_event()`
- `wait_receive_until(due)`
- `step(event, budget)`
- `step_and_wait_until(event, budget, due)`

其中：

- `step(event, budget)` 用于消费一次 receive-ready 或 timeout 事件，并在 receive-ready 路径上最多继续 drain `budget` 条 request
- `step_and_wait_until(event, budget, due)` 用于“若这次 event 是 timeout，则立刻重挂下一次 wait；若这次 event 是 receive-ready，则走 budgeted drain”

返回值 `TaskMessageServiceDrainResult` 当前收口：

- `progressed`
- `wait_armed`
- `timeout_consumed`
- `served`
- `stop_reason`
- `last_dispatch`

其中：

- `served` 表示这次唤醒内实际消费了多少条 request
- `stop_reason` 当前显式区分 `timeout / queue_empty / budget_reached`

## 当前语义边界

### 1) 这层不重写单步 dispatch，也不重写 wait/timeout 语义

`TaskMessageServiceDrain` 当前不重新定义：

- label routing
- handled/unhandled 规则
- reply success/failure 规则
- wait arm / timeout consume / timeout rearm 规则

这些分别仍由：

- `TaskMessageDispatcher`
- `TaskMessageServiceLoop`

定义。

这层只负责把“单次 receive-ready 唤醒之后，是否继续多吃几条 request，以及为什么停下”收成一个稳定 seam。

### 2) `budget_reached` 与 `queue_empty` 是显式可观察的 stop boundary

这是这层最重要的边界之一。

当一次 server 唤醒发生后，drain 当前只会因为两类原因停下：

- 已经达到这次允许消费的 `budget`
- mailbox 队列已经空了，继续 `serve_once()` 不再接受 request

这让“我们是主动因为预算停下”与“我们是自然因为队列空了停下”可以被独立观察，而不是重新散落回 task body 里的隐式循环退出条件。

### 3) 这层只定义单次唤醒内的 bounded drain

`TaskMessageServiceDrain` 当前目标仍然非常克制：

- 它只描述“这一轮 wake 最多再吃多少条 request”
- 它不定义跨多轮 wake 的长期公平性策略
- 它不定义 server 生命周期所有权

因此这里的 `budget` 是单次唤醒内的 progress budget，不是系统级调度配额。

### 4) 这层不隐藏底层 ready edge

`TaskMessageServiceDrain` 当前不会主动吞掉 scheduler / mailbox 已经排队的后续 ready edge。

这意味着：

- 如果底层已经排了多次相同 ready event，而本轮只 drain 到 budget 就停下
- 后续 ready edge 是否继续进入 server，仍由底层 queue/filter 策略决定

这使得上半层 budget 语义与下层 event dedup/coalesce 策略保持解耦。

### 5) 这层仍然不拥有 service discovery / protocol schema

`TaskMessageServiceDrain` 当前仍然不处理：

- service discovery
- endpoint naming
- message catalog/schema
- higher-level protocol progress rules
- trap/syscall ingress
- user/kernel ABI

它当前只是一个 server-side budgeted progress seam，不是完整的 service framework。

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
- 单次唤醒内的 budgeted drain 边界
- service task body 级别的 bootstrap/rearm/hold orchestration

## 当前证据路径

当前与这层直接相关的证据路径有两条：

- `Examples/kernel/runtime_task_message_drain_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

它们当前分别承担：

- `runtime_task_message_drain_host`
  - 验证真实 `RuntimeBridge + RuntimeMailbox + TaskMessageServiceDrain` 已经能在 scheduler loop 里形成“第一次唤醒 drain 到 budget、第二次唤醒 queue empty 收尾、并显式留下 stop boundary”的 live 闭环
- `minimal_kernel_runtime_host_smoke.ps1`
  - 把这层纳入 runtime host smoke，保证它与现有 mailbox/message/runtime 证据网保持对齐

更上一层的 service task body orchestration，当前继续上移到：

- `kernel.task_message_service_pump`

## 当前非目标

当前这层仍然不处理：

- 跨唤醒公平性与 starvation policy
- bootstrap wait / timeout rearm / queue-empty rearm / budget hold 的统一收口
- service discovery
- higher-level message catalog/schema
- trap/syscall ingress
- user/kernel ABI

当前最重要的不是立刻造出完整 service framework，而是先把“单次唤醒内的 bounded drain”收成一条稳定 seam，让后面的 service task / protocol progress task 有明确的落点。
