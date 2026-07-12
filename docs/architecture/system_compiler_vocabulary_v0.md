# System Compiler 局部词汇 v0

## 文档状态

- `status`: `exploration`（停线冻结）
- `scope`: 现有 system compiler schema、脚本和报告中的局部词义
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](charm_core_contract.md) 约束

本文不是 Charm Core 词汇表，也不创建 DSL。字段的机器含义以对应 schema 和脚本为准。

## 使用规则

- 先写实际载体，再写探索名称。
- 不把 CMake 参数、report 字段或脚本对象提升为公共领域概念。
- 不把输入声明、静态结构、运行时观察和证据合并为同一种事实。
- 同名字段跨 schema 使用时，必须由 validator 或 smoke 证明含义一致。
- 词汇没有 producer、consumer 和失败行为时，不得新增 schema。

## 当前工具输入

| 词 | 当前含义 | 实际载体 |
|---|---|---|
| `Case` | 一次 export/report 的命名输入单位 | export case manifest、bundle entry |
| `case_kind` | case 可提供哪类 artifact | `materialized_graph`、`runtime_only`、`fact_only` |
| `Subject` | report 对目标 case 的身份投影 | case、profile、board、facets |
| `Profile` | exporter 接收并记录的项目级标签 | `-Profile`、bundle subject |
| `Board` | exporter 接收并记录的板级标签 | `-Board`、bundle subject |
| `Facet` | exporter 接收并记录的活动面标签 | `-Facet`、bundle subject |
| declared fact | producer 声明的事实 | bundle case、fact evidence |
| required fact | 当前投影要求存在的事实 | bundle case、resource contract |
| audit-provided fact | 审计输入声称可提供的事实 | bundle case、fact evidence |
| declared contract | case 输入的资源约束声明 | bundle case entry |

`Profile`、`Board` 和 `Facet` 在这里仅是 report 输入字段。它们不自动等于 Product Profile、
BSP、Capability Requirement 或 Core Binding。

## 当前工具输出

| 词 | 当前含义 | 权威载体 |
|---|---|---|
| `ArtifactReport` | 单 case 的只读结果聚合 | artifact report schema |
| `ArtifactReportIndex` | report root 的 case 与 headline 索引 | artifact report index schema |
| `SystemInput` | subject、声明输入和解析来源的投影 | `system_input` |
| `Structure` | capability、fact 和 graph 摘要 | `structure` |
| `BindingResult` | binding 条目与 unresolved 摘要 | `binding_result` |
| `BringupOrder` | report 中的顺序投影 | `bringup_order` |
| `SystemFormation` | 当前规则计算的形成状态与 blocker | `system_formation` |
| `BringupEvidence` | producer 提供的 bring-up 证据摘要 | `bringup_evidence` |
| `ResourceContract` | 声明契约及审计计数 | `resource_contract` |
| `FactResolution` | fact inventory 与 required fact 解析 | `fact_resolution` |
| `RuntimeObserve` | observed capability、状态和 transition 快照 | `runtime_observe` |
| `Comparison` | baseline/candidate 已投影字段的差异 | `comparison` |
| `ExplainSurface` | inspector 对 report 的只读查询 | inspect script |

这些输出只表达生成脚本看到的输入。`SystemFormation=ready` 等结论不能替代真实构建、运行或
板级验证。

## 探索词

以下词曾用于讨论输入语言，但没有获得稳定公共身份：

| 词 | 保留的问题 | 当前裁决 |
|---|---|---|
| `SystemSpec` | 如何表达一次系统级目标 | 仅 exploration；当前以 Case/Subject 投影 |
| `BoardPackage` | 如何携带板级事实及来源 | 仅 exploration；不能等同 `BoardCaps` |
| `World` | 是否需要跨 pass 保存语义状态 | compiler 局部历史词，不是 Core |
| `Witness` | 如何关联主张与证据 | 可作为 evidence 工具用语，不是 Core 原语 |
| `Lifecycle` | compiler artifact 的阶段投影 | 只在 lifecycle sidecar 范围使用 |
| `OpeningJudgmentCorridor` | 如何从结果导航到解释 | archived，不进入新接口 |

`Capability Contract`、`Requirement`、`Provision` 和 `Binding` 的正式含义只由 Core Contract
定义。Compiler 可以消费或投影它们，不得重定义。

## 不要混用

- `Case` 不等于 Application 或完整 `SystemSpec`。
- `Profile` 标签不等于 Capability Contract。
- `Board` 标签不等于 BSP 内容或已验证硬件事实。
- declared fact 不等于 observed fact。
- `materialized_graph` 不等于 runtime topology。
- `BringupOrder` 不等于启动成功证据。
- report `BindingResult` 不等于 Core Binding 本身。
- `ExplainSurface` 不创造新事实。

## 代码与文档入口

- schema：[`../../schemas/README.md`](../../schemas/README.md)
- artifact report：[`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- inspector：[`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
- 路线边界：[`system_compiler_roadmap.md`](system_compiler_roadmap.md)
- 历史摘要：
  [`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)
