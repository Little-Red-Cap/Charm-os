# 推进 TODO 与分工（协作说明）

## 目的

在大规模重构期，明确分工与推进节奏，减少冲突与重复劳动。

## 协作规则（最小约束）

1) 开工前先看 `git log -1` 与 `git status`，确保同步最新改动。  
2) 每个任务以“最小可合并单元”提交，避免长时间悬而不决。  
3) 任何跨模块改动必须先在本文件认领。  
4) 若发生冲突：优先保留“最新事实”并在本文件记录冲突点与决策。

## 任务认领方式

在 TODO 列表中标记：`[User]` / `[AI]` / `[Shared]`。  
完成后标记为 `Done` 并写一句结果摘要。

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
  - 大型模块的接口协商（音频/FS/ModuleX）

## 推进 TODO（持续更新）

### 架构收敛

- [Shared] 统一分层语义标注（Foundation/Runtime/Domains）到 `docs/architecture_overview.md`  
  - 状态：In Progress  
  - 结果：完成后标记 Done

- [AI] 输入分层落地检查（HAL/IO/UI 依赖边界）  
  - 状态：Done  
  - 结果：`docs/input_layering_decision.md` 已更新

### 能力回收（UI/Ink & UI/Vivid）

- [Shared] UI/Ink：格式化统一为 `out.format`  
  - 状态：Done

- [Shared] UI/Ink：输入能力回收（raw/intent/encoder_decoder）  
  - 状态：Done

- [Shared] UI/Vivid：基础输出/日志对齐到 `out.*`  
  - 状态：Todo

### 音频主线

- [Shared] 音频重配/回归样例补齐  
  - 状态：In Progress

- [Shared] 声道变化退化策略（文档+最小代码）  
  - 状态：In Progress

### 整体整理

- [AI] 示例与杂项归档到 `Draft/` 的后续清点  
  - 状态：Todo

- [User] 核心模块最终保留清单（Charm 总架构 -> Charm-os 子项目）  
  - 状态：Todo

## 冲突记录（必要时填）

无。

