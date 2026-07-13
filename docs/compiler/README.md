# Compiler 探索入口

## 文档状态

- `status`: `exploration`
- `scope`: 当前有实现支撑的 compiler 工具实验与历史讨论入口
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Compiler 是 `Implementation / Tool`，不定义 Charm Core。当前目录只保留两条可运行实验。

## 当前实现

| 实验 | 入口 | 证据边界 |
|---|---|---|
| lifecycle summary sidecar | [`sidecar contract`](compiler_lifecycle_summary_sidecar_contract_v0.md) | 只读投影已有 artifact/evidence，不创建 freeze、proof 或 archive 事实 |
| static reflection compile probe | [`probe boundary`](compiler_static_reflection_three_stage_prototype_v0.md) | 只证明 hosted `<meta>` 与手写 freestanding residue 可分别编译，不存在 extractor |

具体 exporter、gate、runner 和参数由入口文档与 `scripts/` 维护，本页不复制。

## 历史讨论

World、freeze、lowering、archive manifest 和 World IR 等未实施讨论位于
[`compiler-law-v0`](../archive/compiler-law-v0/README.md)，只用于设计追溯，不是现行承诺。

## 相关工具

- artifact report：[`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- inspector：[`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
- exploration roadmap：
  [`../architecture/system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md)
- local vocabulary：
  [`../architecture/system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md)
