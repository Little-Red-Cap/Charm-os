# 推进 TODO 与分工（协作说明）

## 目的

在大规模耦合重构期，明确分工与推进节奏，减少冲突与重复劳动。

## 协作规则（最小约束）

1) 开工前先看 `git log -1` 与 `git status`，同步最新改动。  
2) 每个任务以“最小可合并单元”提交，避免长时间悬空。  
3) 任意跨模块改动必须先在本文档登记认领。  
4) 发生冲突时，优先保留“最新事实”，并记录冲突点与决策。

## 任务认领方式

在 TODO 列表中标注 `[User]` / `[AI]` / `[Shared]`。完成后标记 `Done` 并写一条结果摘要。

## 分工建议（可调整）

- [User]
  - 需求与优先级决策
  - 关键行为验证与回归样例输出
  - 复杂模块的最终 API 取舍

- [AI]
  - 模块拆分与重构落地
  - 文档同步与结构化整理
  - 回归脚本与示例工程调整

- [Shared]
  - 架构分层红线与依赖规则
  - 关键模块接口协商（Audio/FS/ModuleX）

## 推进 TODO（持续更新）

### 架构收敛

- [Shared] 统一分层语义标注（Foundation/Runtime/Domains）到 `docs/architecture_overview.md`
  - 状态：Done
  - 备注：按当前目录结构与耦合策略补齐

- [AI] 输入分层落地检查（HAL/IO/UI 依赖边界）
  - 状态：Done
  - 产出：`docs/input_layering_decision.md`

### 能力回收（UI/Ink & UI/Vivid）

- [Shared] UI/Ink：格式化统一到 `out.format`
  - 状态：In Progress

- [Shared] UI/Ink：输入链路回收（raw/intent/encoder_decoder）
  - 状态：In Progress

- [Shared] UI/Vivid：输出/日志对齐到 `out.*`
  - 状态：Todo

### 音频主线

- [Shared] 音频重配/回归样例补齐
  - 状态：In Progress

- [Shared] 声道变化退化策略（文档 + 最小代码）
  - 状态：In Progress

### 整体整理

- [AI] 示例与杂项归档到 `Draft/` 的后续清点
  - 状态：Deferred

- [User] 核心模块最终保留清单（Charm 总架构 -> Charm-os 子项目）
  - 状态：Todo

## 冲突记录

无。

















USBHost（你现在只有Device）：Host的管线/Hub/枚举流程可借鉴，但实现可自研。
2.TCP/IP（IwIP/NSFTCPIP）接口层：不急着搬协议栈，先定义socket/packet/endpoint 抽象。
3.驱动框架（设备模型+统一init/probe)：VSF有较完整的 driver组织方式，适合抽成”设备注册表”。
4. Power / Low-Power 管理：时钟域、休眠、唤醒策略，这块现在你的 Kernel/Port还没覆盖。
5. Audio/Video 中间件 (VSF stream/pipeline 的接口形状): 可对标你音频 pipeline的分层设计。
   我更激进的建议：
   先把**"设备注册表+ driver lifecycle"**抽象出来（ModuleX+Kernel + IO 很好承载)，让 USB/TCPIP/FS 都能挂到统一设备模型上。
   这样后续再扩VSF模块时，不会成为孤岛。
   如果你同意，我可以先做一份"设备模型草案"（driver/init/probe/remove/pm hooks+注册表），再对照VSF的设备层结构给出迁移路线。







1. progress_bar_drill
2.
progress_bar_round
(已迁移)
3.
progress_bar_simple
(已迁移)
4.
progress_bar_flowing
(已迁移)
5.
progress_bar_round
(已迁移)
6.
spinning_wheel
7.
image_box
8.
icon_list
(已迁移)
9. text_list / text_tracking_list
   (已迁移)
10.
number_list
(已迁移)
11.
meter_pointer
(可做指针类仪表）
12.
progress_bar_drill
13.
progress_bar_round
14. progress_bar_simple
    （带刻度/钻孔风格）(已迁移)(已迁移)





Histogram/Chart 的数据源回调路径补“局部脏区”标记（目前只在 set_values 路径优化）。
ListView 的缓存回收策略再加一层“行高度变化/数据源变更时的最小失效区”。

ClipPolicy与 LayoutSpec已统一，建议补一个“渲染缓存/dirty rect"的策略接口，让控件可声明"需缓存/无需缓存”，便于scene graph 迁移。

·输入路由建议引l入可配置优先级/命中策略（穿透、独占、hover-only），减少复杂控件的定制代码。

·文本部分可加入“可插拔字体回退链”，避免后续多语言难以扩展