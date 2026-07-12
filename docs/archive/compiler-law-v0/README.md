# Compiler Law v0 历史摘要

## 文档状态

- `status`: `archived`
- `scope`: 已删除 compiler world/freeze/lowering 文档中的独立讨论
- `current entry`: [`../../compiler/README.md`](../../compiler/README.md)

原目录曾把一套未实现的 compiler 理论拆成 constitution、pass authority、world lifecycle、
projection、coverage、sidecar order、World IR、lowering、freeze receipt 和 archive manifest
等多份 contract。它们大多明确声明不实现 schema、validator、IR 或工具，因此不再作为现行
contract 保留。

## 保留的设计问题

- Compiler pass 可以读取、增加或修改哪些语义事实；哪些修改应创建新分支。
- “冻结”若有价值，应由显式 receipt 证明，不能由报告自行宣告。
- Lossy lowering 应保留来源、损失说明和 paired artifact，不能冒充完整输入世界。
- Observation 若要反向影响语义输入，应成为新的 candidate fact 并重新验证。
- Archive 若要证明可复盘，应明确保存对象、身份、来源和 compare 基线。
- 一份 lifecycle summary 只能说明现有 surfaces 覆盖了哪些阶段，不能补齐缺失阶段。

这些是可复用的评审问题，不是 Charm Core 术语或当前实现事实。

## 未实施提案

- canonical World IR 和 pass runner；
- semantic freeze receipt；
- lowering surface manifest；
- compiler archive manifest；
- observation import pass；
- LLVM/MLIR pipeline；
- 由 static reflection 自动生成项目 residue。

## 被收敛的文件

- `charm_compiler_constitution_v0.md`
- `compiler_pass_authority_and_freeze_boundary_v0.md`
- `compiler_world_lifecycle_v0.md`
- `compiler_world_lifecycle_projection_v0.md`
- `compiler_world_lifecycle_projection_coverage_v0.md`
- `compiler_sidecar_landing_order_v0.md`
- `compiler_world_ir_pipeline_contract_v0.md`
- `compiler_world_ir_first_landing_slice_v0.md`
- `compiler_hosted_reflection_extraction_surface_v0.md`
- `compiler_lowering_surface_contract_v0.md`
- `compiler_freeze_receipt_contract_v0.md`
- `compiler_archive_manifest_contract_v0.md`

低价值重复由 Git 历史保留；上述独立问题已在本页提炼。
