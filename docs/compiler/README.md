# Compiler 文档入口

本目录收纳 Charm compile-time world 与 compiler constitution 相关文档。

这里不是新的 DSL 目录，也不是 codegen 目录。它的职责是先定义 Charm 的编译期世界法理：什么是 world、fact、pass authority、semantic freeze、lowering 与 witness。

## 当前入口

- [`charm_compiler_constitution_v0.md`](charm_compiler_constitution_v0.md)
- [`compiler_pass_authority_and_freeze_boundary_v0.md`](compiler_pass_authority_and_freeze_boundary_v0.md)
- [`compiler_world_lifecycle_v0.md`](compiler_world_lifecycle_v0.md)
- [`compiler_world_lifecycle_projection_v0.md`](compiler_world_lifecycle_projection_v0.md)
- [`compiler_world_lifecycle_projection_coverage_v0.md`](compiler_world_lifecycle_projection_coverage_v0.md)
- [`compiler_sidecar_landing_order_v0.md`](compiler_sidecar_landing_order_v0.md)
- [`compiler_lifecycle_summary_sidecar_contract_v0.md`](compiler_lifecycle_summary_sidecar_contract_v0.md)
- [`compiler_lowering_surface_contract_v0.md`](compiler_lowering_surface_contract_v0.md)
- [`compiler_freeze_receipt_contract_v0.md`](compiler_freeze_receipt_contract_v0.md)
- [`compiler_archive_manifest_contract_v0.md`](compiler_archive_manifest_contract_v0.md)

## 当前实现入口

- `scripts/export_compiler_lifecycle_summary.py`
- `scripts/check_compiler_lifecycle_summary.ps1`
- `scripts/report_compiler_lifecycle_summary.ps1`
- `scripts/compiler_lifecycle_summary_sidecar.ps1`
- `scripts/compiler_lifecycle_summary_sidecar_smoke.ps1`
- `scripts/minimal_kernel_runtime_evidence_bundle.ps1 -ExportCompilerLifecycleSummary`

## 阅读关系

建议先读：

1. [`../architecture/charm_methodology_charter.md`](../architecture/charm_methodology_charter.md)
2. [`charm_compiler_constitution_v0.md`](charm_compiler_constitution_v0.md)
3. [`compiler_pass_authority_and_freeze_boundary_v0.md`](compiler_pass_authority_and_freeze_boundary_v0.md)
4. [`compiler_world_lifecycle_v0.md`](compiler_world_lifecycle_v0.md)
5. [`compiler_world_lifecycle_projection_v0.md`](compiler_world_lifecycle_projection_v0.md)
6. [`compiler_world_lifecycle_projection_coverage_v0.md`](compiler_world_lifecycle_projection_coverage_v0.md)
7. [`compiler_sidecar_landing_order_v0.md`](compiler_sidecar_landing_order_v0.md)
8. [`compiler_lifecycle_summary_sidecar_contract_v0.md`](compiler_lifecycle_summary_sidecar_contract_v0.md)
9. [`compiler_lowering_surface_contract_v0.md`](compiler_lowering_surface_contract_v0.md)
10. [`compiler_freeze_receipt_contract_v0.md`](compiler_freeze_receipt_contract_v0.md)
11. [`compiler_archive_manifest_contract_v0.md`](compiler_archive_manifest_contract_v0.md)
12. [`../architecture/system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md)
13. [`../architecture/system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md)

再按需要进入：

- [`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- [`../system/resource_contract_v0.md`](../system/resource_contract_v0.md)
- [`../system/bringup_evidence_pipeline_v0.md`](../system/bringup_evidence_pipeline_v0.md)

## 非目标

- 不在这里定义 `WorldIR.hpp`、`TopologyIR.hpp` 或任何 C++ IR 类型。
- 不在这里新增 schema、validator、smoke 或 codegen pipeline。
- 不把 C++ template、YAML、JSON 或 Python generator 升格为唯一 truth source。
