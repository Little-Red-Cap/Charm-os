# SSU 提交通道台账（阶段 B）

## 目的

把 submit discipline 从原则落到可执行台账：

- 当前哪些路径已经收口
- 哪些路径仍在 event-submit
- 下一批最小迁移候选是什么

## 当前已收口

### 1) event/io-ready/demand 三类入口（内核）

- `scheduler.post(...)`
- `scheduler.post_io_ready(...)`
- `scheduler.post_demand(...)`

并提供 token 版本：

- `post_token(...)`
- `post_io_ready_token(...)`
- `post_demand_token(...)`

### 2) reactor 主路径（系统默认接线）

- `system.reactor_pump` 通过 host/bringup 默认使用 `post_io_ready_fn()`
- waker 路径使用 `io-ready-submit`，drain 后 `more` 的续推路径使用 `demand-submit`（首个 demand 样板）
- `input.pump` 在单次 budget 用满时改走 `demand-submit` 续推，避免全部依赖下一次 timer tick（第二个 demand 样板）
- `canopen.pump` 在 `sdo/nmt` 存在 pending 发送时走 `demand-submit` 续推（第三个 demand 样板）

## 当前仍在 event-submit 的常见路径

- EDA 任务间离散消息与状态推进（预期继续保留）
- run loop / phase 推进（当前阶段允许）
- 定时器投影到 event（当前阶段允许）

## 下一批迁移候选（最小面）

### 候选 A：扩展 demand-submit 的第四个系统样板

目标：在前三个样板（reactor `more` + input.pump budget续推 + canopen pending续推）之外，再选一个真实“下游需求驱动”路径落地 `post_demand(...)`。

建议优先：

- 数据请求明确、依赖干净、可单独构建的 pump/service 路径

### 候选 B：run loop 提交来源标注

目标：不改行为，仅在观测/文档层把 run loop 触发归类为 event-submit 投影，形成可审计台账。

### 候选 C：新增执行路径评审模板落地

目标：新增执行路径 PR 必须声明 submit 类型（event/io-ready/demand）和回收路径。

## 暂不做

- 不把历史路径一次性迁到 demand-submit
- 不在这一轮引入 scheduler 的重策略分流
- 不因为 submit 台账推进而扩大 unrelated 重构

## 一句话

先把“谁从哪条 submit 进入系统”做成可审计事实，再逐步扩大迁移面。




