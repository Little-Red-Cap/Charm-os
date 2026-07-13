# System Evidence And Staging v0 归档

本目录保存 materialized graph 观察面、bring-up evidence、resource contract 和 ARMv7-A minimal-kernel staging 的完整阶段文档。

这些正文包含实现演进、状态语言、候选资源规则和路线取舍，保留用于追溯；但其中存在大量阶段叙事、未来排期和已被后续代码超越的判断。现行入口只保留可核对行为和明确状态。

归档条目：

- materialized graph tooling milestone 正文已删除；其中可复用的边界只有三项：`sample/v2` 未冻结、
  case 覆盖有限、尚未覆盖输入语义树。导出、diff、report、CI 的阶段清单和“下一阶段”叙事不再保留。
- materialized graph observe 完整正文已删除；旧 DTO 字段、DOT/JSON 样例、构建命令和演进路线由
  源码与 Git 历史追溯。现行边界仅承诺只读投影；DOT/JSON 不是稳定 ABI，也不能证明 runtime
  或硬件已运行。
- [`init_plan_retained_notes.md`](init_plan_retained_notes.md)：保留 Plan 不继承产出、Barrier 显式导出
  完成能力、Recipe/bound recipe/Node IR 分层与合并规则。
- [`bringup_evidence_retained_notes.md`](bringup_evidence_retained_notes.md)：保留状态解释、来源不可替代性、
  published/live 分离与静态/动态平面边界。
- [`resource_contract_retained_notes.md`](resource_contract_retained_notes.md)：保留 blocking、heap、reactor、
  clock 与 IRQ 候选维度及声明/事实/审计边界；当前没有统一运行时资源契约。
- [`armv7a_staging_retained_notes.md`](armv7a_staging_retained_notes.md)：保留 bare-metal 到 minimal-kernel
  的历史演进顺序、复用内核策略与证据纪律；不作为当前 roadmap。

默认入口：

- [`../../system/README.md`](../../system/README.md)
- [`../../system/init_materialized_graph_observe.md`](../../system/init_materialized_graph_observe.md)
- [`../../system/artifact_report_v0.md`](../../system/artifact_report_v0.md)
- [`../../system/armv7a_platform_contract.md`](../../system/armv7a_platform_contract.md)

归档中的 fact、producer、stage 或 roadmap 名称不能替代当前源码、构建和运行证据。

bring-up evidence 与 resource contract 完整阶段正文已删除；schema 字段、fixture catalog、命令矩阵、
compare 演进和 smoke 清单由现行 artifact report、脚本与 Git 历史追溯。

ARMv7-A 分阶段路线全文已压缩；旧目录树、接口候选、协作分工、runner 参数和“下一刀”叙事已删除。

init Recipe/Plan 全文已压缩；项目迁移路径、完成状态、模块清单和重复接入规则已删除。
