# Compiler 文档入口

本目录收纳 Charm compile-time world 与 compiler constitution 相关文档。

这里不是新的 DSL 目录，也不是 codegen 目录。它的职责是先定义 Charm 的编译期世界法理：什么是 world、fact、pass authority、semantic freeze、lowering 与 witness。

## 当前入口

- [`charm_compiler_constitution_v0.md`](charm_compiler_constitution_v0.md)
- [`compiler_pass_authority_and_freeze_boundary_v0.md`](compiler_pass_authority_and_freeze_boundary_v0.md)
- [`compiler_world_lifecycle_v0.md`](compiler_world_lifecycle_v0.md)

## 阅读关系

建议先读：

1. [`../architecture/charm_methodology_charter.md`](../architecture/charm_methodology_charter.md)
2. [`charm_compiler_constitution_v0.md`](charm_compiler_constitution_v0.md)
3. [`compiler_pass_authority_and_freeze_boundary_v0.md`](compiler_pass_authority_and_freeze_boundary_v0.md)
4. [`compiler_world_lifecycle_v0.md`](compiler_world_lifecycle_v0.md)
5. [`../architecture/system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md)
6. [`../architecture/system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md)

再按需要进入：

- [`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- [`../system/resource_contract_v0.md`](../system/resource_contract_v0.md)
- [`../system/bringup_evidence_pipeline_v0.md`](../system/bringup_evidence_pipeline_v0.md)

## 非目标

- 不在这里定义 `WorldIR.hpp`、`TopologyIR.hpp` 或任何 C++ IR 类型。
- 不在这里新增 schema、validator、smoke 或 codegen pipeline。
- 不把 C++ template、YAML、JSON 或 Python generator 升格为唯一 truth source。
