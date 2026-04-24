# Charm SSU 契约（草案）

## 目标

Charm 的“可调度语义单元”（SSU, Schedulable Semantic Unit）不是新的事件系统，也不是新的数据流库。
它定义的是一套统一的执行语义原子：

- 谁来执行
- 由什么触发
- 每次最多做多少
- 是否允许阻塞
- 如何被观测与重新调度

SSU 的目标不是消灭 EDA、reactor、run loop、pipeline、audio pull，
而是让它们都能投影到同一套“可调度执行语义”上，避免系统继续并行维护多套互不相通的执行模型。

## 非目标

- 不把一切都重写成同一种 API 风格
- 不强迫 audio/dataflow 放弃设备时钟主导与 pull 模型
- 不在第一阶段引入庞大的 In/Out 类型系统
- 不替代 init.graph 的装配节点语义

## 核心原则

### 1. Execution First

SSU 统一的是执行语义，而不是 Event 类型或 Stream 类型。
事件、定时器、IO ready、frame tick、pull demand 都只是触发源。

### 2. Context Is Part Of The Type

ISR、task context、任意上下文不是注释，而是契约的一部分。
如果一个单元只能在 task context 运行，这必须在类型/trait 上可见。

### 3. Budgeted By Default

SSU 必须天然支持 budgeted 执行：

- 不允许 busy-spin
- 不允许内部 sleep
- 不允许内部 timeout loop
- 做不完就返回并重新调度

### 4. Resubmit Is A First-Class Path

“做一小步，然后把自己重新排队”不是特例，而是标准模式。
ReactorPump 已经证明这是 Charm 的自然路径。

### 5. Observability Is Mandatory

每个 SSU 都必须能被 scheduler/trace/stats 接住，
否则系统无法把执行语义统一成真实的调试与运行时事实。

## 最小契约

第一阶段只定义五个硬字段，不先引入复杂的数据面类型参数。

### 执行域（Execution Domain）

- `isr_only`
- `task_only`
- `anywhere`

### 触发类型（Trigger Kind）

- `event`
- `io_ready`
- `timer`
- `frame`
- `demand`

### 预算语义（Budget Kind）

- `single_step`
- `budgeted`

### 阻塞语义（Blocking Kind）

- `non_blocking`
- `may_block`

### 可观测性（Observability）

SSU 必须允许接入：

- trace 名称/标签
- stats 计数
- 重新调度次数
- 预算耗尽次数（后续扩展）

## 统一执行入口

Charm 第一阶段只收口三类提交入口：

- `event-submit`
- `io-ready-submit`
- `demand-submit`

timer/frame 可以先投影到 event-submit，
等 SSU 主路径稳定后再做更细分的专用入口。

## 当前系统到 SSU 的映射

### EDA Task -> Event Unit

- 触发：`event`
- 执行域：`task_only`
- 语义：run-to-completion 的单步处理

### Reactor Pump -> IO Drain Unit

- 触发：`io_ready`
- 执行域：`task_only`
- 语义：budgeted drain，做不完则 resubmit

### RunLoop Step -> Frame Unit

- 触发：`frame`
- 执行域：`task_only`
- 语义：每帧推进一个阶段

### Audio Pull Engine -> Demand Unit

- 触发：`demand`
- 执行域：控制面通常 `task_only`，数据面由设备时钟驱动
- 语义：device clock 主导、pull graph、IRQ 内仅最短路径

## 命名约束

SSU 不使用 `Node` 作为统一术语，避免与下列既有语义冲突：

- `init::Node`：初始化装配节点
- audio/dataflow node：处理图节点

第一阶段统一使用 `Unit`。

## 第一阶段落地范围

1. 增加 `kernel.ssu`，定义 trait / concept / 元信息结构
2. 增加 EDA -> SSU 的轻量适配
3. 增加 ReactorPump -> SSU 的轻量适配
4. 不重写现有 scheduler，只给 scheduler 一个统一观察口
5. 不改变 audio/dataflow 主设计，只给它预留 `demand` 语义入口

## 禁止事项

- 禁止把 SSU 做成新的“万能对象系统”
- 禁止在第一阶段引入 runtime 多态为主的重框架
- 禁止把所有现有模块强行迁到同一种 API 再求编译通过
- 禁止为了统一而破坏 audio 的 pull 与设备时钟主导

## 第一阶段验证用例

### 用例 B：Event/Message + Reactor/Timer

这是 SSU 的 P0 验证场景。

验收点：

- reactor pump 可以被表达为标准 `task_only + io_ready + budgeted + non_blocking` 单元
- EDA task 可以被表达为标准 `task_only + event + single_step + non_blocking` 单元
- scheduler trace/stats 能看到统一的 SSU 标签
- 不新增旁路 drain 路径

## 与 RTOS 的关系

如果 SSU 成立，RTOS 在 Charm 中更像“执行资源后端”，例如：

- 时间源
- 中断保护
- 唤醒机制
- 软中断触发
- 线程/栈/阻塞资源

Charm 自己负责的是统一的执行语义层：

- 事件注入
- budgeted 执行
- resubmit 路径
- 统一 trace/stats
- 可组合的执行契约
