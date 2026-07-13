# System Evidence And Staging v0 归档

> status: `archived`
>
> 本目录保存 materialized graph、bring-up evidence、resource contract 与 ARMv7-A staging 的历史取舍。

当前行为从 [`system/README.md`](../../system/README.md)、
[`init_materialized_graph_observe.md`](../../system/init_materialized_graph_observe.md)、
[`artifact_report_v0.md`](../../system/artifact_report_v0.md) 和
[`armv7a_platform_contract.md`](../../system/armv7a_platform_contract.md) 进入。

## 保留内容

| 文件 | 独立价值 |
|---|---|
| [`init_plan_retained_notes.md`](init_plan_retained_notes.md) | Plan/Barrier、Recipe/bound recipe/Node IR 与 merge 边界 |
| [`bringup_evidence_retained_notes.md`](bringup_evidence_retained_notes.md) | 状态来源、published/live 与 static/dynamic plane |
| [`resource_contract_retained_notes.md`](resource_contract_retained_notes.md) | blocking、heap、reactor、clock、IRQ 及声明/事实/审计边界 |
| [`armv7a_staging_retained_notes.md`](armv7a_staging_retained_notes.md) | bare-metal 到 minimal-kernel 的历史顺序与证据纪律 |

## 已删除范围

旧 DTO/schema 展开、DOT/JSON 样例、fixture catalog、命令矩阵、阶段清单、迁移分工和 roadmap 已由
当前源码、artifact 工具与 Git 历史替代。归档名称和 producer/stage 术语不证明当前实现状态。
