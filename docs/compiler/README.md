# Compiler 探索入口

## 文档状态

- `status`: `exploration`
- `scope`: 当前有实现支撑的 compiler 工具实验与历史讨论入口
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Compiler 是 `Implementation / Tool`，不定义 Charm Core。当前目录只保留两条可运行实验。

## 当前实现

### Lifecycle summary sidecar

- contract:
  [`compiler_lifecycle_summary_sidecar_contract_v0.md`](compiler_lifecycle_summary_sidecar_contract_v0.md)
- exporter: `scripts/export_compiler_lifecycle_summary.py`
- gate/report/wrapper: `scripts/check_*`、`report_*`、`compiler_lifecycle_summary_sidecar.ps1`
- smoke: `scripts/compiler_lifecycle_summary_sidecar_smoke.ps1`

它把现有 artifact/report/ledger/witness/compare 结果只读投影为九个历史 lifecycle 标签。
它不创建 freeze、proof 或 archive 事实。

### Static reflection compile probe

- boundary:
  [`compiler_static_reflection_three_stage_prototype_v0.md`](compiler_static_reflection_three_stage_prototype_v0.md)
- probe: `scripts/compiler_static_reflection_three_stage_probe.ps1`

它只证明 hosted `<meta>` TU 和 freestanding residue consumer 可以分别编译。当前没有 extractor，
probe 中的 residue 是手写 fixture。

## 历史讨论

World、pass authority、freeze、lowering、archive manifest 和 World IR 等未实施讨论已压缩到：

- [`../archive/compiler-law-v0/README.md`](../archive/compiler-law-v0/README.md)

这些讨论可用于设计评审，但不是现行 contract、schema 或实现承诺。

## 相关工具

- artifact report：[`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- inspector：[`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
- exploration roadmap：
  [`../architecture/system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md)
- local vocabulary：
  [`../architecture/system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md)
