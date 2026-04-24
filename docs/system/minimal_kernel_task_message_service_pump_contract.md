# 最小内核 task message service pump 契约（草案）

这份文档用于把 server task body 里剩下的 `bootstrap wait / timeout rearm / queue-empty rearm / budget hold` 样板，从 `TaskMessageServiceDrain` 之上继续收成一条边界。

它对应当前新增的：

- `Modules/system/kernel/task_message_service_pump.cppm`

目标不是提前定义完整 protocol scheduler，也不是提前固定更高层 service schema，而是先把“server task body 该如何拿着 bootstrap event、budget 和下一次 due，把 wait/drain/rearm/hold 串成稳定循环”收成一个可验证 seam。

## 一句话版本

- `kernel.task_message_service_drain` 负责单次唤醒内的 bounded `dispatch -> drain -> stop(boundary)`。
- `kernel.task_message_service_pump` 负责把 bootstrap、timeout rearm、queue-empty rearm 和 budget hold 收成 task-body 级别的最小 orchestration。

如果说前一层证明的是“server 已经能在一次 receive-ready 唤醒里按预算连续吃几条 request”，那么这一层证明的是“server task body 现在也已经能稳定接住 bootstrap、空队列重挂等待、budget hold 和 stale ready 这些更像 service ownership 的边界”。

## 为什么单独收这一层

如果只有：

- `TaskMessageApi`
- `TaskMessageTable`
- `TaskMessageDispatcher`
- `TaskMessageServiceLoop`
- `TaskMessageServiceDrain`

那么 server task body 仍然要继续手写：

- bootstrap event 到来时先挂第一轮 wait
- timeout 之后用新的 due 重挂下一轮 wait
- queue 已空时重挂下一轮 wait
- budget 打满时先 hold，等待底层已排队的 ready edge 继续推进

这会让：

- task body 仍然保留不少 orchestration 样板
- “为什么这次重挂 wait，为什么这次不重挂而是 hold”缺少统一落点
- 后续 protocol progress task 很难复用同一条 service ownership seam

因此当前更健康的做法是把这层单独收成：

- `TaskMessageServicePump`

## 模块位置与核心类型

模块位置：

- `Modules/system/kernel/task_message_service_pump.cppm`

当前核心类型：

- `TaskMessageServicePumpTraceKind`
- `TaskMessageServicePumpReason`
- `TaskMessageServicePumpResult`
- `TaskMessageServicePumpTraceEvent`
- `TaskMessageServicePumpTraceBuffer<Capacity>`
- `TaskMessageServicePump<ServiceDrain, TraceBuffer>`

配套 helper：

- `task_message_service_pump_trace_kind_name(...)`
- `task_message_service_pump_reason_name(...)`
- `make_task_message_service_pump(...)`

## 当前 API 形状

`TaskMessageServicePump<...>` 当前暴露：

- `valid()`
- `service_drain()`
- `bind_service_drain(...)`
- `bootstrap_event()`
- `bind_bootstrap_event(...)`
- `bind_trace(...)`
- `receive_event()`
- `receive_timeout_event()`
- `wait_receive_until(due)`
- `step(event, budget, due)`

其中：

- `bootstrap_event()` 定义 pump 眼里的 server bootstrap 触发面
- `step(event, budget, due)` 统一接住 bootstrap、timeout、queue-empty 和 budget hold 四条边界
- `due` 是“如果这轮需要重挂 wait，下一轮应挂到哪里”的显式输入

返回值 `TaskMessageServicePumpResult` 当前收口：

- `progressed`
- `bootstrap_consumed`
- `wait_armed`
- `hold_ready`
- `reason`
- `drain`

其中：

- `reason` 当前显式区分 `bootstrap / timeout / queue_empty / budget_reached`
- `drain` 直接携带底层 `TaskMessageServiceDrainResult`

## 当前语义边界

### 1) 这层不重写 drain 的 budget 语义

`TaskMessageServicePump` 当前不重新定义：

- 单步 dispatch/reply 规则
- bounded drain 的 `budget_reached / queue_empty` 判定
- timeout consume 规则

这些仍由：

- `TaskMessageDispatcher`
- `TaskMessageServiceLoop`
- `TaskMessageServiceDrain`

定义。

这层只负责在 task body 视角上回答：

- 现在是不是 bootstrap
- 这轮要不要 rearm wait
- 这轮是不是该 hold，等待后续 ready edge

### 2) `queue_empty rearm` 与 `budget hold` 是显式分开的

这是这层最重要的边界之一。

当一轮 service progress 结束后，pump 当前只会在两类 higher-level 决策间切换：

- `queue_empty`
  - 说明本轮已经把当前 mailbox request 吃空，下一步应该重挂 wait
- `budget_reached`
  - 说明本轮只是主动因为预算停下，当前不应再额外 arm wait，而是先 hold

这让“空队列所以去等下一轮”与“预算打满所以先借现有 ready edge 继续推进”能被独立观察，而不是再次散落回 task body 里的 `if/else`。

### 3) bootstrap event 被收成显式入口

`TaskMessageServicePump` 当前显式持有：

- `bootstrap_event`

这意味着：

- server task body 不必再自己先识别 bootstrap event，再单独写第一次 `wait_receive_until(...)`
- bootstrap 与后续 timeout/queue-empty rearm 可以落在同一条 trace 和同一类结果面上

### 4) stale ready 当前是显式失败边界，不会被吞掉

如果底层已经有 wait 在身上，但之后又来了一个 stale ready edge，当前 pump 仍会尝试按 `queue_empty` 路径重挂 wait。

如果这次重挂失败，结果会表现为：

- `reason == queue_empty`
- `wait_armed == false`

这样 stale ready 不会被误包装成“成功完成了一轮新的 wait arm”，而是作为上层可见的 service ownership 边界被保留下来。

### 5) 这层仍然不拥有 protocol schema / service discovery

`TaskMessageServicePump` 当前仍然不处理：

- message catalog/schema
- service discovery
- endpoint naming
- 跨 service 公平性策略
- trap/syscall ingress
- user/kernel ABI

它当前只是 task-body 级别的 service orchestration seam，不是完整的 service framework。

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

- `Examples/kernel/runtime_task_message_pump_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

它们当前分别承担：

- `runtime_task_message_pump_host`
  - 验证真实 `RuntimeBridge + RuntimeMailbox + TaskMessageServicePump` 已经能在 scheduler loop 里形成 `bootstrap -> timeout rearm -> hold -> queue-empty rearm -> stale-ready rearm-fail -> timeout rearm` 的 live 闭环
- `minimal_kernel_runtime_host_smoke.ps1`
  - 把这层纳入 runtime host smoke，保证它与现有 mailbox/message/runtime 证据网保持对齐

## 当前非目标

当前这层仍然不处理：

- 更高层 protocol schema / message catalog
- service discovery
- 跨 service fairness / starvation policy
- trap/syscall ingress
- user/kernel ABI

当前最重要的不是立刻造出完整 protocol runtime，而是先把“service task body 级别的 bootstrap/rearm/hold orchestration”收成稳定 seam，让后面的 protocol progress task 有明确落点。
