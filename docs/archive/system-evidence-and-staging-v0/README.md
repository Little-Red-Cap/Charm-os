# System Evidence And Staging v0 归档

本目录保存 materialized graph 观察面、bring-up evidence、resource contract 和 ARMv7-A minimal-kernel staging 的完整阶段文档。

这些正文包含实现演进、状态语言、候选资源规则和路线取舍，保留用于追溯；但其中存在大量阶段叙事、未来排期和已被后续代码超越的判断。现行入口只保留可核对行为和明确状态。

归档条目：

- materialized graph tooling milestone 正文已删除；其中可复用的边界只有三项：`sample/v2` 未冻结、
  case 覆盖有限、尚未覆盖输入语义树。导出、diff、report、CI 的阶段清单和“下一阶段”叙事不再保留。
- materialized graph observe 完整正文已删除；旧 DTO 字段、DOT/JSON 样例、构建命令和演进路线由
  源码与 Git 历史追溯。现行边界仅承诺只读投影；DOT/JSON 不是稳定 ABI，也不能证明 runtime
  或硬件已运行。
- [`init_plan_recipe_draft.md`](init_plan_recipe_draft.md)：保留 Plan 不继承产出、Barrier 显式导出完成能力、
  Recipe/bound recipe/Node 分层等早期设计取舍。正文中的 HQZY/Player 路径和迁移完成度已经过期。
- [`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md)：保留 evidence 状态语言、来源区分与
  报告演进讨论；现行工具边界见 [`../../system/artifact_report_v0.md`](../../system/artifact_report_v0.md)。
- [`resource_contract_v0.md`](resource_contract_v0.md)：保留 blocking、heap、reactor、clock 与 IRQ 资源维度
  的候选模型；当前只有 artifact report 投影，没有统一运行时资源契约。
- [`armv7a_minimal_kernel_staging_plan.md`](armv7a_minimal_kernel_staging_plan.md)：保留 minimal-kernel 的历史
  分阶段路线；现行 ARMv7-A 边界见 [`../../system/armv7a_platform_contract.md`](../../system/armv7a_platform_contract.md)。

默认入口：

- [`../../system/README.md`](../../system/README.md)
- [`../../system/init_materialized_graph_observe.md`](../../system/init_materialized_graph_observe.md)
- [`../../system/artifact_report_v0.md`](../../system/artifact_report_v0.md)
- [`../../system/armv7a_platform_contract.md`](../../system/armv7a_platform_contract.md)

归档中的 fact、producer、stage 或 roadmap 名称不能替代当前源码、构建和运行证据。
