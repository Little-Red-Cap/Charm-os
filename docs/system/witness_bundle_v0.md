# Witness Bundle v0

这份文档不是要替代：

- `artifact report`
- `runtime evidence bundle`
- `compare`

它要做的是把这些东西提升到同一个交付语言里。

## 一句话版本

- `artifact report` 是 case 级结论页。
- `runtime evidence bundle` 是某条 runtime 主线的总证词。
- `kernel runtime session` 是 host 语义与 QEMU 机器入口共同证明的 session 对象。
- `witness bundle` 则是把一个 canonical world 需要的 witness 收成同一个对象。

如果说 `artifact report` 更像单案卷宗，
那么 `witness bundle` 更像这次交付的整套作证材料。

## 为什么需要单独收一层

如果只有：

- binary
- 零散 report
- 零散 smoke log

那么系统即使能跑，也还停留在：

- 作者知道怎么解释
- 机器知道怎么跑
- 但交付物本身还不会“组织自己的证词”

`witness bundle` 的目标，是让交付物开始显式带着：

- 世界名
- 法律锚点
- witness 清单
- witness 状态
- 缺失点
- 汇总检查面

## 当前对象边界

当前 `witness bundle` 对应：

- schema：
  - `schemas/system_compiler.witness_bundle.v0.schema.json`
- sample：
  - `schemas/examples/system_compiler.witness_bundle.v0.sample.json`
- 导出脚本：
  - `scripts/export_system_compiler_witness_bundle.ps1`
- 校验脚本：
  - `scripts/validate_system_compiler_witness_bundle.py`

它当前可以组合四类输入：

- `canonical world`
- `artifact report`
- `minimal kernel runtime evidence bundle summary`
- `kernel runtime session summary`

并可附带：

- `kernel_runtime_session`
- `example_ref`

这意味着 v0 先不追求“世界里的所有证据都必须是结构化 summary”，
而是允许代表性样本目录先进入 witness 面。

## 当前输出语义

当前导出的 `witness bundle` 会稳定收这些对象：

- `world`
- `front_page`
- `artifact_context`
- `contract_status`
- `witness_summary`
- `witness_entries`
- `violations`

其中：

### `world`

回答：

- 这次交付的是哪个世界
- 它的核心问题是什么
- compare 时优先追问什么

### `artifact_context`

回答：

- 这份 bundle 从哪里导出
- artifact report root 的 first-read index 在哪里
- 用了哪些 artifact report
- 是否接了 runtime evidence summary

其中 `artifact_context.artifact_report_index` 是来源锚点：

- 当导出时传入 `ArtifactRoot` 且其中存在 `index.json`，脚本会把它记录为 `artifact_report_index`
- 该 index 必须是 `system_compiler.artifact_report_index/v0`
- 它负责让上层 proof / IDE / CI 先找到 artifact report root 的轻量入口
- 它不替代 `artifact_reports`，也不把 case 级结论复制进 witness bundle

### `front_page`

回答：

- 这份 witness bundle 自己的 machine-readable front page 路径是什么
- 如果上层 router 要继续追问，它应该先顺着哪些 supporting surfaces 进入 runtime evidence

对于 root `witness bundle` 来说，`front_page.supporting_surfaces` 允许被更上层 workflow 继续补强。

- 基础导出至少应能路由到 `runtime_evidence`
- 如果 runtime evidence summary 已经带有 `session` 或历史兼容的 `session_summary`，基础导出也应把它路由成 `kernel_runtime_session`
- 当同轮交付已经生成 `biography`、`world_compare`、`world_shelf_review` 时，wrapper 可以把这些上层 surface 一并挂回 root `front_page`

它不替代 `artifact_context`。

- `artifact_context` 更偏导出上下文与来源
- `front_page` 更偏“交付封面现在该先看谁”

### `contract_status`

回答：

- 这个世界宣称依赖的法律锚点是否都还在

也就是说，当前 bundle 至少会诚实地区分：

- contract 存在
- contract 丢失

### `witness_summary`

回答：

- witness 总数
- `ok / missing / fail`
- required witness 缺了多少
- 各 kind 分布

