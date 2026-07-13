# Charm Foundation Runtime 与统一应用入口模型草案

## 1. 这份文档的定位

这份文档用于定义 `Charm` 在运行时层面的两个关键问题：

- 什么能力应当在系统一开始就可用
- 应用层应当拿到怎样一个更干净、更稳定的入口点

它不是某个单独产品的实现说明，而是对 `Charm` 运行时分层的上位约束。

这份文档与以下文档形成互补关系：

- [`charm_工程对象模型草案.md`](charm_工程对象模型草案.md)
- [`charm_构建系统升级方向草案.md`](charm_构建系统升级方向草案.md)
- [`charm_复杂场景开发体验改进提案.md`](charm_复杂场景开发体验改进提案.md)

其中：

- 工程对象模型回答“系统如何理解一个真实工程”
- 构建系统升级方向回答“构建层如何承接这些对象”
- 本文回答“运行时如何为这些对象提供稳定入口与基础宿主能力”

## 2. 为什么需要 Foundation Runtime

在当前 `Charm` 的真实项目推进中，已经暴露出一个很明确的问题：

- 应用层入口仍然夹带过多运行时装配细节
- 最小日志输出、活性探针、panic 诊断等最基础能力，并没有被系统保证为“一开始就可用”
- bringup、runtime、graph、scenario 的边界仍然不够清楚

这会导致非常别扭的开发体验：

- 为了确认程序是否活着，需要临时加“早期输出”
- 但这些输出又常常依赖更高层的 bringup，导致“越要早看，越看不到”
- 应用层为了表达一个场景，不得不理解许多本不该由它关心的系统细节

这说明当前系统缺少一个正式的、稳定的基础运行时层。

## 3. 关键判断

### 判断 1：最小诊断能力不是“早期技巧”，而是正式宿主能力

如果某个能力：

- 不依赖复杂业务逻辑
- 不依赖完整 graph
- 不依赖大量能力装配

那么它就不应被视为“调试时临时加的早期技巧”，而应被视为系统从一开始就保证存在的基础宿主能力。

### 判断 2：应用层不应直接承担启动活性问题

应用层关心的应该是：

- 我要跑哪个场景
- 我要使用哪些能力
- 我拿到的运行环境是否已经可活、可诊断、可观察

应用层不应继续承担：

- UART 何时可用
- 第一条日志什么时候能打
- 图装配前是否已经有基础运行时

### 判断 3：Foundation Runtime 是运行时分层问题，不是 USB 特例

当前问题虽然在 `USB MSC` 调试中暴露得很明显，但它并不是 USB 特例。

同样的问题会出现在：

- Audio bringup
- UI host preview
- FS 验证
- 任何复杂系统启动链调试

因此，这不是某个子系统的小修补，而是 `Charm` 运行时分层必须补上的正式层次。

## 4. Foundation Runtime 的定义

`Foundation Runtime` 是 `Charm` 运行时模型中的最小稳定宿主能力层。

它位于：

- `Board / Platform` 的硬件与宿主事实之上
- `System Graph / Bundle / Scenario` 之下

它的目标不是承载完整系统功能，而是为系统和应用同时提供一组从启动最早阶段就稳定可用的基础能力。

## 5. Foundation Runtime 的职责

### 5.1 必备职责

`Foundation Runtime` 至少应负责以下职责：

- 最小日志输出
- 最小 panic / fault 输出
- 最小时间基准或 monotonic tick
- 最小身份信息访问

这里的身份信息至少包括：

- `Product`
- `Platform`
- `Board`
- `Scenario`

### 5.2 可选职责

在条件允许时，`Foundation Runtime` 还可以逐步承接：

- reset reason
- 最小内存/region 信息
- 最小 build / version 信息
- 最小 trace sink

### 5.3 明确不负责的职责

`Foundation Runtime` 不负责以下内容：

- 完整任务调度
- 完整 event/reactor 体系
- USB / Audio / UI 等高层能力装配
- 复杂 graph 构造
- 场景业务逻辑

它必须保持足够小、足够稳，避免自己重新膨胀成另一层“大系统”。

## 6. 与其它对象的关系

### 6.1 与 `Platform`

`Platform` 决定宿主大类，例如：

- `windows-sdl3`
- `stm32h747-hal`

`Foundation Runtime` 使用 `Platform` 提供的基础实现能力，但不等同于 `Platform`。

### 6.2 与 `Board`

`Board` 决定具体宿主实例，例如：

- `hqzy_cm7`
- `win_stub`

`Foundation Runtime` 可以使用 `Board` 提供的最小物理/宿主后端，例如 UART 或 stdout，但不应直接暴露板级细节给应用层。

### 6.3 与 `Runtime`

`Runtime` 是更一般的运行时对象。本文建议把它明确拆成两层：

- `Foundation Runtime`
- `System Runtime`

其中：

- `Foundation Runtime` 负责最小稳定宿主能力
- `System Runtime` 负责 graph、scheduler、reactor、bundle attach 等更高层运行时结构

