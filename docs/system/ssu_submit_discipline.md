# SSU 提交纪律（草案）

## 目的

SSU 如果只停留在 task 标签层，会变成“可观测命名系统”；
只有把提交入口收口，SSU 才会成为“系统执行秩序”。

这份纪律文档定义：

- 新执行路径如何进入系统
- 哪些入口是主路径
- 什么行为属于旁路
- 出现旁路时如何约束与回收

## 总原则

### 原则 1：所有可调度执行都必须可映射到 SSU 提交通道

不要求第一阶段就把所有路径重写成同一种 API，
但要求任何新执行模型都必须声明其投影到哪类提交语义。

### 原则 2：先收口提交入口，再讨论调度行为统一

本阶段不要求 scheduler 直接按 SSU 做复杂策略分流，
优先级是先把“进入系统执行面”的入口收紧。

### 原则 3：提交语义必须显式携带上下文边界

ISR-safe、task-only、blocking/non-blocking、budgeted 等语义，
不能依赖注释与口口相传，必须在提交路径上可声明、可审查。

## 三类主提交通道

本阶段固定三类主提交通道：

1. `event-submit`
2. `io-ready-submit`
3. `demand-submit`

timer/frame 在本阶段允许投影到 `event-submit`，
等主线稳定后再考虑独立扩展。

## 通道定义

### A. event-submit

#### 语义

- 用于离散事件推进
- 典型来源：task 间消息、状态推进、生命周期事件

#### 约束

- 默认 task-context 提交
- 处理体应满足 run-to-completion 单步语义
- 不得在处理体内进行 busy-spin/阻塞等待

#### 典型映射

- EDA task dispatch
- 部分 timer/frame 的阶段投影

#### 当前内核入口

- `scheduler.post(...)`
- `scheduler.post_token(...)`

### B. io-ready-submit

#### 语义

- 用于表示“IO 侧已经 ready，需要在 task context 消费”
- notify 与 drain 的语义必须分离

#### 约束

- ISR 侧只能触发 notify/入队，不得直接执行重处理
- drain 必须在 task context 执行
- 必须 budgeted，做不完通过 resubmit 回到主路径

#### 典型映射

- reactor waker -> pump task -> drain

#### 当前内核入口

- `scheduler.post_io_ready(...)`
- `scheduler.post_io_ready_token(...)`

### C. demand-submit

#### 语义

- 用于表示“下游对数据/执行产生了明确需求”
- 常见于 pull 模型、设备时钟驱动的数据面

#### 约束

- demand 触发与数据处理域边界必须清晰
- IRQ 内仅最短路径，不做重处理
- 重处理必须通过可调度路径承接

#### 典型映射

- audio pull / DMA 节拍驱动的数据需求

#### 当前内核入口

- `scheduler.post_demand(...)`
- `scheduler.post_demand_token(...)`

## 落地进展（当前）

- scheduler 已落地三类 submit 入口：`post` / `post_io_ready` / `post_demand`
- `system.reactor_pump` 的 host/bringup 默认接线已切到 `io-ready-submit`
- 其余路径继续保持最小变更，避免一次性扩大改造面
## 旁路定义与处理

以下行为视为旁路：

- 新建未声明 submit 类型的执行入口
- 在模块内部自建推进循环，绕开 scheduler 主路径
- 在 ISR 内直接执行应当 defer 的重处理逻辑
- 在协议层引入内部 timeout loop / busy-spin 来替代提交机制

### 出现旁路时的处理规则

必须补齐四项信息：

1. 旁路原因
2. 临时边界（影响范围）
3. 退出条件（何时回收）
4. 回收路径（映射回哪类 submit）

没有这四项，不接受新增旁路。

## 评审清单（Review Checklist）

新增执行相关逻辑时，评审至少回答以下问题：

1. 它属于哪类 submit（event/io-ready/demand）？
2. 它的执行域是什么（ISR/task/anywhere）？
3. 它是否 budgeted？预算耗尽后怎么处理？
4. 它是否阻塞？如果阻塞，边界在哪里？
5. 它是否绕开了现有 scheduler 主路径？
6. 它是否已经可以在 observability 中被识别？

## 与严格模式的关系

`CHARM_KERNEL_REQUIRE_SSU_META=1` 解决的是“task 声明层”的约束。
submit discipline 解决的是“执行入口层”的约束。

两者关系：

- 严格模式确保 task 具备 SSU 身份
- submit discipline 确保执行路径不绕过 SSU 主轴

二者缺一不可。

## 当前阶段的落地顺序

1. 文档先行（本草案）
2. 在主线样板 target 中按清单执行 review
3. 对新增执行路径做 submit 类型声明
4. 逐步把高频旁路回收到三类主通道

## 当前不做

- 不在这一阶段一次性引入完整 runtime submit 框架
- 不强制所有历史代码立刻重写
- 不把 scheduler 重构为完全基于 submit 类型的调度器

## 一句话纪律

新执行路径可以出现，但它不能匿名出现。
它必须声明自己属于哪类 submit，并最终回到 SSU 主通道。