### `witness_entries`

回答：

- 每个 witness 是什么
- 它负责证明哪一层
- 当前状态如何
- 来源路径在哪
- 附带哪些最小观察行

这里要特别注意：

当前 `witness bundle` 的 `status`
表达的是“作证材料是否到场、是否通过自身结果面”，
而不是替代更细粒度的 runtime / formation / compare 语义。

例如：

- `artifact_report` witness 到场，`status` 可以是 `ok`
- 但它内部仍可能证明的是一个 `blocked` 系统

这是健康的。

因为 witness bundle 回答的是：

> 证词有没有、是否可引用

而不是粗暴地把所有下层语义再压平成一个全局 bool。

## 与现有对象的关系

### 1. 与 `artifact report`

`artifact report` 是单 case 的结论对象。

`witness bundle` 不重写它，
而是引用它、总结它在这个 world 里的角色。

### 1.1 与 `artifact report index`

`artifact report index` 是 artifact report root 的 first-read 入口。

`witness bundle` 不把它当成单独 witness entry，
而是在 `artifact_context.artifact_report_index` 中记录其路径。

这表示：

- `artifact_report_index` 负责回答“这批 artifact report 从哪里开始读”
- `artifact_reports` 负责回答“这份 witness bundle 实际拿哪些 case 作证”
- `witness_entries` 负责回答“每个 witness 在 canonical world 里承担什么角色”

三者各自分工，不互相替代。

### 2. 与 `minimal kernel runtime evidence bundle`

当前最小内核这条线已经有自己的 host + QEMU 总证据包。

`witness bundle` 不替代这条 bundle，
而是把它当成某个 canonical world 的一名正式 witness。

### 3. 与 `kernel runtime session`

`kernel_runtime_session` 是 runtime evidence bundle 内部收出的共同被证明对象。

`witness bundle` 不直接解析 host/QEMU 的散日志，
而是通过 runtime evidence summary 找到 session summary，
再把它作为 `kernel_runtime_session` witness entry 暴露给 world compare。

`witness bundle` 也可以把它作为独立 `kernel_runtime_session` entry 消费：

- 优先使用 canonical world witness plan 中显式声明的 `path`
- 如果 `path` 为空，则从 `runtime_evidence_summary.session.summary_path` 解析
- 只有当 session verdict 为 `standing` 且 failure count 为 0 时，entry 才视为 `ok`

同时，`kernel_runtime_session` 也会进入 `front_page.supporting_surfaces`。

- `witness_entries[kind=kernel_runtime_session]` 负责证明对象是否成立
- `front_page.supporting_surfaces[id=kernel_runtime_session]` 负责让上层 reader / IDE / proof workflow 能从交付封面直接追到 session summary、report 与 check

这表示 world compare 后续可以追问：

> 不是“QEMU 日志是否还像昨天”，而是“这个 kernel runtime session witness 是否还站住”。

### 4. 与 `compare`

当前 `witness bundle` 还不是最终的反事实审判器。

但它已经为后续 compare 提供了更健康的落点：

- compare 不是对散文件 diff
- compare 是对一个 world 的 witness 面做漂移追问

### 5. 与 `world compare`

当前 `world compare`

- 不直接绕过 witness bundle 去读散工件
- 而是把 baseline / candidate witness bundle 当成世界级 compare 的正式输入

也就是说：

- `witness bundle` 负责收证词
- `world compare` 负责比较证词并产出 `standing / improved / drifted / collapsed` verdict

## 当前推荐工作流

1. 先定义 canonical world。
2. 再把相关 `artifact report / runtime evidence bundle / kernel_runtime_session / example_ref` 填进 witness plan。
3. 再导出 `witness bundle`。
4. 如需比较世界漂移，再导出 `world compare`。
5. 最后把它和 binary、report、check 一起视作完整交付。

## 当前非目标

当前这层仍然不处理：

- 自动拉起构建
- 自动运行所有 smoke
- 全自动 compare 诊断
- 完整死亡法医报告

v0 的目标更克制：

> 先把“系统要拿什么作证自己为何成立”收成一个正式可交付对象。
