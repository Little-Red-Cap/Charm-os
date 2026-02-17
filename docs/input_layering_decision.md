# UI/Ink 输入层分层决策（阶段记录）

## 背景

我们在收敛 `Modules/ui/ink/platform/input/*` 时遇到边界问题：
“输入模块是否应该归 HAL 管理？”

结合当前的分层红线（Foundation -> Runtime -> Domains）与“先耦合、再收敛”的策略，
本文件记录可落地、低返工的拆分决策。

## 结论（当前执行版本）

**HAL 只管“读到什么”，UI 才管“这是什么意思”。**

拆成三层：

1) **HAL（硬件抽象）**
   - 放原始采样与设备读数
   - 只提供“事实”，不做语义

2) **Input 运行时层（Runtime / IO）**
   - 放通用事件与意图语义
   - 可被 UI / Shell / Game 复用

3) **UI 领域层**
   - 放策略与手感（去抖、重复、手势、节奏）
   - 允许随 UI 设计变化

一句话总结：
**HAL 只管“读到什么”，Runtime 管“通用语义”，UI 管“体验策略”。**

## 当前已落地的拆分

已完成迁移：

- `input.raw` -> **HAL**
  - 位置：`Modules/io/hal/input.raw.cppm`

- `input.events` / `input.intent` / `input.encoder_decoder` -> **Runtime / IO**
  - 位置：`Modules/io/input/`

UI 侧保留：

- `input.sampler`（策略层）
  - 位置：`Modules/ui/ink/platform/input/input.sampler.cppm`

- `input.queue`（UI 事件队列）
  - 位置：`Modules/ui/ink/platform/input/input.queue.cppm`

## 为什么不是“全放 HAL”

如果把语义/手感放入 HAL，会产生：

- HAL 被 UI 语义污染（违反 Runtime 不依赖 Domains）
- UI 更换/演进成本被放大
- 输入策略迭代必须触碰底层，回归成本翻倍

## 映射关系（按文件）

| 文件/模块 | 归属层 | 说明 |
| --- | --- | --- |
| `input.raw` | HAL | 原始采样与硬件事实 |
| `input.events` | Runtime/IO | 通用事件 |
| `input.intent` | Runtime/IO | 通用意图 |
| `input.encoder_decoder` | Runtime/IO | 旋钮解码（无 UI 语义） |
| `input.sampler` | UI/Ink | 去抖/重复/手势等策略 |
| `input.queue` | UI/Ink | UI 事件队列（已使用 service::RingQueue） |

## 规则（硬约束）

- HAL 不包含任何“语义”，只返回 raw 数据。
- Runtime 只做通用语义，不包含 UI 手感策略。
- UI 层允许策略变化，但不得反向依赖 HAL 实现细节。
- 事件队列不得阻塞、不得重试、不得休眠。

## 后续建议（可选）

1) 如果 Shell / Game 也需要输入策略，可在 Runtime 侧新增“默认采样策略”，
   但不要把 UI 的具体手感迁移过去。
2) 如需多声道输入或复杂设备（如触控矩阵），先扩展 `input.raw`，
   再由 UI/Runtime 适配，不直接在 HAL 做语义判断。