### 6.4 与 `Scenario`

`Scenario` 表达“当前要跑什么场景”。

`Scenario` 不应自己解决“最小日志是否可用”“最小活性如何确认”，这些都应由 `Foundation Runtime` 预先保证。

## 7. 统一应用入口模型

在引入 `Foundation Runtime` 后，应用层入口应当从“直接碰系统装配细节”转向“接收一个已可活的运行上下文”。

### 7.1 当前问题

当前应用层入口往往隐含承担：

- board bringup 的顺序认知
- runtime glue 的组织
- graph build/start 的时机
- 日志何时可见

这让应用层入口过脏，也让系统调试非常困难。

### 7.2 目标入口形态

应用层最终应拿到类似如下的入口：

- `scenario_main(AppContext& ctx)`
- `run(AppRuntime& rt)`
- `app_main(ScenarioContext& ctx)`

关键不在名字，而在契约：

- 进入入口时，`Foundation Runtime` 已经可用
- 基础日志已经可用
- identity 已经可用
- 更高层系统运行时是否装配完成，由上下文明确表达

### 7.3 入口上下文至少应提供什么

一个最小可用的应用入口上下文至少应提供：

- foundation log
- monotonic tick
- `Product / Platform / Board / Scenario` identity
- panic / fault report hook

在更高阶段，还可以继续扩展：

- scheduler handle
- reactor post/pump handle
- capability registry view
- bundle attach point

## 8. 建议的运行时分层

本文建议 `Charm` 将运行时分层明确收敛为四层：

### 层 0：Foundation Runtime

负责：

- 最小日志
- panic/fault
- 最小时间基准
- identity

### 层 1：Platform / Board Runtime

负责：

- clock
- UART/stdout backend
- USB PCD
- SDL3 backend
- memory region facts

### 层 2：System Runtime

负责：

- scheduler
- reactor
- graph
- bundle attach
- system chain

### 层 3：Scenario Runtime

负责：

- 具体场景逻辑
- 场景级能力组合
- 场景级调试与 observability

这四层的目标是把“程序活性”“基础诊断”“系统装配”“场景逻辑”从职责上分开。

## 9. Host / MCU 统一要求

`Foundation Runtime` 必须同时适用于 Host 与 MCU。

### 9.1 Host 侧

在 Host 上，`Foundation Runtime` 的后端可能是：

- stdout
- stderr
- SDL log
- host monotonic clock

### 9.2 MCU 侧

在 MCU 上，`Foundation Runtime` 的后端可能是：

- UART
- SWO
- semihosting
- 板级 monotonic tick

### 9.3 上层统一视角

无论底层后端是什么，上层看到的都应是同一种基础运行时契约，而不是分裂成多套“host 日志接口”和“mcu 日志接口”。

## 10. 对当前 Player/USB 现状的直接启示

`Player` 当前在 `USB MSC` 调试里暴露的问题，恰好证明了 `Foundation Runtime` 的必要性。

当前现象是：

- 系统已经有场景、bundle、runtime、graph
- 但最基本的“程序活性是否可见”没有被一开始保证

这导致：

- 即使补了场景日志，也可能完全看不到
- 排查工作被迫回到“先确认程序有没有活”这种更基础的问题

这并不是 `USB MSC` 的特例，而是运行时分层还未补齐的信号。

## 11. 最小落地建议

本文不建议一开始就大改整个系统，而建议按最小路径推进。

### 第一步：把 `Foundation Runtime` 升格为正式概念

先在文档、命名和对象模型中确认它的地位。

### 第二步：在 `Player` 中建立第一个基础运行时样板

建议先在 `Player / hqzy_cm7` 侧定义一个最小 `foundation` 层，统一：

- 最小 log sink
- 最小 panic sink
- monotonic tick
- identity access

### 第三步：把应用入口逐步改造成接收上下文

不要让场景函数继续直接承担过多运行时装配细节，而是逐步转向：

- `scenario_main(ctx)`

### 第四步：让 Host 侧同步具备同类 foundation 层

这样 `Player` 才能真正成为双端统一运行时模型的试点。

## 12. 不该做的事

### 不该把 Foundation Runtime 做成另一个大系统

它的目标是小而稳，而不是重新引入一层复杂框架。

### 不该把所有日志系统都塞进去

`Foundation Runtime` 只承载最小稳定日志，不负责完整 trace / metrics / advanced observability 体系。

### 不该让应用继续自己解决活性问题

一旦应用层还要自己判断“此刻能不能 print”，说明 foundation 仍未真正建立。

## 13. 小结

`Charm` 当前最缺的，不只是更多模块，也不只是更漂亮的构建脚本。

它同样缺一层更稳、更早、更统一的基础运行时层。

本文建议把这层正式定义为：

- `Foundation Runtime`

并据此推动两件事：

- 让最小输出、最小活性、最小诊断从系统一开始就可用
- 让应用层逐步获得一个更干净、更统一的入口上下文

这不是某个调试技巧的优化，而是 `Charm` 运行时分层成熟度提升的关键一步。
