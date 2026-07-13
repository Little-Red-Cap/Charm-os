# System Compiler 局部词汇 v0

> `status`: `exploration`（停线冻结）

本文只约束现有 system compiler schema、脚本和报告中的局部用语，不创建 Charm Core 词汇或 DSL。
字段的机器含义以对应 schema、producer 和 validator 为准；正式 Capability Contract 术语只由
[`charm_core_contract.md`](charm_core_contract.md) 定义。

## 工具边界

- `Case`、`case_kind`、`Subject`、`Profile`、`Board` 和 `Facet` 是 exporter 的输入与分类标签，
  不是 Application、Product Profile、BSP 或 Capability Requirement。
- declared、required 和 audit-provided fact 是输入声明；它们不等于 observed fact，也不自动证明硬件事实。
- `ArtifactReport`、`ArtifactReportIndex`、`SystemInput`、`Structure`、`BindingResult`、
  `BringupOrder`、`SystemFormation`、`BringupEvidence`、`ResourceContract`、`FactResolution`、
  `RuntimeObserve` 和 `Comparison` 都是工具投影，不是 Core 对象或运行事实。
- `ExplainSurface` 只查询已有 report，不创造事实或重新判定上游 verdict。
- `materialized_graph` 不等于 runtime topology；report 中的 binding、order 或 ready 状态不能替代
  Core Binding、启动成功或板级证据。

## 历史词

`SystemSpec`、`BoardPackage`、`World`、`Witness`、`Lifecycle` 和 `OpeningJudgmentCorridor` 只保留为
历史探索语言。除非出现真实 producer、consumer、失败语义和 smoke，它们不得进入公共接口或新增 schema。

## 入口

- schema：[`../../schemas/README.md`](../../schemas/README.md)
- artifact report：[`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- inspector：[`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
- 路线边界：[`system_compiler_roadmap.md`](system_compiler_roadmap.md)
- 历史摘要：[`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)
