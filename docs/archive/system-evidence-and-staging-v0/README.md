# System Evidence And Staging v0 归档

本目录保存 materialized graph 观察面、bring-up evidence、resource contract 和 ARMv7-A minimal-kernel staging 的完整阶段文档。

这些正文包含实现演进、状态语言、候选资源规则和路线取舍，保留用于追溯；但其中存在大量阶段叙事、未来排期和已被后续代码超越的判断。现行入口只保留可核对行为和明确状态。

归档条目：

- [`init_materialized_graph_tooling_milestone.md`](init_materialized_graph_tooling_milestone.md)：保留早期
  materialized graph 导出、diff、report、CI 工具链的阶段清单，以及 `sample/v2` 未冻结、case 覆盖有限、
  尚未覆盖输入语义树三项边界。正文中的“完整闭环”和未来工具愿景只作历史记录。
- [`init_plan_recipe_draft.md`](init_plan_recipe_draft.md)：保留 Plan 不继承产出、Barrier 显式导出完成能力、
  Recipe/bound recipe/Node 分层等早期设计取舍。正文中的 HQZY/Player 路径和迁移完成度已经过期。

默认入口：

- [`../../system/README.md`](../../system/README.md)
- [`../../system/init_materialized_graph_observe.md`](../../system/init_materialized_graph_observe.md)
- [`../../system/bringup_evidence_pipeline_v0.md`](../../system/bringup_evidence_pipeline_v0.md)
- [`../../system/resource_contract_v0.md`](../../system/resource_contract_v0.md)
- [`../../system/armv7a_minimal_kernel_staging_plan.md`](../../system/armv7a_minimal_kernel_staging_plan.md)

归档中的 fact、producer、stage 或 roadmap 名称不能替代当前源码、构建和运行证据。
